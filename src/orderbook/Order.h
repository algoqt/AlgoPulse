#pragma once

#include "typedefs.h"
#include "common.h"
#include "KeepAlive.h"

using BrokerOrderNo_t = std::string;

constexpr auto ALGO_MIN_DATE_TIME = boost::posix_time::min_date_time;

struct Order :KeepAlivePool<Order> {

    OrderId_t orderId       {};

    OrderTime_t orderTime   { ALGO_MIN_DATE_TIME };

    uint64_t brokerId            {0};

    AlgoMsg::AcctType acctType    {};

    Acct_t      acct           {};

    Symbol_t    symbol         {};

    int         tradeSide      {0};

    int         status         { agcommon::OrderStatus::UNSENT };

    int         orderQty       { 0 };

    double      orderPrice     { 0.0 };

    OrderType_t orderType      { '0'};

    int         filledQty      { 0 };

    double      avgPrice       { 0 };

    double      filledAmt      { 0 };

    int         cancelQty      { 0 };

    uint64_t        pairId     { 0 };

    AlgoOrderId_t   algoOrderId{ 0 };

    OrderTime_t     cancelTime  { ALGO_MIN_DATE_TIME };

    int             cancelTimes { 0 };

    OrderTime_t     updTime     { ALGO_MIN_DATE_TIME };

    std::string     downStreamOrderId {};

    BrokerOrderNo_t brokerOrderNo     {};

    OrderTime_t     quoteTime   { ALGO_MIN_DATE_TIME };

    std::string     text        { };

public:
    Order() = default;

    Order(const Order& other);

    Order& operator=(const Order& other);

    inline bool isBuy() const{
        return agcommon::isBuy(tradeSide);
    }

    inline bool isFinalStatus() const{
        return agcommon::OrderStatus::isFinalStatus(status);
    }

    inline bool isNewerThan(const Order& preOrder) const {
        if (preOrder.isFinalStatus()) {
            return false;
        }
        if (isFinalStatus() 
            || filledQty > preOrder.filledQty 
            || (preOrder.status == agcommon::OrderStatus::REPORTING 
                || preOrder.status <= agcommon::OrderStatus::SENTOK)) {
            return true;
        }
        return false;
    }

    inline bool isNewerThan(const Order* preOrder) const {

        if (preOrder == nullptr)
            return true;
        return isNewerThan(*preOrder);
    }

    inline void setOrderBackTestTime(const QuoteTime_t& quoteTime) {
        orderTime = quoteTime;
    }

    /*回测时每个回测算法单独一个OrderBook*/
    inline OrderBookKey_t getOrderBookKey() const {
        if (agcommon::getQuoteMode(acctType) == agcommon::QuoteMode::REPLAY) {
            return OrderBookKey_t(acctType, acct, algoOrderId);
        }
        return OrderBookKey_t(acctType, acct, brokerId);
    }

    inline AcctKey_t getAcctKey() const {

        return AcctKey_t(acctType, acct, brokerId);
    }

    std::string to_string() const;

    std::string to_string_simple() const;

};

struct Trade: KeepAlivePool<Trade> {

    OrderId_t   tradeId{};

    OrderId_t   orderId{};

    uint64_t    brokerId { 0 };

    AlgoMsg::AcctType acctType{};

    Acct_t      acct{};

    Symbol_t    symbol{};

    int         tradeSide{ 0 };

    int         filledQty{ 0 };

    double      price{ 0 };

    double      filledAmt{ 0.0 };

    double      fee{ 0.0 };

    AlgoOrderId_t    algoOrderId{ 0 };

    BrokerOrderNo_t  brokerOrderNo{};

    OrderTime_t     filledTime{ ALGO_MIN_DATE_TIME };

public:

    Trade() = default;
    ~Trade() = default;

    Trade(const Trade& oth);
    //Trade& operator= (const Trade& oth);

    std::string to_string() const;

    std::string to_string_simple() const;

    inline OrderBookKey_t getOrderBookKey() const {
        if (agcommon::getQuoteMode(acctType) == agcommon::QuoteMode::REPLAY) {
            return OrderBookKey_t(acctType, acct, algoOrderId);
        }
        return OrderBookKey_t(acctType, acct, brokerId);
    }
};