#pragma once

#include <fstream> 
#include "MarketDepth.h"
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include "ContextService.h"
#include <boost/filesystem.hpp>

namespace bip = boost::interprocess;

#pragma pack(push, 1) // 确保紧密内存布局
struct MappedFileHeader {
    uint32_t version = 1;
    uint64_t size = 0;
    uint64_t capacity = 0;
    std::atomic<uint32_t>   readers = 0;
};

#pragma pack(pop)

struct EmptyIndexEntry {};

template<typename T>
struct IndexTraits {
    using IndexType = EmptyIndexEntry;
    static constexpr bool enabled = false;
};

template<typename T>
class MappedFileManager;

template<typename T>
class MappedFileWriter : public std::enable_shared_from_this<MappedFileWriter<T>> {

    friend class MappedFileManager<T>;

    using IndexEntryT = typename IndexTraits<T>::IndexType;

    static constexpr bool kUseIndex = IndexTraits<T>::enabled;

public:
    void append(const T* md) {

        if (!m_running) return;
        md->retainAlive();

        auto task = [this, self = this->shared_from_this(), md]() {
            if (!m_running) return;

            if (m_header->size >= m_header->capacity) {
                growFile();
            }

            size_t i = m_header->size;

            memcpy(&m_data[i], md, sizeof(T));

            if constexpr (kUseIndex) {
                auto entry = IndexEntryT::create(md, i * sizeof(T));
                m_index[i] = entry;

                uint32_t indexKey = IndexEntryT::getIndexKey_1(entry);
                m_indexKeyPositionMap[indexKey].push_back(i);
            }

            m_header->size++;
            md->release();
            };

        asio::post(*m_contextPtr, task);
    }

private:

    MappedFileWriter(const std::string& fileFullName, size_t initialCapacity = 1024 * 1024)
        : m_fileFullName(fileFullName)
        , m_initCapacity(initialCapacity) {

        m_contextPtr = ContextService::getInstance().createContext("MappedFileWriter", 1);
        open(true, true);

    }

    ~MappedFileWriter() {

        m_running = false;

        if (m_mappedRegion) m_mappedRegion->flush();

        if (m_file.is_open()) m_file.close();

        m_mappedRegion.reset();
        m_fileMapping.reset();
    }

    void createFile(size_t cap) {

        size_t fileSize = calculateFileSize(cap);

        m_file.open(m_fileFullName, std::ios::binary | std::ios::out);
        m_file.seekp(fileSize - 1);
        m_file.put('\0');
        m_file.seekp(0);

        MappedFileHeader header{ .capacity = cap };

        m_file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        m_file.close();

        SPDLOG_INFO("create File:{},capacity {},fileSize:{}", m_fileFullName, cap, fileSize);
    }

    bool open(bool writable = true, bool rebuildIndex = true) {

        bool exists = std::filesystem::exists(m_fileFullName);

        if (!exists && writable) {
            createFile(m_initCapacity);
        }

        m_fileMapping = std::make_unique<bip::file_mapping>(m_fileFullName.c_str(), writable ? bip::read_write : bip::read_only);
        m_mappedRegion = std::make_unique<bip::mapped_region>(*m_fileMapping, writable ? bip::read_write : bip::read_only);

        auto base = static_cast<char*>(m_mappedRegion->get_address());

        m_header = reinterpret_cast<MappedFileHeader*>(base);

        m_data = reinterpret_cast<T*>(base + sizeof(MappedFileHeader));

        if constexpr (kUseIndex) {
            m_index = reinterpret_cast<IndexEntryT*>(base + sizeof(MappedFileHeader) + sizeof(T) * m_header->capacity);
        }

        if (rebuildIndex && kUseIndex) rebuildIndexMap();

        SPDLOG_DEBUG("open File:{},size:{},capacity:{},fileSize:{}", m_fileFullName, m_header->size, m_header->capacity, std::filesystem::file_size(m_fileFullName));

        m_running = true;

        return true;
    }

    void growFile() {

        if (m_mappedRegion) m_mappedRegion->flush();

        size_t oldCapacity = m_header->capacity;
        size_t newCapacity = oldCapacity * 2;
        size_t newFileSize = calculateFileSize(newCapacity);

        m_mappedRegion.reset();
        m_fileMapping.reset();

        m_file.open(m_fileFullName, std::ios::binary | std::ios::in | std::ios::out);
        m_file.seekp(newFileSize - 1);
        m_file.put('\0');
        m_file.close();

        open(true, false);

        if constexpr (kUseIndex) {

            void* oldIndexStart = reinterpret_cast<char*>(m_index);
            size_t oldIndexSize = sizeof(IndexEntryT) * oldCapacity;

            IndexEntryT* newIndex = reinterpret_cast<IndexEntryT*>(reinterpret_cast<char*>(m_header) + sizeof(MappedFileHeader) + sizeof(T) * newCapacity);
            std::memcpy(newIndex, oldIndexStart, oldIndexSize);
            m_index = newIndex;
        }

        m_header->capacity = newCapacity;

        SPDLOG_INFO("grow File:{},capacity from:{} to:{},newFileSize:{}", m_fileFullName, oldCapacity, newCapacity, newFileSize);
    }

    void shrinkFile() {

        if (!m_header || m_running) return;
        if (not hasNoReaders()) return;

        size_t newFileSize = calculateFileSize(m_header->size);

        size_t oldFileSize = std::filesystem::file_size(m_fileFullName);

        if (m_header->size < m_header->capacity) {
            m_header->capacity = m_header->size;

            if constexpr (kUseIndex) {
                char* newIndexAddr = reinterpret_cast<char*>(m_data) + m_header->size * sizeof(T);
                std::memcpy(newIndexAddr, m_index, m_header->size * sizeof(IndexEntryT));
            }

            SPDLOG_INFO("try resize file:{},from size:{},capcity:{},oldFileSize:{} to newFileSize:{}", m_fileFullName
                , m_header->size
                , m_header->capacity
                , oldFileSize
                , newFileSize);

            m_mappedRegion.reset();
            m_fileMapping.reset();

            std::error_code ec;
            std::filesystem::resize_file(m_fileFullName, newFileSize, ec);
            if (ec) {
                SPDLOG_ERROR("shrinkFile error: {}", ec.message());
            }
            else {
                SPDLOG_INFO("resize file:{} done", m_fileFullName);
            }
        }
    }

    void rebuildIndexMap() {

        m_indexKeyPositionMap.clear();
        for (size_t i = 0; i < m_header->size; ++i) {
            uint32_t indexKey = IndexEntryT::getIndexKey_1(m_index[i]);
            m_indexKeyPositionMap[indexKey].push_back(i);
        }
    }

    size_t calculateFileSize(size_t cap) {
        return sizeof(MappedFileHeader) + sizeof(T) * cap + (kUseIndex ? sizeof(IndexEntryT) * cap : 0);
    }

    inline void stop() { m_running = false; }

    inline bool hasNoReaders() { return m_header && m_header->readers.load() == 0; }

    inline std::string getFilePath() { return m_fileFullName; }

private:

    std::string         m_fileFullName;

    size_t              m_initCapacity;

    AsioContextPtr      m_contextPtr = nullptr;

    std::atomic<bool>   m_running    = false;

    std::fstream        m_file{};

    std::unique_ptr<bip::file_mapping>  m_fileMapping;
    std::unique_ptr<bip::mapped_region> m_mappedRegion;

    MappedFileHeader*   m_header = nullptr;
    T*                  m_data   = nullptr;
    IndexEntryT*        m_index = nullptr;

    std::unordered_map<uint32_t, std::vector<size_t>> m_indexKeyPositionMap;
};

#pragma pack(push, 1) 
struct MarketDepthIndex {

    uint32_t    symbol;
    time_t      timestamp;
    uint64_t    offset;

    static MarketDepthIndex create(const MarketDepth* md, size_t offset) {
        return MarketDepthIndex{
            .symbol = md->symbolIntFormat(),
            .timestamp = agcommon::to_timestamp(md->quoteTime),
            .offset = offset
        };
    }
    static uint32_t getIndexKey_1(const MarketDepthIndex& entry) {
        return entry.symbol;
    }
};
#pragma pack(pop)

template<>
struct IndexTraits<MarketDepth> {

    using IndexType = MarketDepthIndex;
    static constexpr bool enabled = true;
};