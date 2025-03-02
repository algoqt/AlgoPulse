#pragma once

#include<atomic>
#include<chrono>

enum class IdType {
    RequestID,
    AlgoOrderID,
    OrderID,
    TradeID,
    MessageSequenceID,
    SignalID
};

template<IdType idtype>
class IdGenerator {
public:


    static IdGenerator& getInstance() {
        static IdGenerator generator{};
        return generator;
    }

    inline uint64_t NewId() {

        constexpr static int numBitOfMilliSecond = 41; // 1<<41 ~  UTC Time: 2039-09-07 15:47:35.552000  ; 1<<42 UTC Time: 2109-05-15 07:35:11.104000 
        constexpr static int numBitOfIdType  = 6;  // 0~63  6bit
        constexpr static int numBitOfBussise = 64 - numBitOfIdType;  // 58
        constexpr static int numBitRemain    = 64 - numBitOfIdType - numBitOfMilliSecond;  // 17  每毫秒 2^numBitRemain 个不同Id 2^16=65536 2^17=131072

        uint64_t uid =      m_uid.fetch_add(1);
        auto t = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
        uint64_t now_time = t.count();
        //std::cout << now_time << std::endl;
        //std::cout << ((uint64_t)idtype << 58) << std::endl;
        //std::cout << ((uint64_t)now_time << 16) << std::endl;
        //std::cout << (uid & 0xFFFF) << std::endl;
        uint64_t id = ((uint64_t)idtype << numBitOfBussise ) + ((uint64_t)now_time << numBitRemain) + (uid & 0xFFFF);

        return id;
    }

private:
    IdGenerator()        { m_uid.store(0); }
    std::atomic<uint64_t> m_uid;
};


using AlgoOrderIdGenerator  = IdGenerator<IdType::AlgoOrderID>;
using OrderIdGenerator      = IdGenerator<IdType::OrderID>;
using RequestIdGenerator    = IdGenerator<IdType::RequestID>;
using MessageIdGenerator    = IdGenerator<IdType::MessageSequenceID>;
using TradeIdGenerator      = IdGenerator<IdType::TradeID>;
using SignalIdGenerator     = IdGenerator<IdType::SignalID>;
