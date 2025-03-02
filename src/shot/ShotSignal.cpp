#include "ShotSignal.h"
#include "IdGenerator.h"

using AshareMarketTime = agcommon::AshareMarketTime;

ShotSignal::ShotSignal(const AlgoOrderId_t algoOrderId, MarketDepth* signal_at_md)

    : shotId(agcommon::getTimeInt(signal_at_md->quoteTime) * 1000000 + std::stoull(signal_at_md->symbol.substr(0, 6)))
    , algoOrderId(algoOrderId)
    , symbol(signal_at_md->symbol)
    , signalTime(signal_at_md->quoteTime)
    , signal_at_md{ signal_at_md } {
}


void ShotSignal::update(const MarketDepth* md, const bool useAvgTradePrice) {

    //if (useAvgTradePrice and AshareMarketTime::getMarketDuration(signal_at_md->quoteTime, md->quoteTime) <= 60) {
    //    avgBuyPrice = md->volume - signal_at_md->volume > 0 ? (md->amount - signal_at_md->amount) / (md->volume - signal_at_md->volume) : signal_at_md->askPrice1;
    //}
    //else if (avgBuyPrice == 0 and md->quoteTime < AshareMarketTime::getClosingCallAuctionBeginTime(md->quoteTime)) {
    //    avgBuyPrice = md->askPrice1;
    //}

    lastPrice = md->price;
    if (!isClosed && avgBuyPrice > 0) {
        sigPnlReturn = (lastPrice - avgBuyPrice) / avgBuyPrice * 100.0;
    }
}

std::string ShotSignal::to_string() const {

    constexpr auto format3 = [](double price) { return fmt::format("{:.3f}", price); };
    constexpr auto formatPct = [](double pct) { return fmt::format("{:4.2f}%", pct); };
    constexpr auto formatAmount = [](double amt) { return fmt::format("{:6.2f}", amt); };
    constexpr auto formatDuration = [](double duration) { return fmt::format("{:.1f}", duration); };

    auto formattedSignalTime = agcommon::getDateTimeInt(signalTime);

    return std::format(
        "aid:{}, desc:{}, symbol:{}, preClose:{}, OpenReturn:{}, signalTime:{}, sigArriveChange:{}, "
        "sigShotAmt:{}, sigShotChange:{}, sigShotDuration:{}, sigArrivePrice:{}, sigShotTr:{}, cnt:{}, "
        "buyPrice:{}, avgBuyPrice:{}, closePrice:{}, sigPnlReturn:{}, nextDayOpenPrice:{}, nextDayOpenReturn:{}, "
        "adjFactor:{}, nextDayAdjFactor:{}, avgSellPrice:{}, totalTr:{}",
        algoOrderId,
        description,
        symbol,
        format3(preClose),
        formatPct(openReturn), 
        formattedSignalTime,
        formatPct(sigArriveChange), 
        formatAmount(sigShotAmt), 
        formatPct(sigShotChange), 
        formatDuration(sigShotDuration),
        format3(sigArrivePrice),
        format3(sigShotTr),
        cnt,
        format3(buyPrice),
        format3(avgBuyPrice),
        format3(lastPrice),
        formatPct(sigPnlReturn),   
        format3(nextDayOpenPrice),
        formatPct(nextDayOpenReturn),
        format3(adjFactor),
        format3(nextDayAdjFactor),
        format3(avgSellPrice),
        format3(totalTr)     
    );
}
