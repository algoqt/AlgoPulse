#include "AlgoOrder.h"
#include "Configs.h"

std::string Slicer::to_string() {

    return std::format("index:{},cumQty:{},bias:{:.2f},duration:{:.2f},duration_remain:{:.2f},rounds:{},qty2make:{},qty2take:{},isLast:{}"
        ,index, cumQty, bias, duration, duration_remain, rounds_remain, qty2make, qty2take, isLast);
}


OrderBookKey_t AlgoOrder::getOrderBookKey() const {

    if (getQuoteMode() == agcommon::QuoteMode::REPLAY) {
        return OrderBookKey_t(acctType, acct, algoOrderId);
    }

    return OrderBookKey_t(acctType, acct, brokerId);
}

AcctKey_t AlgoOrder::getAcctKey() const {

    return AcctKey_t(acctType, acct, brokerId);
}

agcommon::QuoteMode AlgoOrder::getQuoteMode() const {

    return agcommon::getQuoteMode(acctType);

}

std::string AlgoOrder::to_string() const {

    std::string algoParamStr;
    for (const auto& [k, v] : paramKeyconfig) {
        if (!algoParamStr.empty()) {
            algoParamStr += ", ";
        }
        algoParamStr += k + "=" + v;
    }

    const auto& algoStrategy_name = AlgoMsg::MsgAlgoStrategy_Name(algoStrategy);

    return fmt::format(
        "[{}]: acctType={}, acct={}, symbol={}, tradeSide={}, algoStrategy={}, qtyTarget={}, amtTarget={}, startTime={}, "
        "endTime={}, execDuration={}, priceLimit={}, participateRate={}, minAmountPerOrder={}, notPegOrderAtLimitPrice={}, "
        "notBuyOnLLOrSellOnHL={}, clientAlgoOrderId={}, createTime={},algoParamStr:{}",
        algoOrderId,
        acctType,
        acct,
        symbol,
        agcommon::TradeSide::name(tradeSide),
        algoStrategy_name,
        qtyTarget,
        amtTarget,
        agcommon::getDateTimeStr(startTime),
        agcommon::getDateTimeStr(endTime),
        execDuration,
        priceLimit,
        participateRate,
        minAmountPerOrder,
        notPegOrderAtLimitPrice,
        notBuyOnLLOrSellOnHL,
        clientAlgoOrderId,
        agcommon::getDateTimeStr(createTime),
        algoParamStr
    );
}