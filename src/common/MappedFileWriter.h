#pragma once

#include <fstream> 
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include "ContextService.h"
#include <boost/filesystem.hpp>
#include "concurrentqueue.h"
#include "MapppeFileIndexTraits.h"

namespace bip = boost::interprocess;


template<typename T>
class MappedFileManager;

template<typename T>
class MappedFileWriter : public std::enable_shared_from_this<MappedFileWriter<T>> {

    friend class MappedFileManager<T>;

    using IndexEntryT    = typename MapppeFileIndexTraits<T>::IndexType;

    using IndexEntryKeyT = typename MapppeFileIndexTraits<T>::IndexKeyType;

    static constexpr bool useIndex = MapppeFileIndexTraits<T>::enabled;

public:

    void append(const T* mdPtr) {

        if (!m_running) return;

        // 1: fastest,just call function
        // _append(mdPtr);

        // 2: queue to sink
        static moodycamel::ProducerToken ptok(m_queue);
        m_queue.enqueue(ptok,boost::intrusive_ptr(mdPtr));
        
        // 3: asio post
        //asio::post(*m_runContext, [this, instrPtr = boost::intrusive_ptr(mdPtr)]() { _append(instrPtr.get()); });

    }

    size_t recordSize() const {
        return m_header ? m_header->size.load(std::memory_order_acquire) : 0;
    }
private:

    MappedFileWriter(const std::string& fileFullName, size_t initialCapacity = 1024 * 1024, size_t cacheQueueSize = 1024)
        : m_fileFullName(fileFullName)
        , m_currentCapacity(initialCapacity)
        , m_queue(cacheQueueSize) {
    }

    ~MappedFileWriter() {

        stop();

        if (m_mappedRegion) m_mappedRegion->flush();

        if (m_file.is_open()) m_file.close();

        m_mappedRegion.reset();
        m_fileMapping.reset();

        ContextService::getInstance().stopContext(typeid(T).name());

    }

    void createFile(size_t cap) {

        size_t fileSize = calculateFileSize(cap);

        m_file.open(m_fileFullName, std::ios::binary | std::ios::out);
        m_file.seekp(fileSize - 1);
        m_file.put('\0');
        m_file.seekp(0);

        MappedFileHeader header{ .size=0,.capacity = cap };

        m_file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        m_file.flush();

        SPDLOG_INFO("create File:{},capacity {},fileSize:{}", m_fileFullName, cap, fileSize);
    }

    bool init() {

        bool exists = std::filesystem::exists(m_fileFullName);

        if (!exists) {
            createFile(m_currentCapacity);
        }
        else {
            if (std::filesystem::file_size(m_fileFullName) < sizeof(MappedFileHeader)) {
                std::filesystem::remove(m_fileFullName);
                return init();
            }
            m_file.open(m_fileFullName, std::ios::in | std::ios::out);
        }

        m_fileMapping = std::make_unique<bip::file_mapping>(m_fileFullName.c_str(), bip::read_write );

        remapFile(0, m_currentCapacity,true);

        if constexpr (useIndex) {
            buildIndexMap(); 
        }

        m_currentSize  = m_header->size;
        m_currentCapacity     = m_header->capacity;

        SPDLOG_INFO("open File:{},size:{},capacity:{},fileSize:{}"
            , m_fileFullName
            , m_header->size.load()
            , m_header->capacity.load()
            , std::filesystem::file_size(m_fileFullName));

        m_runContext = ContextService::getInstance().createContext(typeid(T).name(), 1);

        m_running = true;

        asio::post(*m_runContext, [this]() { sink();  }); // dont't need this->shared_from_this(), see MappedFileManager::createWriter

        return true;
    }

    void remapFile(size_t oldCapacity,size_t newCapacity,bool isfirstMap = false) {

        //agcommon::TimeCost tc("remap file");

        auto new_mappedRegion = std::make_unique<bip::mapped_region>(*m_fileMapping, bip::read_write);

        auto base = static_cast<char*>(new_mappedRegion->get_address());

        m_header = reinterpret_cast<MappedFileHeader*>(base);

        m_data = reinterpret_cast<T*>(base + sizeof(MappedFileHeader));

        if constexpr (useIndex) {

            newCapacity = isfirstMap ? m_header->capacity.load() : newCapacity;
            
            m_index = reinterpret_cast<IndexEntryT*>(base + sizeof(MappedFileHeader) + sizeof(T) * newCapacity);

            if (oldCapacity > 0) {
                //agcommon::TimeCost tc("move index");

                void* oldIndexStart = reinterpret_cast<IndexEntryT*>(reinterpret_cast<char*>(m_data) + sizeof(T) * oldCapacity);
                size_t oldIndexSize = sizeof(IndexEntryT) * oldCapacity;

                std::memcpy(m_index, oldIndexStart, oldIndexSize);
            }
        }
        m_mappedRegion.swap(new_mappedRegion);
    }
    
    void growFile() {
        
        // grow file may cost seconds for xG data, better pregrow async or pre reserve enough capacity 

        auto startTime = std::chrono::steady_clock::now();
        size_t oldCapacity = m_currentCapacity;
        size_t newCapacity = oldCapacity * 2;
        size_t newFileSize = calculateFileSize(newCapacity);

        {
            //agcommon::TimeCost tc("seekp file");
            m_file.seekp(0, std::ios::end);
            m_file.seekp(newFileSize - 1);
            m_file.put('\0');
            m_file.flush();
            SPDLOG_INFO("{} grow to fileSize:{}", m_fileFullName,std::filesystem::file_size(m_fileFullName));
        }
        
        remapFile(oldCapacity,newCapacity);

        m_currentCapacity = newCapacity;
        m_header->capacity.store(m_currentCapacity, std::memory_order_release);
        
        auto costTime = (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-startTime)).count();
        SPDLOG_INFO("grow File:{},capacity from:{} to:{},newFileSize:{},costTime:{}ms", m_fileFullName, oldCapacity, newCapacity, newFileSize, costTime);

    }

    void shrinkFile() {

        if (!m_header || m_running) return;
        if (not hasNoReaders()) return;

        size_t newFileSize = calculateFileSize(m_header->size);

        size_t oldFileSize = std::filesystem::file_size(m_fileFullName);

        if (m_header->size.load() < m_header->capacity.load()) {
            m_header->capacity.store(m_header->size,std::memory_order_release);

            if constexpr (useIndex) {
                char* newIndexAddr = reinterpret_cast<char*>(m_data) + m_header->size * sizeof(T);
                std::memcpy(newIndexAddr, m_index, m_header->size * sizeof(IndexEntryT));
            }

            SPDLOG_INFO("try resize file:{},from size:{},capcity:{},oldFileSize:{} to newFileSize:{}", m_fileFullName
                , m_header->size.load()
                , m_header->capacity.load()
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

    void buildIndexMap() {

        m_indexKeyPositionMap.clear();
        m_indexKeyPositionMap.reserve(m_header->size);
        for (size_t i = 0; i < m_header->size; ++i) {
            IndexEntryKeyT indexKey = IndexEntryT::getIndexKey_1(m_index[i]);
            m_indexKeyPositionMap[indexKey].push_back(i);
        }
    }

    size_t calculateFileSize(size_t cap) {
        return sizeof(MappedFileHeader) + sizeof(T) * cap + (useIndex ? sizeof(IndexEntryT) * cap : 0);
    }

    inline void stop() { m_running.store(false); }

    inline bool hasNoReaders() { return m_header && m_header->readers.load() == 0; }

    inline std::string getFilePath() { return m_fileFullName; }

    void sink() {
        constexpr int BatchSize = 128;

        static moodycamel::ConsumerToken ctok(m_queue);

        std::vector<boost::intrusive_ptr<const T>> ptrBuffer(BatchSize);

        boost::intrusive_ptr<const T> instrusivPtr;

        while (m_running.load()) {
            // batch mode
            //_appendBatch(ctok, ptrBuffer, BatchSize);

            // single mode
            if (m_queue.try_dequeue(ctok,instrusivPtr)) {
                _append(instrusivPtr.get());
            }
        }

        SPDLOG_INFO("Mapped file sink stopped");
    }

    void _appendBatch(moodycamel::ConsumerToken& ctok,std::vector<boost::intrusive_ptr<const T>>& buffer,size_t batchSize) {

        size_t count = m_queue.try_dequeue_bulk(ctok, buffer.begin(), batchSize);

        size_t currentItemSize = m_currentSize;

        for (size_t i = 0; i < count; i++) {

            auto rawPtr = buffer[i].get();

            if (currentItemSize >= m_currentCapacity) {
                growFile();
            }

            memcpy(&m_data[currentItemSize], rawPtr, sizeof(T));

            if constexpr (useIndex) {
                auto entry = IndexEntryT::getIndex(rawPtr, currentItemSize);
                m_index[i] = entry;

                IndexEntryKeyT indexKey = IndexEntryT::getIndexKey_1(entry);
                m_indexKeyPositionMap[indexKey].push_back(currentItemSize);
            }

            currentItemSize++;

        }

        m_currentSize = m_currentSize + count;

        m_header->size.store(m_currentSize, std::memory_order_release);
    }

    void _append(const T* ptr) {

        if (m_currentSize >= m_currentCapacity) {
            growFile();
        }

        memcpy(&m_data[m_currentSize], ptr, sizeof(T));

        if constexpr (useIndex) {
            auto entry = IndexEntryT::getIndex(ptr, m_currentSize);
            m_index[m_currentSize] = entry;

            IndexEntryKeyT indexKey = IndexEntryT::getIndexKey_1(entry);
            m_indexKeyPositionMap[indexKey].push_back(m_currentSize);
        }

        m_currentSize++;
        m_header->size.store(m_currentSize, std::memory_order_release);
    }

private:

    std::string         m_fileFullName;

    size_t              m_currentCapacity;

    size_t              m_currentSize = 0;

    std::atomic<bool>   m_running    = false;

    std::fstream        m_file{};

    std::unique_ptr<bip::file_mapping>  m_fileMapping;
    std::unique_ptr<bip::mapped_region> m_mappedRegion;

    MappedFileHeader*   m_header = nullptr;
    T*                  m_data   = nullptr;
    IndexEntryT*        m_index  = nullptr;

    AsioContextPtr     m_runContext = nullptr;

    std::unordered_map<IndexEntryKeyT, std::vector<size_t>> m_indexKeyPositionMap;

    moodycamel::ConcurrentQueue<boost::intrusive_ptr<const T>> m_queue;
};
