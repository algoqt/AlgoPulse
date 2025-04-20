#pragma once

#include <map>
#include "MappedFileWriter.h"
#include "MappedFileReader.h"


template<class T>
class MappedFileManager {
    using MappedFileWriter4T  = MappedFileWriter<T>;
    using MappedFileReader4T  = MappedFileReader<T>;

public:
    static std::shared_ptr<MappedFileWriter4T> createWriter(const std::string& fileFullName
        ,size_t initialSize = 1024 * 1024
        ,size_t asyncQueueSize = 1024 * 16) {
        std::scoped_lock<std::mutex> lock(m_mutex);

        auto [it, inserted] = mappedFileWriters.try_emplace(fileFullName, std::weak_ptr<MappedFileWriter4T>());

        if (auto instancePtr = it->second.lock()) {  // `weak_ptr`
            return instancePtr;
        }

        auto instancePtr = std::shared_ptr<MappedFileWriter4T>(
            new MappedFileWriter4T(fileFullName, initialSize,asyncQueueSize),
            [fileFullName](MappedFileWriter4T* ptr) {
                std::scoped_lock<std::mutex> lock(m_mutex);

                ptr->stop();

                if (ptr->hasNoReaders()) {
                    ptr->shrinkFile();
                }

                mappedFileWriters.erase(fileFullName);

                delete ptr;               
            }
        );

        instancePtr->init();
        it->second = instancePtr;
        return instancePtr;
    }

    static std::shared_ptr<MappedFileReader4T> createReader(const std::string& fileFullName) {

        auto readPtr = std::make_shared<MappedFileReader4T>(fileFullName);

        return readPtr;
    }

private:
    inline static std::mutex m_mutex;

    inline static std::map<std::string, std::weak_ptr<MappedFileWriter4T>> mappedFileWriters;
};