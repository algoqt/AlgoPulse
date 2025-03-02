#include "ShotTrader.h"
#include "QuoteFeedService.h"
#include "OrderBook.h"
#include "common.h"
#include "TCPSession.h"

using namespace agcommon;

ShotTrader::ShotTrader(const std::shared_ptr<AlgoOrder>& _algoOrderPtr
    , const AsioContextPtr& c
    , std::unordered_set<Symbol_t> _symbols) :
    
    algoOrderPtr(_algoOrderPtr)
    , symbols(_symbols)
    , Trader(contextPtr)
    , maxQuoteTime(posix_time::min_date_time)
    
{
    algoPerf.algoOrderId = algoOrderPtr->algoOrderId;
    algoPerf.clientAlgoOrderId = algoOrderPtr->clientAlgoOrderId;
    algoPerf.acctKey = algoOrderPtr->getAcctKey();

    algoPerf.tradeDate = agcommon::getDateInt(algoOrderPtr->startTime);
    algoPerf.startTime = algoOrderPtr->startTime;
    algoPerf.endTime   = algoOrderPtr->endTime;
    algoPerf.symbolRange = algoOrderPtr->symbol;
    algoPerf.createTime = agcommon::now();

    algoPlacerPtr = std::make_shared<AlgoPlacer>(contextPtr);
    auto callback = [ this ](const Order* order) { onOrderUpdate(order); };
    algoPlacerPtr->setOnFusionOrderUpdateCallback(callback, contextPtr);


};

AlgoErrorMessage_t ShotTrader::preStartCheck() {

    tradeDate = agcommon::getDateInt(algoOrderPtr->startTime);

    double Filter_MarketCap_Lower = algoOrderPtr->getOrDefault("Filter_MarketCap_Lower", 0);
    double Filter_MarketCap_Upper = algoOrderPtr->getOrDefault("Filter_MarketCap_Upper", 500);
    int    Filter_MaxLatest_Days = algoOrderPtr->getOrDefault("Filter_MaxLatest_Days", 10);
    double Filter_MaxLatest_Return = algoOrderPtr->getOrDefault("Filter_MaxLatest_Return_Percent", 0)/100.0;

    double underWater           = algoOrderPtr->getOrDefault("Filter_UnderWater_Percent", 0)/100.0;
    double aboveWater           = algoOrderPtr->getOrDefault("Filter_AboveWater_Percent", 0)/100.0;

    auto& dataSource = StockDataManager::getInstance();
    auto treadeDate_lastN   = dataSource.getPreTradeDate_N(tradeDate, Filter_MaxLatest_Days);
    auto treadeDate_last    = dataSource.getPreTradeDate_N(tradeDate, 1);

    auto dailyBar_lastN     = dataSource.getDailyBarBlock(treadeDate_lastN);
    auto dailyBar_base      = dataSource.getDailyBarBlock(treadeDate_last);

    if (algoOrderPtr->isParentOrder) {
        SPDLOG_INFO("{}", algoOrderPtr->to_string());
    }

    if (symbols.size() == 0) {  //  algoOrderPtr->symbol = specialName

        localSecurityInfoMap = StockDataManager::getInstance().getSecurityBlockInfo(tradeDate, algoOrderPtr->symbol);
        
        if (localSecurityInfoMap.size() == 0) {
            return std::format("{} invalid stock range:{}", tradeDate, algoOrderPtr->symbol);
        }
        for (auto& [s, ssinfo] : localSecurityInfoMap) {

            if (not dailyBar_lastN.contains(s)) {
                continue;
            }
            double adjfactor = ssinfo->adjFactor;
            auto&  dailyBar_Nth = dailyBar_lastN[s];
            double adjClose_Nth = dailyBar_Nth->close * dailyBar_Nth->adjFactor;
            double return_lastN = dailyBar_Nth->close > 0 ? (ssinfo->preClosePrice * adjfactor) / adjClose_Nth - 1 : 0;

            SPDLOG_DEBUG("[aId:{}]symbol:{},preClosePrice:{:.2f},today_adjFactor:{:.3f},window_adjFactor:{:.3f},close:{:.2f},return_lastN:{:.2f}"
                , algoOrderPtr->algoOrderId,ssinfo->symbol, ssinfo->preClosePrice, adjfactor, dailyBar_Nth->adjFactor, dailyBar_Nth->close, return_lastN);
            
            double marketCap = ssinfo->unLAShare * ssinfo->preClosePrice / 1e8;

            if ((not ssinfo->isSuspended) and ssinfo->listedDate<tradeDate and ssinfo->delistedDate>tradeDate) {
                auto check1 = true;
                if (Filter_MarketCap_Upper > 0) {
                    check1 = marketCap <= Filter_MarketCap_Upper and marketCap >= Filter_MarketCap_Lower;
                }
                auto check2 = Filter_MaxLatest_Return != 0 ? return_lastN < Filter_MaxLatest_Return : true;
                auto check3 = true;

                if (algoOrderPtr->isBackTestOrder()) {
                    auto dailyBar = dataSource.getDailyBar(s,tradeDate);
                    if (dailyBar) {
                        if (underWater > 0) {
                            check3 = dailyBar->low  < ssinfo->preClosePrice  * (1.0 - underWater);
                        }
                        if (aboveWater > 0) {
                            check3 = dailyBar->high > ssinfo->preClosePrice  * (1.0 + aboveWater);
                        }
                    }
                }
                
                if (check1 and check2 and check3) {
                    symbols.insert(ssinfo->symbol);
                }
            }
            else {
                SPDLOG_DEBUG("[aId:{}]symbol {} isSuspended:{},listedDate:{},dellistedDate:{},marketCap:{:.2f},return_lastN:{:.3f}", algoOrderPtr->algoOrderId,
                    ssinfo->symbol, ssinfo->isSuspended, ssinfo->listedDate, ssinfo->delistedDate, marketCap, return_lastN);
            }
        }
    }
    else {
        localSecurityInfoMap = dataSource.getSecurityInfoBatch(symbols, tradeDate);
    }
    
    nextTradeDate = dataSource.getNextTradeDate(tradeDate);
    if (symbols.size() == 0) {
        return "symbols has 0 size!";
    }

    bars      = dataSource.getDailyBarBlock(tradeDate);
    next_bars = dataSource.getDailyBarBlock(nextTradeDate);

    return "";
}

asio::awaitable<void> ShotTrader::start() {

    if (hasStarted) {
        co_return;
    }
    hasStarted = true;

    auto title = std::format("[aId:{}]ShotTrader", algoOrderPtr->algoOrderId);
    TimeCost tc{ title };

    algoPerf.errMsg = preStartCheck();
    if (algoPerf.errMsg != "") {
        SPDLOG_ERROR("[aId:{}]ShotTrader start error:{}", algoOrderPtr->algoOrderId, algoPerf.errMsg);
        stop(algoPerf.errMsg);
        co_return;
    }

    if (not AlgoStatus::isFinalStatus(algoPerf.algoStatus)) {
        algoPerf.algoStatus = AlgoStatus::Running;
    }
    else {
        publishAlgoperformance();
        co_return;
    }

    publishAlgoperformance();

    const int max_symbolSize_perTask = 75;

    bool useMultiThread = algoOrderPtr->isBackTestOrder() and symbols.size() > max_symbolSize_perTask;

    SPDLOG_INFO("[aId:{}]ShotTrader Start,with total symbols:{}", algoOrderPtr->algoOrderId, symbols.size());

    if (algoOrderPtr->isParentOrder and useMultiThread) {

        createSubTask(max_symbolSize_perTask);    
    }
    else {

        QuoteFeedRequest df_req(algoOrderPtr->getQuoteMode(), algoOrderPtr->algoOrderId, symbols, algoOrderPtr->startTime, algoOrderPtr->endTime, contextPtr);
        auto quoteFeedPtr = QuoteFeedService::getInstance().createQuoteFeed(df_req, algoPerf.errMsg);

        if (!quoteFeedPtr) {
            SPDLOG_ERROR("[aId:{}]ShotTrader start error:{}", algoOrderPtr->algoOrderId, algoPerf.errMsg);
            stop(algoPerf.errMsg);
            co_return;
        }

        auto self = keep_alive_this<ShotTrader>();

        auto onMarketDepth = [ self ](MarketDepth* md) { self->onMarketDepth(md); };

        auto co_onMarketDepth = nullptr;

        auto subscribeMarketDepthReq = std::make_shared<SubMarketDepth_t>(algoOrderPtr->algoOrderId, symbols,
            onMarketDepth, co_onMarketDepth, algoOrderPtr->getAcctKey(), false, contextPtr);

        quoteFeedPtr->regesterOnQuoteFeedFinish([self]() {
                self->stop("");
            },contextPtr);

        if (quoteFeedPtr->subscribe(subscribeMarketDepthReq) != algoOrderPtr->algoOrderId) {
            SPDLOG_ERROR("[aId:{}]subscribe error:{}", algoOrderPtr->algoOrderId, algoPerf.errMsg);
            stop(algoPerf.errMsg);
            co_return;
        }
    }

    auto timer = asio::steady_timer(*contextPtr);
    while (not AlgoStatus::isFinalStatus(algoPerf.algoStatus)) {

        timer.expires_after(std::chrono::milliseconds(1000));
        co_await timer.async_wait(asio::deferred);
        SPDLOG_DEBUG("[aId:{}]Waiting.... {}", algoOrderPtr->algoOrderId, algoPerf.algoStatus);

        if (algoPerf.algoStatus == AlgoStatus::Canceling) {
            if (algoPlacerPtr->hasPendingOrders() == 0) {
                stop("user stop");
            }
        }
    }
    co_return;
}

void  ShotTrader::createSubTask(int max_symbolSize_perTask) {

    auto workerContexts = ContextService::getInstance().getWorkerContext();

    const int symbolSize_perTask = max_symbolSize_perTask;

    std::vector<std::string> symbolPoolVec { symbols.begin(), symbols.end() };

    size_t subTaskCount = symbolPoolVec.size() / symbolSize_perTask;
    subTaskCount = symbolPoolVec.size() - symbolSize_perTask * subTaskCount > 0 ? subTaskCount + 1 : subTaskCount;

    SPDLOG_INFO("[aId:{}]max_symbolSize_perTask:{},will create subTaskCount:{}", algoOrderPtr->algoOrderId, max_symbolSize_perTask, subTaskCount);

    for (size_t i = 0; i < subTaskCount; i++) {

        size_t lower = i * symbolSize_perTask;
        size_t upper = std::min(i * symbolSize_perTask + symbolSize_perTask, symbols.size());

        std::unordered_set<Symbol_t> subTask_symbols{ symbolPoolVec.begin() + lower, symbolPoolVec.begin() + upper };

        auto subAlgoOrderPtr = std::make_shared<AlgoOrder>(*algoOrderPtr);

        subAlgoOrderPtr->algoOrderId = AlgoOrderIdGenerator::getInstance().NewId();
        subAlgoOrderPtr->parentAlgoOrderId = algoOrderPtr->algoOrderId;
        subAlgoOrderPtr->isParentOrder = false;

        subAlgoOrders.push_back({ subAlgoOrderPtr,subTask_symbols });
    }

    SPDLOG_INFO("[aId:{}]max_symbolSize_perTask:{},will spawn subTaskCount:{}", algoOrderPtr->algoOrderId, max_symbolSize_perTask, subAlgoOrders.size());

    auto self = keep_alive_this<ShotTrader>();

    for (auto& ioContext : workerContexts) {
        if (subAlgoOrders.size() == 0) {
            break;
        }
        const auto& [subOrder, subTask_symbols] = subAlgoOrders.back();

        auto trader = std::make_shared<ShotTrader>(subOrder, ioContext, subTask_symbols);

        if (trader->preStartCheck() == "") {
            asio::co_spawn(*ioContext, trader->start(), [self, trader](const std::exception_ptr ex) {
                self->onSubTaskBackTestDone(trader, ex);
                }
            );
            subTraders.insert({ subOrder->algoOrderId,trader });
        }

        subAlgoOrders.pop_back();

        SPDLOG_INFO("[aId:{}]spawned,symbol size:{},paId:{} ", subOrder->algoOrderId, subTask_symbols.size(), subOrder->parentAlgoOrderId);
    }
}

void ShotTrader::onSubTaskBackTestDone(const std::shared_ptr<ShotTrader>& trader_ptr, const std::exception_ptr ex) {

    auto& subAlgoOrderId = trader_ptr->algoOrderPtr->algoOrderId;

    if (!ex) {
        SPDLOG_INFO("[aId:{}]co_spawn done, paraent aId:{}"  , subAlgoOrderId, trader_ptr->algoOrderPtr->parentAlgoOrderId);
    }
    else {
        SPDLOG_ERROR("[aId:{}]co_spawn failed,paraent aId:{}", subAlgoOrderId, trader_ptr->algoOrderPtr->parentAlgoOrderId);
        try {
            std::rethrow_exception(ex);
        }
        catch (const std::exception& e) {
            SPDLOG_ERROR("[aId:{}]exception:{}", subAlgoOrderId, e.what());
        }
    }
    asio::dispatch(*contextPtr, [this, trader_ptr]() {

        for (const auto& [symbol, sss] : trader_ptr->symbol2Signals) {
            auto& vec = symbol2Signals[symbol];
            for (auto& [id,ss] : sss) {
                ss->algoOrderId = trader_ptr->algoOrderPtr->parentAlgoOrderId;
                vec.insert({ id,ss });
            }
        }
        trader_ptr->symbol2Signals.clear();

        if (not subAlgoOrders.empty()) {

            auto& [shOrder, part_symbols] = subAlgoOrders.back();
            auto trader = std::make_shared<ShotTrader>(shOrder,trader_ptr->contextPtr, part_symbols);

            if (trader->preStartCheck() == "") {

                auto self = keep_alive_this<ShotTrader>();
                asio::co_spawn(*trader_ptr->contextPtr, trader->start(), 
                    [self,trader](const std::exception_ptr ex) {
                        self->onSubTaskBackTestDone(trader, ex);
                    }
                );
                subTraders.insert({ shOrder->algoOrderId,trader });
            }
            subAlgoOrders.pop_back();
        }
        subTraders.erase(trader_ptr->getAlgoOrderId());
        SPDLOG_INFO("[aid{}]Remain subAlgoOrders size:{},running traders:{}", algoOrderPtr->algoOrderId, subAlgoOrders.size(), subTraders.size());
        if(subTraders.size()==0) {
            stop("");
        }
    }
    );
}

void ShotTrader::stop(const std::string& reason) {
    if (AlgoStatus::isFinalStatus(algoPerf.algoStatus))
        return;
    
    if (reason != "") {
        algoPerf.errMsg = reason;
        if (algoPerf.errMsg == "user stop") {
            algoPerf.algoStatus = AlgoStatus::Terminated;
        }
        else {
            algoPerf.algoStatus = AlgoStatus::Error;
        }
    }
    else {
        algoPerf.algoStatus = AlgoStatus::Finished;
    }

    if (quoteFeedPtr) {
        quoteFeedPtr->unSubscribe(algoOrderPtr->algoOrderId);
    }

    if (orderBookPtr) {
        orderBookPtr->unSubscribe(algoOrderPtr->algoOrderId);
    }

    performanceSummary(true);

    if (algoOrderPtr->isParentOrder) {
        for (const auto& [symbol, sss] : symbol2Signals) {
            for (const auto& [id,ss] : sss) {
                SPDLOG_INFO(ss->to_string());
                ss->signal_at_md->release();
            }
        }
    }

    release();
}

void ShotTrader::release() {

    for (auto& [symbol, queue] : symbol2MdQueue) {
        queue->release();
    }
    symbol2MdQueue.clear();

    orderBookPtr = nullptr;
    quoteFeedPtr = nullptr;

    return;
}

void ShotTrader::cancel() {

    if (isStopped())
        return;

    if (algoPlacerPtr->cancelAllAlgoOrder()) {
        algoPerf.algoStatus = agcommon::AlgoStatus::Canceling;
    }
    else {
        stop("user stop");
    }
}

void ShotTrader::onOrderUpdate(const Order* order) {

    SPDLOG_DEBUG("{}", order->to_string());

    if (const auto it = symbol2Signals.find(order->symbol); it != symbol2Signals.end()) {
        if (const auto it_it = it->second.find(order->pairId); it_it != it->second.end()) {
            it_it->second->avgBuyPrice = order->avgPrice;
            SPDLOG_DEBUG("[{}]", it_it->second->to_string());
        }
        else {
            SPDLOG_WARN("[aid:{},{}] cant find shotSignal {}.", algoOrderPtr->algoOrderId, order->symbol, order->pairId);
        }
    }
}

void  ShotTrader::onMarketDepth(MarketDepth* md) {
    if (md->quoteTime > algoOrderPtr->endTime) {
        stop("");
        return;
    }
    if (isStopped()) {
        return;
    }

    algoShot(md);
}

void ShotTrader::algoShot(MarketDepth* md) {

    if (md->quoteTime < agcommon::AshareMarketTime::getMarketOpenTime(md->quoteTime)) {
        return;
    }
    if (agcommon::AshareMarketTime::isAfternoonBreakTime(md->quoteTime)) {
        return;
    }
    auto preMaxQuoteTime = maxQuoteTime;
    if (md->quoteTime > maxQuoteTime) {
        maxQuoteTime = md->quoteTime;
    }

    int SHOT_Shot_Window_TickSize  = algoOrderPtr->getOrDefault("SHOT_Watch_Window_TickSize", 120);
    int SHOT_Refer_Window_TickSize = algoOrderPtr->getOrDefault("SHOT_Refer_Window_TickSize",0);

    auto [it, inserted] = symbol2MdQueue.try_emplace(md->symbol, std::make_shared<LimitedQueue>(SHOT_Shot_Window_TickSize,SHOT_Refer_Window_TickSize));
    auto& mdQueue = it->second;
    mdQueue->append(md);

    auto& signalMap = symbol2Signals[md->symbol];

    for (auto& [id, ss] : signalMap) {
        ss->update(md, true);
    }
    
    if (md->quoteTime< algoOrderPtr->startTime or md->quoteTime>algoOrderPtr->endTime or
        md->quoteTime >= agcommon::AshareMarketTime::getClosingCallAuctionBeginTime(md->quoteTime)) {
        return;
    }

    if (mdQueue->isReady() and AlgoStatus::isRunning(algoPerf.algoStatus)) {

        double shotAmtThreshold  = algoOrderPtr->getOrDefault("SHOT_Watch_Window_Money", 0.0)      * 10000.0;
        double shotFlowThreshold = algoOrderPtr->getOrDefault("SHOT_Watch_Window_Money_Flow", 0.0) * 10000.0;
        double shotReferRatio    = algoOrderPtr->getOrDefault("SHOT_Watch2Refer_Ratio", 0.0);
        double underWater        = algoOrderPtr->getOrDefault("Filter_UnderWater_Percent", 0.0)/100.0;
        double aboveWater        = algoOrderPtr->getOrDefault("Filter_AboveWater_Percent", 0.0)/100.0;
        double SHOT_Watch_Window_Range_Percent = algoOrderPtr->getOrDefault("SHOT_Watch_Window_Range_Percent", 2.0);
        double SHOT_Confirm_Duration_Seconds = algoOrderPtr->getOrDefault("SHOT_Confirm_Duration_Seconds", 60.0 * 5);

        auto base_md = mdQueue->get_item(0);

        const auto [shotAmt, shotAvgAmt]   = mdQueue->getShotAmount();
        const auto [referAmt, referAvgAmt] = mdQueue->getShotAmount();

        double raito = referAvgAmt > 0 ? shotAvgAmt / referAvgAmt : 0;
        if (shotReferRatio > 0 and not (raito > shotReferRatio)) {
            return;
        }

        auto& ssinfo = localSecurityInfoMap.find(md->symbol)->second;

        if (underWater > 0) {
            if (ssinfo->lowLimitPrice == 0) {
                return;
            }
            double _refPrice = ssinfo->preClosePrice * (1.0 - underWater);
            if (md->low > _refPrice) {
                return;
            }
        }
        if (aboveWater > 0) {
            if (ssinfo->highLimitPrice == 0) {
                return;
            }
            double _refPrice = ssinfo->preClosePrice * (1.0 + aboveWater);
            if (md->high < _refPrice) {
                return;
            }
        }

        bool istrig = mdQueue->triggerShotMinMax(md, SHOT_Watch_Window_Range_Percent, SHOT_Confirm_Duration_Seconds);

        if (istrig) {

            double midPrice     = base_md->getMidPrice();
            double shotChange   = mdQueue->getShotChageRate();
            double shotDuration = AshareMarketTime::getMarketDuration(base_md->quoteTime, md->quoteTime);

            double buyPrice = std::max(md->price, md->askPrice1);

            std::size_t times = signalMap.size() + 1;

            if (buyPrice > 0) {
                int qty = algoOrderPtr->qtyTarget;
                if (qty == 0 and ssinfo) {
                    qty = ssinfo->floor2FitLotSize(algoOrderPtr->amtTarget / buyPrice);
                }
                else {
                    return;
                }
                auto ss = std::make_shared<ShotSignal>(algoOrderPtr->algoOrderId, md);

                ss->nextDay  = nextTradeDate;
                ss->preClose = md->preClose;
                ss->openPrice = md->open;

                if (md->preClose > 0)
                    ss->openReturn = (md->open / md->preClose - 1) * 100;

                ss->sigShotAmt     = shotAmt / 10000.0;
                ss->sigShotChange  = shotChange;
                ss->sigShotDuration = shotDuration;
                ss->sigShotFlow     = mdQueue->shotPeriodFlow / 10000;
                ss->sigShotTr       = mdQueue->shotPeriodVol * 1.0 / md->volume * md->turnRate;

                ss->sigArrivePrice  = md->price;
                ss->sigArriveChange = md->changeP;
                ss->buyPrice = md->askPrice1;

                ss->avgBuyPrice = 0;
                ss->lastPrice  =  md->price;

                ss->avgSellPrice = 0;

                ss->cnt = times;
                ss->totalTr = md->turnRate;

                if (ssinfo) {
                    ss->description = ssinfo->getIndexBelongName();
                }

                auto order      = Order::make_intrusive();
                order->orderId  = OrderIdGenerator::getInstance().NewId();
                order->acct     = algoOrderPtr->acct;
                order->acctType = algoOrderPtr->acctType;
                order->symbol   = md->symbol;
                order->tradeSide = agcommon::TradeSide::BUY;
                order->orderQty  = qty;
                order->orderPrice = 0;
                order->pairId = ss->shotId;
                order->algoOrderId = algoOrderPtr->algoOrderId;
                order->orderTime = agcommon::now();

                signalMap.insert({ ss->shotId,ss });

                algoPlacerPtr->placeAlgoOrder(order.get());

                SPDLOG_INFO("[SHOTS]{}", ss->to_string());

                publishSignalMessage(ss.get());
            }
        }
    }

    if (maxQuoteTime > preMaxQuoteTime) {
        performanceSummary();
    }

}

void ShotTrader::performanceSummary(bool backTestDone /*=false*/) {

    if (algoOrderPtr->isBackTestOrder()) {
        if (not backTestDone)       //when bt,only send last result when done
            return;
    }

    if (not algoOrderPtr->isParentOrder)
        return;

    int signalCounts    = 0;
    int        wins     = 0;
    double pnl_win      = 0;
    int        losses   = 0;
    double pnl_loss     = 0;

    double total_pnl_this_day = 0;
    double total_pnl = 0;
    double total_nextDayOpenReturn = 0.0;

    std::shared_ptr<DailyBar> next_bar = nullptr;
    std::shared_ptr<DailyBar> bar = nullptr;

    for (auto& [symbol, sss] : symbol2Signals) {

        if (auto it = bars.find(symbol); it != bars.end()) {
            bar = it->second;
        }

        if (auto it = next_bars.find(symbol); it != next_bars.end()) {
            next_bar = it->second;
        }
        
        for (auto& [id,ss] : sss) {

            ss->nextDayOpenReturn = 0;
            if (backTestDone) {
                if (bar != nullptr) {
                    ss->adjFactor = bar->adjFactor;
                    ss->lastPrice = bar->close;
                }
                if (next_bar != nullptr) {
                    ss->nextDayAdjFactor = next_bar->adjFactor;
                    ss->nextDayOpenPrice = next_bar->open;
                    ss->avgSellPrice     = next_bar->open; // to be exec
                    ss->nextDayOpenReturn = next_bar->preclose > 0 ? next_bar->open * 100.0 / next_bar->preclose - 100 : 0;
                }
            }

            if (ss->buyPrice > 0 and ss->avgBuyPrice > 0) {
                double sigPnlReturn = ss->avgBuyPrice > 0 ? (ss->lastPrice - ss->avgBuyPrice) / ss->avgBuyPrice * 100.0 : 0;
                ss->sigPnlReturn = ((1 + sigPnlReturn / 100) * (1 + ss->nextDayOpenReturn / 100) - 1) * 100;
            }
            
            if (ss->cnt == 1) {

                if (ss->buyPrice > 0 and ss->avgBuyPrice > 0) {
                    signalCounts += 1;
                    total_nextDayOpenReturn += ss->nextDayOpenReturn;
                    total_pnl_this_day += ss->avgBuyPrice > 0 ? (ss->lastPrice - ss->avgBuyPrice) / ss->avgBuyPrice * 100.0 : 0;
                    total_pnl += ss->sigPnlReturn;
                }

                if (ss->sigPnlReturn > 0) {
                    wins += 1;
                    pnl_win += ss->sigPnlReturn;
                }
                if (ss->sigPnlReturn < 0) {
                    losses += 1;
                    pnl_loss += ss->sigPnlReturn;
                }
            }

            publishSignalMessage(ss.get());
        }
    }

    algoPerf.symbolCount = symbol2Signals.size();
    algoPerf.signalCount = signalCounts;

    algoPerf.winRate = (wins + losses) > 0 ? wins * 100.0 / (wins + losses) : 0.0;
    algoPerf.profit2LossRatio = 0.0;
    algoPerf.avgPnlWin  = wins >   0 ? pnl_win / wins : 0;
    algoPerf.avgPnlLoss = losses > 0 ? pnl_loss / losses : 0;
    if (losses > 0 and wins > 0) {
        algoPerf.profit2LossRatio = (pnl_win / wins) / (pnl_loss / losses) * -1;
    }
    algoPerf.avgPnl           = signalCounts > 0 ? total_pnl / signalCounts : 0.0;
    algoPerf.avgPnlThisDay    = signalCounts > 0 ? total_pnl_this_day / signalCounts : 0.0;
    algoPerf.avgNextDayOpenReturn = signalCounts > 0 ? total_nextDayOpenReturn / signalCounts : 0.0;

    publishAlgoperformance();

    if (isStopped()) {
        SPDLOG_INFO(
            "[algoPerf]aid:{}, symbolCount: {}, signalCount: {}, winRate: {:.2f}%, "
            "profit2LossRatio: {:.2f}, avgPnlWin: {:.2f}%, avgPnlLoss: {:.2f}%, avgNextDayOpenReturn: {:.2f}%",
            algoOrderPtr->algoOrderId
            , algoPerf.symbolCount
            , algoPerf.signalCount
            , algoPerf.winRate
            , algoPerf.profit2LossRatio
            , algoPerf.avgPnlWin
            , algoPerf.avgPnlLoss
            , algoPerf.avgNextDayOpenReturn
        );
    }
}
 
void ShotTrader::publishSignalMessage(const ShotSignal* ss) const {

    auto msgPtr = std::make_shared<AlgoMsg::MsgShotSignalInfo>();

    msgPtr->set_algo_order_id(algoOrderPtr->algoOrderId);

    if (not algoOrderPtr->isParentOrder) {  // only for backtest
        msgPtr->set_algo_order_id(algoOrderPtr->parentAlgoOrderId);
    }

    msgPtr->set_client_algo_order_id(algoOrderPtr->clientAlgoOrderId);
    msgPtr->set_acct(algoOrderPtr->acct);
    msgPtr->set_acct_type(algoOrderPtr->acctType);
    msgPtr->set_algo_category(algoOrderPtr->algoCategory);
    msgPtr->set_symbol(ss->symbol);
    msgPtr->set_amt_target(algoOrderPtr->amtTarget);

    msgPtr->set_signal_id(ss->shotId);

    msgPtr->set_symbol(ss->symbol);
    msgPtr->set_pre_close(ss->preClose);
    msgPtr->set_open_price(ss->openPrice);
    msgPtr->set_open_return(ss->openReturn);
    msgPtr->set_signal_time(agcommon::getDateTimeInt(ss->signalTime));
    msgPtr->set_sig_shot_amt(ss->sigShotAmt);
    msgPtr->set_sig_shot_change(ss->sigShotChange);
    msgPtr->set_sig_shot_duration(ss->sigShotDuration);
    msgPtr->set_sig_shot_flow(ss->sigShotFlow);
    msgPtr->set_sig_shot_tr(ss->sigShotTr);
    msgPtr->set_sig_arrive_price(ss->sigArrivePrice);
    msgPtr->set_sig_arrive_change(ss->sigArriveChange);
    msgPtr->set_buy_price(ss->buyPrice);
    msgPtr->set_avg_buy_price(ss->avgBuyPrice);
    msgPtr->set_avg_sell_price(ss->avgSellPrice);
    msgPtr->set_sig_pnl(ss->sigPnlReturn);
    msgPtr->set_close_price(ss->lastPrice);
    msgPtr->set_adj_factor(ss->adjFactor);
    msgPtr->set_next_day_adj_factor(ss->nextDayAdjFactor);
    msgPtr->set_next_day_open_price(ss->nextDayOpenPrice);
    msgPtr->set_next_day_open_return(ss->nextDayOpenReturn);
    msgPtr->set_cnt(ss->cnt);
    msgPtr->set_total_tr(ss->totalTr);
    msgPtr->set_description(ss->description);

    auto shouldCache = AlgoStatus::isFinalStatus(algoPerf.algoStatus);

    TCPSessionManager::getInstance().sendNotify2C(algoOrderPtr->getAcctKey(), AlgoMsg::CMD_NOTIFY_ShotSignalInfo, msgPtr, shouldCache);

    return ;
}

void ShotTrader::publishAlgoperformance() const {
    if (not algoOrderPtr->isParentOrder)
        return;

    auto msgPtr = std::make_shared<AlgoMsg::MsgShotPerformance>();
    msgPtr->set_algo_order_id(algoPerf.algoOrderId);
    msgPtr->set_client_algo_order_id(algoPerf.clientAlgoOrderId);
    msgPtr->set_algo_category(algoOrderPtr->algoCategory);
    msgPtr->set_algo_strategy(algoOrderPtr->algoStrategy);
    msgPtr->set_acct(algoOrderPtr->acct);
    msgPtr->set_acct_type(algoOrderPtr->acctType);
    msgPtr->set_trade_date(algoPerf.tradeDate);
    msgPtr->set_start_time(agcommon::getDateTimeInt(algoPerf.startTime));
    msgPtr->set_end_time(agcommon::getDateTimeInt(algoPerf.endTime));
    msgPtr->set_symbol_range(algoPerf.symbolRange);
    msgPtr->set_symbol_count(algoPerf.symbolCount);
    msgPtr->set_signal_count(algoPerf.signalCount);
    msgPtr->set_win_rate(algoPerf.winRate);
    msgPtr->set_profit2_loss_ratio(algoPerf.profit2LossRatio);
    msgPtr->set_avg_pnl_win(algoPerf.avgPnlWin);
    msgPtr->set_avg_pnl_loss(algoPerf.avgPnlLoss);
    msgPtr->set_trade_amount(algoPerf.tradeAmount);
    msgPtr->set_avg_pnl(algoPerf.avgPnl);
    msgPtr->set_avg_pnl_this_day(algoPerf.avgPnlThisDay);
    msgPtr->set_avg_next_day_open_return(algoPerf.avgNextDayOpenReturn);
    msgPtr->set_algo_status(algoPerf.algoStatus);
    msgPtr->set_algo_msg(algoPerf.errMsg);
    msgPtr->set_create_time(agcommon::getDateTimeInt(algoPerf.createTime));

    auto shouldCache = agcommon::AlgoStatus::isFinalStatus(algoPerf.algoStatus);

    TCPSessionManager::getInstance().sendNotify2C(algoOrderPtr->getAcctKey(), AlgoMsg::CMD_NOTIFY_ShotPerformance, msgPtr, shouldCache);
    
}