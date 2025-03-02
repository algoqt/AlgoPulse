#pragma once

#include "typedefs.h"
#include "common.h"
#include "PositionInfo.h"
#include "AssetInfo.h"
#include "Order.h"
#include "MarketDepth.h"
#include "OrderBook.h"
#include "AlgoPlacer.h"

constexpr int64_t SIMACCT_DEFAUL_CAPITAL = 5'000'000;

class OrderBookSim : public OrderBook {

public:
    OrderBookSim(const OrderBookRequest& req);
    ~OrderBookSim(){  SPDLOG_DEBUG("DECONSTRUCT OrderBookSim:{}", m_request.orderBookKey);}

    OrderBookSim(const OrderBookSim&) = delete;
    OrderBookSim(OrderBookSim&&) = delete;
    OrderBookSim& operator = (const OrderBookSim&) = delete;
    OrderBookSim& operator = (const OrderBookSim&&) = delete;

    void stop() override;

    void run() override;

    void release() override;

    SubscribeOrderKey_t subscribe(std::shared_ptr<SubscribeOrder_t> subPtr) override;

    void unSubscribe(const SubscribeOrderKey_t& subcribeKey) override;

    void onMarketDepth(MarketDepth* md) override;

    void addPosition(const PositionInfo& position);

    void addAcctAsset(const AcctKey_t& acctKey, AssetInfo& asset);

    bool placeOrder(const Order* order)   override;

    bool cancelOrderByOrderId(const OrderId_t& orderId) override;

    void onDelayTest(agcommon::TimeCost& delay) override;

    bool placeAlgoOrder(const Order* order, float executeDuration = 90, bool isPriceLimit = false) override;

    bool cacelAlgoOrder(const OrderId_t& orderId) override;

    size_t  getOrderSize() override { return localOrderId2Order.size(); };

private:

    MarketDepth* getMarketDepth(const Symbol_t& symbol);

    std::map<PositionKey_t, std::map<Symbol_t, PositionInfo>> init_positions{};

    std::map<SymbolKey_t, PositionInfo> positionInfos{};

    std::map<AcctKey_t, AssetInfo>      acctAssetInfos{};

    UnorderMarketDepthPtrMap            m_mds{};

    std::map<std::string, std::function<void(const Order*)>>              routerKey2OnOrderUpdate{};

    std::unordered_map<uint64_t, std::shared_ptr<SubscribeOrder_t>>       m_algoKey2OrderUpdateCallBack;

    std::unordered_map<OrderBookKey_t, std::shared_ptr<SubscribeOrder_t>, OrderBookKey_t::Hash> m_orderBookKey2OrderUpdateCallBack;

    using OrderPtr = boost::intrusive_ptr<Order>;

    using UnFinishedOrderMap = std::map<OrderId_t, std::pair<OrderPtr, int32_t>>;  // won't relocate

    std::unordered_map<Symbol_t, UnFinishedOrderMap>                   symbol2UnFinishedOrders{};

private:
    std::unordered_map<OrderId_t, std::pair<OrderPtr, UnFinishedOrderMap::iterator>>       localOrderId2Order{};

    std::shared_ptr<AlgoPlacer>  algoPlacerPtr;

    std::shared_ptr<OrderBookSim> shared_from_this();

    void refreshAssetPosition(const Order& order, const Order& preOrder);

    void simOrderMatchFill(const MarketDepth* const md);

    void simOrderMatchFill(UnFinishedOrderMap::iterator& it, const MarketDepth* const md);

    bool _placeOrder(const OrderPtr& order);

    bool _cancelOrderWithOrderId(const OrderId_t& orderId);

    void _onOrderUpdate(Order* order, const Order* preOrder);

    void _onOrderUpdate(Order* order);

    void _onTrade(Trade* trade);

    SubscribeOrderKey_t _subscribe(std::shared_ptr<SubscribeOrder_t> subPtr);

    void     _unSubscribe(const SubscribeOrderKey_t& subcribeKey);

    void     _onMarketDepth(MarketDepth* md);

    //std::shared_mutex m_mutex;
};

using OrderBookSimCust = OrderBookSim;

using OrderBookLive    = OrderBookSim;    // adator to broker



