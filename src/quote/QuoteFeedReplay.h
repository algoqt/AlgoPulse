#pragma once

#include "QuoteFeed.h"
#include "MappedFileManager.h"
/*
* QuoteFeedReplay should dispatch with context run only in one thread, so it equal run with stand
*/
class QuoteFeedReplay : public QuoteFeed {

public:
    QuoteFeedReplay(const QuoteFeedRequest& req);

    ~QuoteFeedReplay() { SPDLOG_INFO("Free QuoteFeedReplay:{}", m_request.algoOrderId); };

    void run() override;

    asio::awaitable<void> co_run();

    void stop() override;

    bool isRuning() override { return m_keepRuning.load(); };

    SubscribeKey_t subscribe(std::shared_ptr<SubMarketDepth_t> subPtr) override;

    void unSubscribe(const uint64_t subcribeKey) override;

    QuoteTime_t getCurrentQuoteTime() override { return currentQuoteTime.load(); };

    double getVWAP(const Symbol_t& symobl, const QuoteTime_t& begTime, const QuoteTime_t& endTime) override;

public:

    std::map<QuoteTime_t, UnorderMarketDepthRawPtrMap>           m_quoteTime2Symbol2md = {};

private:

    QuoteFeedRequest                           m_request{};

    std::atomic<bool>                          m_keepRuning{ false };

    UnorderMarketDepthRawPtrMap                m_symbol2md;

    uint64_t _subscribe(std::shared_ptr<SubMarketDepth_t> subPtr);

    void     _unSubscribe(const uint64_t subcribeKey);

    std::atomic<QuoteTime_t>                    currentQuoteTime;
};
