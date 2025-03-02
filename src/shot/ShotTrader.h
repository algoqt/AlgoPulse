#pragma once

#include <vector>
#include <unordered_map>
#include "common.h"
#include "AlgoOrder.h"
#include "ShotSignal.h"
#include "AlgoPlacer.h"
#include "StockDataManager.h"
#include "LimitedQueue.h"

class OrderBook;
class QuoteFeed;

class ShotTrader : public Trader {

public:

    ShotTrader(const std::shared_ptr<AlgoOrder>& _algoOrderPtr
        , const AsioContextPtr& contextPtr = nullptr
        , std::unordered_set<Symbol_t> _symbols = {}) ;

    AlgoErrorMessage_t preStartCheck()  override;

    asio::awaitable<void> start()       override;

    void stop(const std::string& reasion) override;

    void onMarketDepth(MarketDepth* md)   override;

    void onOrderUpdate(const Order* order) override;

    bool isStopped() const override { return agcommon::AlgoStatus::isFinalStatus(algoPerf.algoStatus); }

    void cancel() override;

    void release() override;

    AlgoMsg::MsgAlgoCategory getAlgoCategory() const override { return algoOrderPtr->algoCategory; };

    AlgoOrderId_t  getAlgoOrderId() const override {
        if (algoOrderPtr)
            return algoOrderPtr->algoOrderId;
        return 0;
    };

    std::shared_ptr<AlgoOrder>  getAlgoOrder() const override { return algoOrderPtr; };

private:

    void algoShot(MarketDepth* md);

    void performanceSummary(bool backTestDone = false);

    void publishSignalMessage(const ShotSignal* ss) const;

    void publishAlgoperformance() const override;

    std::shared_ptr<AlgoOrder>      algoOrderPtr;

    ShotPerformance                 algoPerf;

    std::shared_ptr<AlgoPlacer>     algoPlacerPtr;

    std::unordered_set<Symbol_t>     symbols{};

    std::unordered_map<Symbol_t, std::shared_ptr<SecurityStaticInfo>>   localSecurityInfoMap{};

    std::unordered_map<std::string, std::shared_ptr<LimitedQueue>>      symbol2MdQueue{};

    std::shared_ptr<OrderBook> orderBookPtr{ nullptr };

    std::shared_ptr<QuoteFeed>  quoteFeedPtr{ nullptr };

    using SignalContainer = std::unordered_map<Symbol_t, std::unordered_map<OrderId_t, std::shared_ptr<ShotSignal>>>;

    SignalContainer symbol2Signals{};

    QuoteTime_t     maxQuoteTime{};

private:

    uint32_t   tradeDate = 0;

    uint32_t   nextTradeDate = 0;

    using DailyBarMap = std::unordered_map<Symbol_t, std::shared_ptr<DailyBar>>;

    DailyBarMap bars{};
    DailyBarMap next_bars{};

    /*backtest only*/
    void createSubTask(int max_symbolSize_perTask);

    void onSubTaskBackTestDone(const std::shared_ptr<ShotTrader>& trader_ptr, const std::exception_ptr ex);

    std::unordered_map<AlgoOrderId_t, std::shared_ptr<ShotTrader>> subTraders;

    std::vector<std::pair<std::shared_ptr<AlgoOrder>, std::unordered_set<Symbol_t>>> subAlgoOrders;
};
