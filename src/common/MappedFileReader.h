#pragma once
#include <fstream> 
#include "MappedFileWriter.h"
#include <immintrin.h>

template<class T>
class MappedFileReader {
public:

    explicit MappedFileReader(const std::string& filePath):m_filePath(filePath){
        if (std::filesystem::exists(m_filePath)) {
            remap_file();
            m_header->readers.fetch_add(1);
        }
    }

    ~MappedFileReader() {
        if (m_header) {
            m_header->readers.fetch_sub(1);
        }
        if (m_mappedRegion)
            m_mappedRegion.reset();
        if (m_fileMapping)
            m_fileMapping.reset();
    }

    size_t recordCount() const {
        return m_header ? m_header->recordCount : 0;
    }

    const T& getRecord(size_t index) const {
        if (index >= recordCount()) {
            throw std::out_of_range("Record index out of range");
        }
        return m_recordArray[index];
    }
    const T* getRecordPtr(size_t index) const{
        if (index >= recordCount()) {
            return nullptr;
        }
        return reinterpret_cast<const T*>(
            reinterpret_cast<char*>(m_header) + sizeof(MappedFileHeader) + index * sizeof(T));  // &m_recordArray[index];
    }

    void stop() {
        m_running = false;
    }

    void readRecord(const std::function<void(const T&)>& func, int32_t checkFileDuration = 1000) {

        m_running = true;
        uint64_t lastCount = 0;

        while (not std::filesystem::exists(m_filePath)) {
            if (checkFileDuration > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
            else {
                SPDLOG_ERROR("{} not exits!", m_filePath);
                return;
            }
        }

        if (not m_header) {
            remap_file();
            m_header->readers.fetch_add(1);
        }

        SPDLOG_INFO("init record count:{},fileSize:{},readers:{}", m_header->recordCount, m_lastFileSize, m_header->readers.load());

        while (m_running) {

            if (m_header->fileSize != m_lastFileSize) {
                remap_file();
            }

            uint64_t currentCount = m_header->recordCount;
            if (lastCount < currentCount) {
                SPDLOG_DEBUG("read records:last :{},current:{}", lastCount, currentCount);

                for (auto index = lastCount; index < currentCount; index++) {

                    if (func) {
                        func(m_recordArray[index]);
                    }
                }
                lastCount = currentCount;
            }
            _mm_pause();
            std::this_thread::yield();
            //std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }


    }

private:

    void remap_file() {

        m_mappedRegion.reset();
        m_fileMapping.reset();
        m_header = nullptr;

        m_fileMapping = std::make_unique<bip::file_mapping>(m_filePath.c_str(), bip::read_write);

        m_mappedRegion = std::make_unique<bip::mapped_region>(*m_fileMapping, bip::read_write);

        m_header = reinterpret_cast<MappedFileHeader*>(m_mappedRegion->get_address());

        if (m_header->magicNumber != 0x4D445354) {
            throw std::runtime_error("Invalid market data file format");
        }

        m_recordArray = reinterpret_cast<const T*>(
            reinterpret_cast<const char*>(m_header) + sizeof(MappedFileHeader));

        m_lastFileSize = m_header->fileSize;

    }

    std::atomic<bool>                   m_running{ false };

    std::string                         m_filePath;

    std::unique_ptr<bip::file_mapping>  m_fileMapping;

    std::unique_ptr<bip::mapped_region> m_mappedRegion;

    MappedFileHeader*                   m_header = nullptr;
    const T*                            m_recordArray = nullptr;

    size_t m_lastFileSize = 0;
};