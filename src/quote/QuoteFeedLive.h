#pragma once

#include "QuoteFeed.h"
#include "QuoteFeedInternet.h"

/* implement your own realtime QuoteFeed */
//
//class QuoteFeedLive : public QuoteFeed {
//
//public:
//
//    QuoteFeedLive(const QuoteFeedRequest& req):m_request(req),QuoteFeed(req.dispatchContextPtr){};
//
//    ~QuoteFeedLive() = default;
//
//    void run() override   {};
//
//    void stop() override  {};
//
//    bool isRuning() override { return false; };
//
//    SubscribeKey_t subscribe(std::shared_ptr<SubMarketDepth_t> subPtr) override { return SubscribeKey_t(); };
//
//    void unSubscribe(const uint64_t subcribeKey) override {};
//
//    std::shared_ptr<QuoteFeedLive> shared_from_this() {
//
//        return std::static_pointer_cast<QuoteFeedLive>(QuoteFeed::shared_from_this());
//    }
//
//private:
//
//    QuoteFeedRequest                       m_request{};
//
//    std::atomic<bool>                      m_keepRuning{ false };
//};

using QuoteFeedLive = QuoteFeedInternet;