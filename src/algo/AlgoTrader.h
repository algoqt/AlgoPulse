#pragma once

#include <memory>
#include "AlgoOrder.h"
#include "Algo.h"
#include "AlgoPerformance.h"
#include "common.h"
#include "waiting.h"
#include "IdGenerator.h"

class SecurityStaticInfo;
class QuoteFeed;
class OrderBook;

class AlgoTrader: public Trader {

public:

    AlgoTrader(const std::shared_ptr<AlgoOrder>& _algoOrderPtr
        , const AsioContextPtr& contextPtr = nullptr);

    ~AlgoTrader() = default;

    AlgoTrader(AlgoTrader& other)       = delete;
    AlgoTrader(AlgoTrader&&) noexcept   = delete;
    AlgoTrader& operator = (const AlgoTrader&)      = delete;
    AlgoTrader& operator = (AlgoTrader&&) noexcept  = delete;

    const std::shared_ptr<AlgoOrder>    algoOrderPtr;

    AlgoOrderId_t                       algoOrderId;

    const SecurityStaticInfo*           ssinfoPtr;

    std::string preStartCheck()             override;

    asio::awaitable<void> start()           override;

    void stop(const std::string& reason="") override;

    void cancel()           override;

    bool isStopped() const  override { return agcommon::AlgoStatus::isFinalStatus(algoPerf.algoStatus); }

    AlgoMsg::MsgAlgoCategory getAlgoCategory() const override { return algoOrderPtr->algoCategory; };

    AlgoOrderId_t  getAlgoOrderId() const override {
        if (algoOrderPtr)
            return algoOrderPtr->algoOrderId;
        return 0;
    };

    virtual std::shared_ptr<AlgoOrder>  getAlgoOrder() const override { return algoOrderPtr; };


    void onMarketDepth(MarketDepth* newMd) override;

    void onOrderUpdate(const Order* order) override;

    asio::awaitable<int> co_onMarketDepth(const MarketDepth* md);

    std::shared_ptr<AlgoMsg::MsgAlgoPerformance> encode2AlgoMessage() const;

    const AlgoPerformance& getAlgoPerf() const {
        return algoPerf;
    }

    void publishAlgoperformance() const override;

    void setQuoteFeed(const std::shared_ptr<QuoteFeed>& ptr);

    void setOrderBook(const std::shared_ptr<OrderBook>& ptr);

private:


    asio::awaitable<void> schedual();
    asio::awaitable<void> executeSlicer(Slicer& slicer);
    asio::awaitable<void> slicePolicy(Slicer& slicer);
    asio::awaitable<void> lastSlicePolicy(Slicer& slicer);

    enum class PolicyAction
    {
        MAKE,
        TAKE
    };

    void executeSignal(const MarketDepth* newMd, const MarketDepth* last_md);

    void slicePolicyOnSignal(const PolicyAction& action, const double price);

    double getPrice4Make(int qty);

    int32_t getQtyShinkImpact(int32_t qty2eat);

    double getPriceCover4Take(int qty, double coverRate);

    void placeOrder(const int qty, const double price,bool checkMinAmtPerOrder=true);

    void cancelOrder(const std::vector<OrderId_t>& orderIds);

    asio::awaitable<void> awaitMarketTime(double duration);

    asio::steady_timer    timer;

private:

    AlgoPerformance algoPerf{ algoOrderPtr.get(), ssinfoPtr};

    std::vector<Slicer> slicers{};
    int32_t curSlicerIndex{ -1 };

    OrderTime_t     startTime{};
    OrderTime_t     endTime{};

    double          algoDuration{ 0 };
    int             minQtyPerOrder{ 0 };
    double          algoExecProgressAllowedBias{ 0.1 };
    bool            cancelAllOrderFlag{ false };

    int mdcounts        { 0 };
    MarketDepthKeepAlivePtr  m_md;

    std::shared_ptr<OrderBook> orderBookPtr{ nullptr };
    std::shared_ptr<QuoteFeed>  quoteFeedPtr { nullptr };

    // back test only
    OrderTime_t         replayNextQuoteTime;
    Wait2Resume         wait2Resume;
};
