#pragma once

#include "typedefs.h"

constexpr size_t CacheLineSize = std::hardware_constructive_interference_size;

struct alignas(CacheLineSize) MappedFileHeader {
    uint32_t version = 1;
    std::atomic<uint64_t> size = 0;
    std::atomic<uint64_t> capacity = 0;
    char pad1[CacheLineSize - sizeof(uint32_t) - sizeof(std::atomic<uint64_t>) * 2];

    std::atomic<uint32_t> readers = 0;
};

struct EmptyIndexEntry {};

template<typename T>
struct MapppeFileIndexTraits {
    using IndexType = EmptyIndexEntry;
    using IndexKeyType = int32_t;
    static constexpr bool enabled = false;
};