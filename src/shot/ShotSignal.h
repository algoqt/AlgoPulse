#pragma once

#include <vector>
#include "common.h"
#include "MarketDepth.h"

class ShotSignal {

public:

    explicit ShotSignal(const AlgoOrderId_t algoOrderId, MarketDepth* signal_at_md);

    OrderId_t   shotId;

    AlgoOrderId_t    algoOrderId;

    std::string symbol;

    QuoteTime_t  signalTime = MIN_DATE_TIME;

    double preClose = 0;
    double openPrice = 0;
    double openReturn = -1;

    double sigShotFlow = 0;
    double sigShotAmt = 0;
    double sigShotChange = 0.0;     /*SHOT 窗口期涨幅 */
    double sigShotDuration = 0.0;   /*窗口时长*/
    double sigShotTr = 0.0;         /*窗口换手率*/

    double sigArrivePrice = 0;      /*信号时最新价*/
    double sigArriveChange = 0;     /*信号时股票涨幅*/

    double buyPrice    = 0;         /*信号时对手1价*/
    double avgBuyPrice = 0.0;
    double avgSellPrice = 0;
    double lastPrice = 0;           /*最新价*/
    double sigPnlReturn = 0;

    double      adjFactor = 1;
    uint32_t    nextDay = 0;
    double      nextDayOpenPrice = 0;
    double      nextDayOpenReturn = 0.0;
    double      nextDayAdjFactor = 1;

    int    cnt = 0;             /*当天第几次信号*/
    double totalTr = 0;
    bool   isClosed = false;

    std::string description;

    std::vector<std::pair<std::string, double>> nextNDayClosePrice;

    MarketDepthKeepAlivePtr signal_at_md;

    void update(const MarketDepth* md, const bool useAvgTradePrice);

    std::string to_string() const;

};

struct ShotPerformance {

    AlgoOrderId_t   algoOrderId = 0;
    std::string     clientAlgoOrderId;
    AcctKey_t       acctKey{};

    int32_t        tradeDate{};
    OrderTime_t    startTime{};
    OrderTime_t    endTime{};

    std::string     symbolRange;
    int         symbolCount = 0;
    int         signalCount = 0;
    double      winRate = 0;
    double      profit2LossRatio = 0;
    double      avgPnlWin = 0;
    double      avgPnlLoss = 0;
    double      tradeAmount = 0;
    double      avgPnl = 0;
    double      avgPnlThisDay = 0;
    double      avgNextDayOpenReturn = 0;

    int         algoStatus = agcommon::AlgoStatus::Creating;

    std::string     errMsg;

    OrderTime_t     createTime{};

};