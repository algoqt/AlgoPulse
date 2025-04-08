#pragma once

#include "common.h"
#include "MarketDepth.h"
#include "EnableSelfHelper.h"

using AsioContextPtr = std::shared_ptr<asio::io_context>;

using OnMarketDepthCallback = std::function<void(const MarketDepth*)>;

using co_OnMarketDepthCallback = std::function<asio::awaitable<int>(const MarketDepth*)>;

using OnDelayTestCallback = std::function<void(agcommon::TimeCost& delay)>;

using SubscribeKey_t = AlgoOrderId_t;

struct SubMarketDepth_t {

    SubscribeKey_t                  subscribeKey;

    std::unordered_set<Symbol_t>    symbols{};

    OnMarketDepthCallback           onMarketDepth{ nullptr };

    co_OnMarketDepthCallback        co_onMarketDepth{ nullptr };

    const AcctKey_t                 acctKey{};

    bool                            publish2Client{ false };

    AsioContextPtr                  dispatchContextPtr{ nullptr };

    OnDelayTestCallback             onDelayTest{ nullptr };

};

struct QuoteFeedRequest {

    agcommon::QuoteMode             quoteMode;

    AlgoOrderId_t                   algoOrderId;

    std::unordered_set<Symbol_t>    symbolSet{};

    QuoteTime_t             startTime{ boost::posix_time::min_date_time }; // 只回测用
    QuoteTime_t             endTime{ boost::posix_time::min_date_time }; // 只回测用

    AsioContextPtr          dispatchContextPtr{ nullptr };
};


class QuoteFeed : public EnableSelfHelper<QuoteFeed> {

public:
    QuoteFeed(std::shared_ptr<asio::io_context> contextPtr, const QuoteTime_t& currentQuoteTime = agcommon::now())
        : m_contextPtr(contextPtr)
        , m_strand(contextPtr->get_executor())
        , lastTestQuoteTime(currentQuoteTime) {
    };
    virtual ~QuoteFeed() = default;

    virtual void run() = 0;

    virtual void stop() = 0;

    virtual bool isRuning() = 0;

    virtual SubscribeKey_t subscribe(std::shared_ptr<SubMarketDepth_t> subPtr) = 0;

    virtual void unSubscribe(const uint64_t subcribeKey) = 0;

    virtual void regesterOrderBookCallback(const OrderBookKey_t& orderBookKey
        , const OnMarketDepthCallback& onMarketDepth
        , const OnDelayTestCallback& onDelay = nullptr);

    virtual void unRegesterOrderBookCallback(const OrderBookKey_t& orderBookKey);

    virtual void regesterOnQuoteFeedStart(const std::function<void()>& f, const AsioContextPtr&) {};

    virtual void regesterOnQuoteFeedFinish(const std::function<void()>& f, const AsioContextPtr&);

    virtual QuoteTime_t getCurrentQuoteTime() { return agcommon::now(); };

    virtual double getVWAP(const Symbol_t& symobl
        , const QuoteTime_t& begTime
        , const QuoteTime_t& endTime) {    return 0;    };

    template<class T>
        requires std::is_base_of_v<QuoteFeed, T>
    inline void statDelay(agcommon::TimeCost& delay) {

        auto self = keep_alive_this<T>();

        if (agcommon::getSecondsDiff(self->lastTestQuoteTime, self->getCurrentQuoteTime()) > 60) {

            for (auto& [orderBookKey, onPair] : m_orderBookKey2CallBack) {

                auto& [onMarketDepth, onDelayStat] = onPair;

                if (onDelayStat) {
                    onDelayStat(delay);
                }
            }
            lastTestQuoteTime = self->getCurrentQuoteTime();
        }
    }

public:

    std::shared_ptr<asio::io_context>      m_contextPtr;

    strand_type                            m_strand;

    std::unordered_map<uint64_t, std::shared_ptr<SubMarketDepth_t>>        m_subscribeKey2SymbolCallBack;

    std::unordered_map<Symbol_t
        , std::unordered_map<uint64_t, std::shared_ptr<SubMarketDepth_t>>> m_symbol2SubscribeCallBack;

    std::unordered_map<OrderBookKey_t
        , std::pair<OnMarketDepthCallback, OnDelayTestCallback>
        , AcctKey_t::Hash>    m_orderBookKey2CallBack;


    std::vector<std::pair<std::function<void()>, AsioContextPtr>>      onQuoteFeedStarts;

    std::vector<std::pair<std::function<void()>, AsioContextPtr>>      onQuoteFeedFinisheds;

    QuoteTime_t  lastTestQuoteTime;

};