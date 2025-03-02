#include"Order.h"

Order::Order(const Order& other)
    : orderId(other.orderId),
    orderTime(other.orderTime),
    brokerId(other.brokerId),
    acctType(other.acctType),
    acct(other.acct),
    symbol(other.symbol),
    tradeSide(other.tradeSide),
    status(other.status),
    orderQty(other.orderQty),
    orderPrice(other.orderPrice),
    orderType(other.orderType),
    filledQty(other.filledQty),
    avgPrice(other.avgPrice),
    filledAmt(other.filledAmt),
    cancelQty(other.cancelQty),
    pairId(other.pairId),
    algoOrderId(other.algoOrderId),
    cancelTime(other.cancelTime),
    cancelTimes(other.cancelTimes),
    updTime(other.updTime),
    downStreamOrderId(other.downStreamOrderId),
    brokerOrderNo(other.brokerOrderNo),
    quoteTime(other.quoteTime),
    text(other.text) {
}

Order& Order::operator=(const Order& other) {
    if (this != &other) {

        orderId = other.orderId;
        orderTime = other.orderTime;
        brokerId = other.brokerId;
        acctType = other.acctType;
        acct = other.acct;
        symbol = other.symbol;
        tradeSide = other.tradeSide;
        status = other.status;
        orderQty = other.orderQty;
        orderPrice = other.orderPrice;
        orderType = other.orderType;
        filledQty = other.filledQty;
        avgPrice = other.avgPrice;
        filledAmt = other.filledAmt;
        cancelQty = other.cancelQty;
        pairId = other.pairId;
        algoOrderId = other.algoOrderId;
        cancelTime = other.cancelTime;
        cancelTimes = other.cancelTimes;
        updTime = other.updTime;

        downStreamOrderId = other.downStreamOrderId;
        brokerOrderNo = other.brokerOrderNo;
        quoteTime = other.quoteTime;
        text = other.text;
    }
    return *this;
}

std::string Order::to_string() const {

    constexpr auto formatPrice = [](double price) { return fmt::format("{:.3f}", price); };

    auto formattedQuoteTime = agcommon::getTimeStr(quoteTime);

    return fmt::format(
        "orderId:{},pairId:{},algoOrderId:{},acct:{},acctType:{},tradeSide:{},symbol:{},price:{},avgPrice:{},qty:{}, "
        "filledQty:{},cancelQty:{},status:{},qt:{},text:{}",
        orderId,
        pairId,
        algoOrderId,
        acct,
        acctType,
        agcommon::TradeSide::name(tradeSide),
        symbol,
        formatPrice(orderPrice),
        formatPrice(avgPrice),
        orderQty,
        filledQty,
        cancelQty,
        status,
        formattedQuoteTime,
        text
    );
}

std::string Order::to_string_simple() const {

        constexpr auto formatPrice = [](double price) { return fmt::format("{:.3f}", price); };

        auto formattedQuoteTime = agcommon::getTimeStr(quoteTime);

        return fmt::format(
            "orderId:{},algoOrderId:{},symbol:{},price:{},avgPrice:{},qty:{}, "
            "filledQty:{},cancelQty:{},status:{},qt:{}",
            orderId,
            algoOrderId,
            symbol,
            formatPrice(orderPrice),
            formatPrice(avgPrice),
            orderQty,
            filledQty,
            cancelQty,
            status,
            formattedQuoteTime
        );

}

Trade::Trade(const Trade& oth)
    : tradeId(oth.tradeId),
    orderId(oth.orderId),
    brokerId(oth.brokerId),
    acctType(oth.acctType),
    acct(oth.acct),
    symbol(oth.symbol),
    tradeSide(oth.tradeSide),
    filledQty(oth.filledQty),
    price(oth.price),
    filledAmt(oth.filledAmt),
    fee(oth.fee),
    algoOrderId(oth.algoOrderId),
    brokerOrderNo(oth.brokerOrderNo),
    filledTime(oth.filledTime) {
}

//Trade& Trade::operator=(const Trade& oth) {
//    if (this != &oth) {
//        KeepAlive::release();
//        tradeId = oth.tradeId;
//        orderId = oth.orderId;
//        brokerId = oth.brokerId;
//        acctType = oth.acctType;
//        acct = oth.acct;
//        symbol = oth.symbol;
//        tradeSide = oth.tradeSide;
//        filledQty = oth.filledQty;
//        price = oth.price;
//        filledAmt = oth.filledAmt;
//        fee = oth.fee;
//        algoOrderId = oth.algoOrderId;
//        brokerOrderNo = oth.brokerOrderNo;
//        filledTime = oth.filledTime;
//        KeepAlive::operator=(oth);
//    }
//    return *this;
//}

std::string Trade::to_string() const {

    constexpr auto formatPrice = [](double price) { return fmt::format("{:.3f}", price); };

    return fmt::format(
        "tradeId:{},orderId:{},algoOrderId:{},acct:{},acctType:{},tradeSide:{},symbol:{}, "
        "price:{},filledQty:{},filledAmt:{},filledTime:{}",
        tradeId,
        orderId,
        algoOrderId,
        acct,
        acctType,
        agcommon::TradeSide::name(tradeSide),
        symbol,
        formatPrice(price), 
        filledQty,
        formatPrice(filledAmt),
        agcommon::getTimeStr(filledTime)
    );
}

std::string Trade::to_string_simple() const {

    constexpr auto formatPrice = [](double price) { return fmt::format("{:.3f}", price); };

    return fmt::format(
        "tradeId:{},orderId:{},algoOrderId:{},symbol:{}, "
        "price:{},filledQty:{},filledAmt:{},filledTime:{}",
        tradeId,
        orderId,
        algoOrderId,
        symbol,
        formatPrice(price),
        filledQty,
        formatPrice(filledAmt),
        agcommon::getTimeStr(filledTime)
    );
}
