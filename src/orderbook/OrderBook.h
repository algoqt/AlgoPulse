#pragma once

#include "typedefs.h"
#include "common.h"

struct Order;
struct Trade;
class QuoteFeed;
class MarketDepth;

typedef std::function<void(const Order* )> OnOrderUpdateCallback;
typedef std::function<void(const Trade* )> OnTradeCallback;

using SubscribeOrderKey_t = std::variant<uint64_t, OrderBookKey_t>;

struct OrderBookRequest {

    OrderBookKey_t              orderBookKey;

    agcommon::QuoteMode         quoteMode;

    AsioContextPtr              runContextPtr;   // 回测时需与算法所在asioContext一致

    std::shared_ptr<QuoteFeed>  quoteFeedPtr{nullptr};    // 模拟/回测模式时不能为nullptr,
};

struct SubscribeOrder_t {

    SubscribeOrderKey_t     subscribeKey;

    OnOrderUpdateCallback   onOrderUpdate{ nullptr };

    OnTradeCallback         onTrade{ nullptr };

    AsioContextPtr          callBackContextPtr{ nullptr };
};

class OrderBook: public std::enable_shared_from_this<OrderBook> {

public:
    OrderBook(const OrderBookRequest& req) :m_request(req), m_running(false), m_strand(*req.runContextPtr) {};
    ~OrderBook() = default;
    OrderBook(const OrderBook&) = delete;
    OrderBook(OrderBook&&)      = delete;
    OrderBook& operator = (const OrderBook&) = delete;
    OrderBook& operator = (const OrderBook&&) = delete;

    virtual void run() =0;

    virtual void stop() = 0;

    virtual void release() = 0;

    inline bool isStopped() const { return not m_running; };

    inline bool isBackTest() const { return m_request.quoteMode == agcommon::QuoteMode::REPLAY; }

    virtual void onMarketDepth(MarketDepth* const md) = 0;

    virtual bool placeOrder(const Order* order) = 0;

    virtual bool cancelOrderByOrderId(const OrderId_t& orderId) = 0;

    virtual bool placeAlgoOrder(const Order* order, float executeDuration = 90, bool isPriceLimit = false)  { return true; };

    virtual bool cacelAlgoOrder(const OrderId_t& orderId) { return true; };

    virtual void onDelayTest(agcommon::TimeCost& delay) { };

    virtual SubscribeOrderKey_t subscribe(std::shared_ptr<SubscribeOrder_t> subPtr) { return subPtr->subscribeKey; };

    virtual void     unSubscribe(const SubscribeOrderKey_t& subcribeKey) {};

    virtual size_t  getOrderSize() = 0;

public:

    OrderBookRequest                m_request;

    std::atomic<bool>               m_running{ false };

    asio::io_context::strand        m_strand;
};



