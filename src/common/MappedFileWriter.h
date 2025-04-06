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
    uint64_t magicNumber = 0x4D445354; // "MDST" in hex
    uint32_t version = 1;
    uint64_t recordCount = 0;
    uint64_t fileSize = 0;
    time_t   createTime = 0;
    time_t   lastUpdateTime = 0;
    std::atomic<uint32_t>   readers = 0;
};
#pragma pack(pop)

template<class T>
class MappedFileWriter: public std::enable_shared_from_this<MappedFileWriter<T>> {

public:

    explicit MappedFileWriter(const std::string& filePath, size_t initialSize = 1024 * 1024)
        : m_filePath(filePath) {

        m_contextPtr = ContextService::getInstance().createContext("MappedFileWriter", 1);

        m_file.open(filePath, std::ios::binary | std::ios::in | std::ios::out | std::ios::ate);

        if (not m_file.is_open()) {
            SPDLOG_INFO("Create map File:{}", filePath);
            m_file.open(filePath, std::ios::binary | std::ios::out);
            initializeNewFile(initialSize);
        }
        else {

            loadExistingFile();
        }

        mapFile();

        SPDLOG_INFO("map File:{},recorde count:{},fileSize:{},updateTime:{}", filePath, m_header->recordCount, m_header->fileSize, m_header->lastUpdateTime);
        
        m_running = true;
    }

    MappedFileWriter(const MappedFileWriter&) = delete;
    MappedFileWriter& operator=(const MappedFileWriter&) = delete;

    ~MappedFileWriter() {

        m_running = false;

        if (m_mappedRegion) {
            m_mappedRegion->flush();
        }

        if (m_file.is_open()) {
            m_file.close();
        }

        if(m_mappedRegion)
            m_mappedRegion.reset();
        if(m_fileMapping)
            m_fileMapping.reset();

    }

    void stop() {

        m_running = false;
    }

    bool hasNoReaders() {
        if (m_header) {
            return m_header->readers.load() == 0;
        }
        return false;
    }

    std::string getFilePath() {
        return m_filePath;
    }

    void appendMarketDepth(const T* depth) {

        if (not m_running) return;

        depth->retainAlive();

        auto task = [this, self=this->shared_from_this(), depth]() {

            if (not m_running) return;

            if ((m_header->recordCount + 1) * sizeof(T) + sizeof(MappedFileHeader) >= m_header->fileSize) {
                growFile();
            }

            T* record = reinterpret_cast<T*>(reinterpret_cast<char*>(m_header)
                + sizeof(MappedFileHeader)
                + m_header->recordCount * sizeof(T));

            memcpy(record, depth, sizeof(T));

            depth->release();

            m_header->recordCount++;
            m_header->lastUpdateTime = std::time(nullptr);
        };

        asio::post(*m_contextPtr, task);
    }

    void shrinkFile() {

        if (!m_header) return;
        if (m_running) return;

        size_t actualSize = sizeof(MappedFileHeader) + m_header->recordCount * sizeof(T);
        size_t fileSize = (size_t)std::filesystem::file_size(m_filePath);

        if (actualSize < fileSize) {

            SPDLOG_INFO("resizing file: {},from headerSize {}[{}] to actualSize {}", m_filePath, m_header->fileSize, fileSize, actualSize);

            m_mappedRegion.reset();
            m_fileMapping.reset();

            std::error_code ec;
            std::filesystem::resize_file(m_filePath, actualSize, ec);
            if (ec) {
                SPDLOG_ERROR("{}", ec.message());
            }
            else {

                mapFile();
                m_header->fileSize = actualSize;
            }
        }
    }

private:
    void initializeNewFile(size_t initialSize) {

        m_file.seekp(initialSize - 1);
        m_file.put('\0');
        m_file.seekp(0);

        MappedFileHeader header;

        header.createTime       = std::time(nullptr);
        header.lastUpdateTime   = header.createTime;
        header.fileSize         = initialSize;

        m_file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    }

    void loadExistingFile() {

        m_file.seekg(0, std::ios::end);

        std::size_t loadFileSize = (size_t)std::filesystem::file_size(m_filePath);

        m_file.seekg(0);

        MappedFileHeader header;
        m_file.read(reinterpret_cast<char*>(&header), sizeof(header));

        SPDLOG_INFO("load File:{},header fileSize:{},loadFileSize:{}", m_filePath, header.fileSize, loadFileSize);

        header.fileSize = loadFileSize;

        if (header.magicNumber != 0x4D445354) {
            throw std::runtime_error("Invalid market data file format");
        }
    }

    void mapFile() {

        m_file.close();

        m_fileMapping = std::make_unique<bip::file_mapping>(m_filePath.c_str(), bip::read_write);

        m_mappedRegion = std::make_unique<bip::mapped_region>(*m_fileMapping, bip::read_write);

        m_header = reinterpret_cast<MappedFileHeader*>(m_mappedRegion->get_address());
    }

    void growFile() {

        if (m_mappedRegion) {
            m_mappedRegion->flush();
        }
        auto lastSize = m_header->fileSize;
        size_t newSize = lastSize * 2;

        m_mappedRegion.reset();
        m_fileMapping.reset();

        m_file.open(m_filePath, std::ios::binary | std::ios::in | std::ios::out);
        m_file.seekp(newSize - 1);
        m_file.put('\0');

        mapFile();

        m_header->fileSize = newSize;
        SPDLOG_INFO("grow File:{} from:{},to:{}", m_filePath, lastSize, newSize);
    }

    std::string  m_filePath;

    std::fstream m_file;

    std::unique_ptr<bip::file_mapping> m_fileMapping;

    std::unique_ptr<bip::mapped_region> m_mappedRegion;

    MappedFileHeader*       m_header = nullptr;

    AsioContextPtr          m_contextPtr{ nullptr };

    std::atomic<bool>       m_running{false};

};
