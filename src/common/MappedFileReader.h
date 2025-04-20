#pragma once
#include <fstream> 
#include <immintrin.h>
#include "MappedFileWriter.h"


template<class T>
class MappedFileReader {
public:

    explicit MappedFileReader(const std::string& filePath):m_fileFullName(filePath){
        if (std::filesystem::exists(m_fileFullName)) {
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

    size_t recordSize() const {
        return m_header ? m_header->size.load(std::memory_order_acquire) : 0;
    }
    size_t processSize() const {
        return m_processItemSize;
    }
    const T& getRecord(size_t index) const {
        if (index >= recordSize()) {
            throw std::out_of_range("Record index out of range");
        }
        return m_data[index];
    }
    const T* getRecordPtr(size_t index) const{
        if (index >= recordSize()) {
            return nullptr;
        }
        return m_data + index ;
    }

    void stop() {
        m_running = false;
    }

    size_t realTimeRead(const std::function<void(const T&)>& func, int32_t checkFileDuration = 1000) {

        m_running = true;

        while (not std::filesystem::exists(m_fileFullName)) {
            if (checkFileDuration > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(checkFileDuration));
            }
            else {
                SPDLOG_ERROR("{} not exits!", m_fileFullName);
                return 0;
            }
        }

        if (not m_header) {
            remap_file();
            m_header->readers.fetch_add(1);
        }

        SPDLOG_INFO("init record count:{},capacity:{},readers:{}", m_header->size.load(), m_lastCapacity, m_header->readers.load());

        m_processItemSize = 0;

        while (m_running) {

            uint64_t currentSize = m_header->size.load(std::memory_order_acquire);

            if (currentSize > m_lastCapacity) {
                auto newCapacity = m_header->capacity.load(std::memory_order_acquire);
                if (newCapacity != m_lastCapacity) {
                    SPDLOG_INFO("{} capacity:{} Change to {}", m_fileFullName, m_lastCapacity, newCapacity);
                    remap_file();
                    currentSize = m_header->size.load(std::memory_order_acquire);
                }
            }
            if (m_processItemSize < currentSize) {
                SPDLOG_DEBUG("read records:last :{},current:{}", m_processItemSize, currentSize);

                for (auto i = m_processItemSize; i < currentSize; i++) {
                    func(m_data[i]);
                }

                m_processItemSize = currentSize;
            }
            //else {
            //    _mm_pause();              //std::this_thread::yield();
            //}
        }
        return m_processItemSize;
    }

private:

    void remap_file() {

        if (not m_fileMapping) {
            m_fileMapping = std::make_unique<bip::file_mapping>(m_fileFullName.c_str(), bip::read_write);
        }
        m_mappedRegion.reset();

        m_header = nullptr;

        m_mappedRegion = std::make_unique<bip::mapped_region>(*m_fileMapping, bip::read_write);

        m_header = reinterpret_cast<MappedFileHeader*>(m_mappedRegion->get_address());

        m_data = reinterpret_cast<const T*>(reinterpret_cast<const char*>(m_header) + sizeof(MappedFileHeader));

        m_lastCapacity = m_header->capacity.load();

    }

    std::atomic<bool>                   m_running{ false };

    std::string                         m_fileFullName;

    std::unique_ptr<bip::file_mapping>  m_fileMapping;

    std::unique_ptr<bip::mapped_region> m_mappedRegion;

    MappedFileHeader*                   m_header = nullptr;
    const T*                            m_data = nullptr;

    size_t                              m_lastCapacity = 0;

    uint64_t               m_processItemSize{ 0 };

};