#pragma once

#include "MarketDepth.h"
#include "MapppeFileIndexTraits.h"

#pragma pack(push, 1) 
struct MarketDepthIndex {
    uint32_t    symbol;
    time_t      timestamp;
    uint32_t    offset;

    static MarketDepthIndex getIndex(const MarketDepth* md, uint32_t offset) {
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
struct MapppeFileIndexTraits<MarketDepth> {

    using IndexType = MarketDepthIndex;
    using IndexKeyType = uint32_t;
    static constexpr bool enabled = true;
};