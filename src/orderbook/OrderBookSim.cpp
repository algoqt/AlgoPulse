#include "OrderBookSim.h"
#include "OrderService.h"
#include "QuoteFeedService.h"
#include "IdGenerator.h"
#include "StockDataManager.h"


OrderBookSim::OrderBookSim(const OrderBookRequest& req):
    OrderBook(req)
    , algoPlacerPtr(std::make_shared<AlgoPlacer>(req.runContextPtr)){

    if (req.quoteMode == agcommon::QuoteMode::SIMLIVE) {
        symbol2UnFinishedOrders.reserve(10000);
        localOrderId2Order.reserve(50000);
    }
};

void OrderBookSim::run() {

    if (m_running) return;

    m_running = true;

    if (m_request.quoteFeedPtr) {

        SPDLOG_INFO("{} OrderBookSim start.", m_request.orderBookKey);
        auto self = shared_from_this();

        OnDelayTestCallback onDelayTest = std::bind(&OrderBook::onDelayTest, self, std::placeholders::_1);;

        m_request.quoteFeedPtr->regesterOrderBookCallback(m_request.orderBookKey
            , std::bind(&OrderBook::onMarketDepth, self, std::placeholders::_1), onDelayTest);

        m_request.quoteFeedPtr->regesterOnQuoteFeedFinish(std::bind(&OrderBook::stop, self),m_request.runContextPtr);

    }

}
void OrderBookSim::stop() {

    if (isStopped()) {
        return;
    }
    auto task = [self = shared_from_this()] () {

        self->m_running = false;
        SPDLOG_INFO("[{}]OrderBookSim stop", self->m_request.orderBookKey.broker);
        self->m_request.quoteFeedPtr->unRegesterOrderBookCallback(self->m_request.orderBookKey);
        OrderService::getInstance().removeOrderBook(self->m_request);
        self->release();
     };

    asio::dispatch(m_strand, task);
}

void OrderBookSim::release() {

    if (isStopped()) {

        localOrderId2Order.clear();

        symbol2UnFinishedOrders.clear();

        m_mds.clear();
    }
}


SubscribeOrderKey_t OrderBookSim::subscribe(std::shared_ptr<SubscribeOrder_t> subPtr) {

    asio::dispatch(m_strand, [self = shared_from_this(), subPtr]() {self->_subscribe(subPtr); });

    return subPtr->subscribeKey;
}

SubscribeOrderKey_t OrderBookSim::_subscribe(std::shared_ptr<SubscribeOrder_t> subPtr) {

    bool isinserted = false;

    if (std::holds_alternative<uint64_t>(subPtr->subscribeKey)) {

        auto subscribeKey = std::get<uint64_t>(subPtr->subscribeKey);

        SPDLOG_INFO("aId:{} subscribe for orderUpdate", subscribeKey);

        auto [it, isinserted] = m_algoKey2OrderUpdateCallBack.insert({ subscribeKey,subPtr });
    }
    else {
        if(std::holds_alternative<OrderBookKey_t>(subPtr->subscribeKey)) {

            auto& subscribeKey = std::get<OrderBookKey_t>(subPtr->subscribeKey);

            SPDLOG_INFO("[aId:{}]subscribe orderUpdate", subscribeKey);

            auto [it, isinserted] = m_orderBookKey2OrderUpdateCallBack.insert({ subscribeKey,subPtr });
        }
    }
    if (isinserted) {
        return subPtr->subscribeKey;
    }

    return SubscribeOrderKey_t{0u};
}

void OrderBookSim::unSubscribe(const SubscribeOrderKey_t& subscribeKey) {

    asio::dispatch(m_strand, [self = shared_from_this(), subscribeKey]() {
        self->_unSubscribe(subscribeKey);
        });
}

void OrderBookSim::_unSubscribe(const SubscribeOrderKey_t& subscribeKey) {

    if (std::holds_alternative<uint64_t>(subscribeKey)) {

        auto int_subscribeKey = std::get<uint64_t>(subscribeKey);

        SPDLOG_INFO("[aId:{}]unsubscribe", int_subscribeKey);

        if (auto subPtrIt = m_algoKey2OrderUpdateCallBack.find(int_subscribeKey); subPtrIt != m_algoKey2OrderUpdateCallBack.end()) {
            m_algoKey2OrderUpdateCallBack.erase(subPtrIt);
        }
    }
    else {
        if (std::holds_alternative<OrderBookKey_t>(subscribeKey)) {

            auto& o_subscribeKey = std::get<OrderBookKey_t>(subscribeKey);

            SPDLOG_INFO("[aId:{}]unsubscribe", o_subscribeKey);

            if (auto subPtrIt = m_orderBookKey2OrderUpdateCallBack.find(o_subscribeKey); subPtrIt != m_orderBookKey2OrderUpdateCallBack.end()) {
                m_orderBookKey2OrderUpdateCallBack.erase(o_subscribeKey);
            }
        }
    }
}

std::shared_ptr<OrderBookSim> OrderBookSim::shared_from_this()
{
    return std::static_pointer_cast<OrderBookSim>(OrderBook::shared_from_this());
}

void OrderBookSim::addAcctAsset(const AcctKey_t& acctKey, AssetInfo& asset) {

    asio::dispatch(m_strand, [self = shared_from_this(), this, asset]() {

        acctAssetInfos.try_emplace(asset.acctKey, asset);
    });
}

void OrderBookSim::addPosition(const PositionInfo& position) {

    asio::dispatch(m_strand, [self = shared_from_this(), this, position]() {

        AcctKey_t acctKey = { position.acctType, position.acct, position.brokerId };

        SymbolKey_t symbolKey = { acctKey,position.symbol };

        if (acctAssetInfos.find(acctKey) == acctAssetInfos.end()) {
            double cash = SIMACCT_DEFAUL_CAPITAL;
            AssetInfo asset;
            asset.acctKey = acctKey;
            asset.enabledBalance = cash;
            asset.currentBalance = cash;

            addAcctAsset(acctKey, asset);
        }

        positionInfos.try_emplace(symbolKey, position);

        SPDLOG_INFO("Add New Position:{}" + position.to_string());
    });
}

void OrderBookSim::refreshAssetPosition(const Order& order, const Order& preOrder) {

    AcctKey_t   acctKey   = AcctKey_t(order.acctType ,order.acct, order.brokerId);

    SymbolKey_t symbolKey = std::make_pair(acctKey,order.symbol);

    AssetInfo&    ai =   acctAssetInfos[acctKey]; 

    PositionInfo& pi =   positionInfos[symbolKey];

    if (preOrder.orderId==order.orderId) {

        int     deltaqty = order.filledQty - preOrder.filledQty;
        double  deltaFilledAmt = order.filledAmt - preOrder.filledAmt;
        int     cancelqty =  order.cancelQty - preOrder.cancelQty;
        double deltafrozenAmt = 0;

        if (order.status == agcommon::OrderStatus::REJECTED){
            deltaqty = order.orderQty;
            deltafrozenAmt = order.orderQty * order.orderPrice;
            if (agcommon::isBuy(order.tradeSide)){
                ai.enabledBalance= ai.enabledBalance + deltafrozenAmt;
            } else{
                pi.enabledQty=pi.enabledQty + deltaqty;
            }
        }
        else{
        // 撤单及成交 可用资金 及 可用数量更新 
            if (agcommon::isBuy(order.tradeSide)){
                pi.enabledQty = pi.enabledQty + deltaqty;
                pi.currentQty = pi.currentQty + deltaqty;
                ai.enabledBalance = ai.enabledBalance + cancelqty * order.orderPrice;
            }else{
                ai.enabledBalance   = ai.enabledBalance + deltaFilledAmt;
                pi.enabledQty = pi.enabledQty + cancelqty;
                pi.currentQty = pi.currentQty - deltaqty;
            }
        }
        ai.updTime = agcommon::now();
        pi.updTime = ai.updTime;

    } else {
        double frozenAmt = 0;
        int frozenQty = 0;
        if (const auto it = m_mds.find(order.symbol);it!= m_mds.end()) {
            const auto& md = it->second;
            double frozenPrice = 0;
            if (order.orderPrice) {
                frozenPrice = order.orderPrice;
            } else {
                frozenPrice = std::max({md->bidPrice1, md->askPrice1, md->price});
            }
            if (frozenPrice == 0) {
                SPDLOG_ERROR(order.symbol + "0 Market Price."); 
            }
            if (agcommon::isBuy(order.tradeSide)) { 
                frozenAmt = frozenPrice * order.orderQty;
                ai.enabledBalance -= frozenAmt;
                ai.updTime = agcommon::now();
            } else {
                frozenQty = order.orderQty;
                pi.enabledQty -= frozenQty;
                pi.updTime = agcommon::now();
            }
        } else {
            SPDLOG_ERROR("[aId:{}]{} has not Subcribe MarketDepth yet.",order.algoOrderId,order.symbol);
        }
        // ... rest of the code
    }
    auto md = getMarketDepth(order.symbol);
    if(md) {
        pi.lastPrice = md->price;
        pi.mktValue = pi.currentQty * md->price;
    }
}

bool OrderBookSim::placeOrder(const Order* _order) {
    if (isStopped()) {
        return false;
    }
    auto order = Order::make_intrusive(*_order);

    asio::dispatch(m_strand,[self = shared_from_this(), order]() {  
        self->_placeOrder(order); });
    return true;
}

bool OrderBookSim::_placeOrder(const OrderPtr& order) {

    if (isStopped()) {
        return false;
    }
    auto md = getMarketDepth(order->symbol);

    SPDLOG_DEBUG("[aId:{}]localOrderId2Order.size:{}", order->algoOrderId, localOrderId2Order.size());
    try {
        if (localOrderId2Order.contains(order->orderId)) {
            order->status = agcommon::OrderStatus::REJECTED;
            if (auto it = m_algoKey2OrderUpdateCallBack.find(order->algoOrderId); it != m_algoKey2OrderUpdateCallBack.end()) {
                auto& subOrderUpdateInfo = it->second;
                subOrderUpdateInfo->onOrderUpdate(order.get());
            }
            if (auto it = m_orderBookKey2OrderUpdateCallBack.find(order->getOrderBookKey()); it != m_orderBookKey2OrderUpdateCallBack.end()) {
                auto& subOrderUpdateInfo = it->second;
                subOrderUpdateInfo->onOrderUpdate(order.get());
            }
            SPDLOG_ERROR("[ORDER_ERROR]OrderId EXIST:{}", order->orderId);
            return false;
        }
        else {
            uint32_t marketVolPeging = -1;
            if (md) {
                if (isBackTest()) {
                    order->orderTime = md->quoteTime ;
                    order->updTime   = md->quoteTime ;
                    order->quoteTime = md->quoteTime;
                }
                else {
                    order->quoteTime = md->quoteTime;
                }
                auto marketVolPeging = md->getPricezVol(order->orderPrice);
                SPDLOG_INFO("[OrderNew]{},bid1/ask1:{:.3f},{:.3f},qtime:{},pegVol:{},tradeVol:{}", order->to_string()
                    , md->bidPrice1, md->askPrice1
                    , agcommon::getTimeStr(md->quoteTime), marketVolPeging, md->deltaVolume);
            }

            auto& _unfinishs = symbol2UnFinishedOrders[order->symbol];

            auto it = _unfinishs.emplace_hint(_unfinishs.end(), order->orderId, std::pair(order, marketVolPeging));

            localOrderId2Order.emplace(order->orderId, std::pair(order, it));

            if (md)
            {
                Order preOrder{};

                refreshAssetPosition(*order, preOrder);

                simOrderMatchFill(it, md);  // 如果订单是主动单,模拟及时成交

                if (it->second.first->isFinalStatus()) {

                    _unfinishs.erase(it);
                }
            }

            return true;
        }
    }
    catch (std::exception& e) {
        SPDLOG_ERROR("[ORDER_ERROR]OrderId EMPTY!{},{}", order->to_string(), e.what());
    }
    return false;
}

bool OrderBookSim::placeAlgoOrder(const Order* _order, float executeDuration /*= 90*/, bool isPriceLimit /*= false*/) {

    if (isStopped()) {
        return false;
    }

    asio::dispatch(m_strand, 
        [self = shared_from_this(), order = Order::make_intrusive(*_order),executeDuration, isPriceLimit]() {

            if (auto it = self->m_algoKey2OrderUpdateCallBack.find(order->algoOrderId); it != self->m_algoKey2OrderUpdateCallBack.end()) {

                if (it->second->onOrderUpdate) {
                    self->algoPlacerPtr->placeAlgoOrder(order.get(),executeDuration,isPriceLimit
                        ,it->second->onOrderUpdate
                        ,it->second->callBackContextPtr);
                }
            }
        }
        );

    return true;
}

bool OrderBookSim::cacelAlgoOrder(const OrderId_t& orderId) {

    algoPlacerPtr->cancelAlgoOrder(orderId);
    
    return true;
}

void OrderBookSim::onDelayTest(agcommon::TimeCost& delay) {

    auto self = shared_from_this();
    asio::post(m_strand, [self,this, delay]() mutable {

        size_t totals = localOrderId2Order.size();

        size_t fills=0, cancels = 0, finals = 0;

        for (auto& [orderId,order_pair] : localOrderId2Order) {
            auto& order = order_pair.first;
            if (order->isFinalStatus()) {
                finals++;
                if (order->status == agcommon::OrderStatus::FILLED)   { fills++; }
                if (order->status == agcommon::OrderStatus::CANCELED) { cancels++; }
            }
		}
        std::string suftext = fmt::format("orders:{},finals:{},fills:{},cancels:{},pendings:{}", totals, finals, fills, cancels, totals - finals);

        delay.logTimeCost(suftext);
    });
}


bool OrderBookSim::cancelOrderByOrderId(const OrderId_t& orderId) {
    if (isStopped()) {
        return false;
    }
    asio::dispatch(m_strand, [self = shared_from_this(), orderId]() {  self->_cancelOrderWithOrderId(orderId); });
    return true;
}

bool OrderBookSim::_cancelOrderWithOrderId(const OrderId_t& orderId){
    if (isStopped()) {
        return false;
    }

    SPDLOG_DEBUG("*Cancel*orderId:{}", orderId);

    if (auto it = localOrderId2Order.find(orderId);it != localOrderId2Order.end()) {

        auto& order = it->second.first;

        if (agcommon::OrderStatus::isFinalStatus(order->status)) {
            return false;
        }

        SPDLOG_INFO("[OrderCancel][{}]symbol:{},qty:{},filled:{}",order->symbol, order->orderId, order->orderQty, order->filledQty);

        if (not agcommon::OrderStatus::isFinalStatus(order->status)) {
            Order preOrder = *order;
            if (order->filledQty == 0) {
                order->status = agcommon::OrderStatus::CANCELED;
            }
            else {
                order->status = agcommon::OrderStatus::PARTIALCANCEL;
                order->avgPrice = order->filledAmt / order->filledQty;
            }

            order->cancelQty = order->orderQty - order->filledQty;
            auto md = getMarketDepth(order->symbol);

            if (md and isBackTest()) {
                order->updTime = md->quoteTime;
                //orderIt->second.first = order;
            }

            _onOrderUpdate(order.get(), &preOrder);

            auto& _unfinishs = symbol2UnFinishedOrders[order->symbol];
            auto& _local_it = it->second.second;

            if (order->isFinalStatus()) {
                _unfinishs.erase(it->second.second);
                
            }
            return true;
        }
    }
    return false;
}

void OrderBookSim::_onOrderUpdate(Order* order, const Order* preOrder) {

    refreshAssetPosition(*order, *preOrder);

    _onOrderUpdate(order);
}

void OrderBookSim::_onOrderUpdate(Order* order) {

    if (auto it = m_algoKey2OrderUpdateCallBack.find(order->algoOrderId); it != m_algoKey2OrderUpdateCallBack.end()) {

        auto& subcribeInfo = it->second;

        if (subcribeInfo->onOrderUpdate) {
            if (subcribeInfo->callBackContextPtr) {

                asio::dispatch(*subcribeInfo->callBackContextPtr, [subcribeInfo, orderPtr = boost::intrusive_ptr(order)]() {
                    subcribeInfo->onOrderUpdate(orderPtr.get()); });
            }
            else {
                subcribeInfo->onOrderUpdate(order);
            }
        }
    }

    if (auto it = m_orderBookKey2OrderUpdateCallBack.find(order->getOrderBookKey()); it != m_orderBookKey2OrderUpdateCallBack.end()) {

        auto& subcribeInfo = it->second;

        if (subcribeInfo->onOrderUpdate) {
            if (subcribeInfo->callBackContextPtr) {

                asio::dispatch(*subcribeInfo->callBackContextPtr, [subcribeInfo, orderPtr = boost::intrusive_ptr(order)]() {
                    subcribeInfo->onOrderUpdate(orderPtr.get()); });
            }
            else {
                subcribeInfo->onOrderUpdate(order);
            }
        }
    }

    OrderService::getInstance().onOrderUpdate(order);
}

void OrderBookSim::_onTrade(Trade* trade) {

    if (auto it = m_algoKey2OrderUpdateCallBack.find(trade->algoOrderId); it != m_algoKey2OrderUpdateCallBack.end()) {

        auto& subcribeInfo = it->second;

        if (subcribeInfo->onTrade) {

            if (subcribeInfo->callBackContextPtr) {

                asio::dispatch(*subcribeInfo->callBackContextPtr, [subcribeInfo, tradePtr=boost::intrusive_ptr(trade) ]() {
                    subcribeInfo->onTrade(tradePtr.get()); });
            }
            else {
                subcribeInfo->onTrade(trade);
            }
        }
    }

    if (auto it = m_orderBookKey2OrderUpdateCallBack.find(trade->getOrderBookKey()); it != m_orderBookKey2OrderUpdateCallBack.end()) {

        auto& subcribeInfo = it->second;

        if (subcribeInfo->onTrade) {

            if (subcribeInfo->callBackContextPtr) {

                asio::dispatch(*subcribeInfo->callBackContextPtr, [subcribeInfo, tradePtr = boost::intrusive_ptr(trade)]() {
                    subcribeInfo->onTrade(tradePtr.get()); });
            }
            else {
                subcribeInfo->onTrade(trade);
            }
        }
    }

    OrderService::getInstance().onTrade(trade);
}

/* run on m_strand*/
void OrderBookSim::onMarketDepth(MarketDepth* md) {

    if (isStopped())
        return;

    asio::dispatch(m_strand, [self=shared_from_this(), md = MarketDepthKeepAlivePtr(md)]() {

        //agcommon::TimeCost tc(fmt::format("{},{}",agcommon::getDateTimeStr(md->quoteTime), md->price));

        self->m_mds[md->symbol] = md;

        self->simOrderMatchFill(md.get());
    });
}

void OrderBookSim::_onMarketDepth(MarketDepth* new_md) {
    if (isStopped())
        return;
    SPDLOG_DEBUG("orderBook onMarketDepth:{}", new_md->to_string());
    new_md->retainAlive();
    auto it = m_mds.find(new_md->symbol);
    if (it != m_mds.end()) {
        it->second->release();
        it->second = new_md;
    }
    else {
        m_mds[new_md->symbol] = new_md;
    }
    simOrderMatchFill(new_md);
}

MarketDepth* OrderBookSim::getMarketDepth(const Symbol_t& symbol) {

    if (auto it = m_mds.find(symbol);it != m_mds.end()) {
        return it->second.get();
    }
    return nullptr;
}


void OrderBookSim::simOrderMatchFill(const MarketDepth* const md) {

    if (not agcommon::AshareMarketTime::isInMarketContinueTradingTime(md->quoteTime)) {
        return;
    }

    auto& _unfinishs = symbol2UnFinishedOrders[md->symbol];

    //SPDLOG_INFO("[{}] ORDERS SIZE:{}", md->symbol, _unfinishs.size());
    if (_unfinishs.empty()) {

        for(auto& [symbolKey,pi] : positionInfos){
            if (pi.symbol == md->symbol) {
                pi.lastPrice = md->price;
                pi.mktValue = pi.currentQty * md->price;
            }
        }
		return;
	}

    for (auto it = _unfinishs.begin(); it != _unfinishs.end();) {

        simOrderMatchFill(it, md);

        if (it->second.first->isFinalStatus()) {

            _unfinishs.erase(it++);

        }
        else {
            it++;
        }
    }
}

void OrderBookSim::simOrderMatchFill(UnFinishedOrderMap::iterator& it, const MarketDepth* const md) {

    const OrderId_t& orderId = it->first;

    OrderPtr        preOrder = it->second.first;

    auto& queueVol = it->second.second;

    auto order  = Order::make_intrusive(*preOrder);

    Symbol_t& symbol = order->symbol;

    int tradeVol = md->deltaVolume;

    auto marketVolPeging = md->getPricezVol(order->orderPrice);

    if (queueVol == -1) {
        auto marketVolPeging = md->getPricezVol(order->orderPrice);
        queueVol = marketVolPeging > 0 ? marketVolPeging : -1;

    }

    if (order->status == agcommon::OrderStatus::UNSENT || order->status == agcommon::OrderStatus::SENTUNKNOWN ||
        order->status == agcommon::OrderStatus::SENTOK || order->status == agcommon::OrderStatus::REPORTING) {
        order->status = agcommon::OrderStatus::NEW;
    }
    if (order->orderPrice == 0) {
        order->status = agcommon::OrderStatus::REJECTED;
        SPDLOG_ERROR("[{},{}]orderId:{},price:0,askPrice1:{:.3f},bidPrice1:{:.3f},qt:{}", order->acctType, order->acct, order->orderId,md->askPrice1,md->bidPrice1,agcommon::getTimeStr(md->quoteTime));
    }

    auto trade = Trade::make_intrusive();
    trade->tradeId = TradeIdGenerator::getInstance().NewId();
    trade->orderId = orderId;
    trade->acctType = order->acctType;
    trade->acct     = order->acct;
    trade->symbol    = order->symbol;
    trade->tradeSide = order->tradeSide;
    trade->price = 0;
    trade->filledQty = 0;
    trade->filledAmt = 0;
    trade->algoOrderId = order->algoOrderId;
    trade->filledTime = isBackTest() ? md->quoteTime : agcommon::now();

    if (agcommon::isBuy(order->tradeSide)) {

        bool isAggressFilled = order->orderPrice >= md->askPrice1 && md->askPrice1 > 0;

        bool isPegFilled = order->orderPrice < md->askPrice1 && md->bidPrice1 > 0 
            && (order->orderPrice - md->bidPrice1)>-0.00001 && (-0.00001 <= order->orderPrice - md->price);
        
        SPDLOG_DEBUG("[aId:{}]{},isAggressFilled:{},isPegFilled:{},orderPrice:{},{}/{},fillq:{}", order->algoOrderId, order->orderId, isAggressFilled, isPegFilled
            ,order->orderPrice, md->bidPrice1, md->askPrice1, order->filledQty);
        
        if (isAggressFilled) {
            auto [matchQty,matchAmout] = md->tryMathWithinPrice(agcommon::QuoteSide::Ask, order->orderPrice, order->orderQty-order->filledQty);
            trade->filledQty = matchQty;
            trade->filledAmt = matchAmout;
            trade->price     = matchAmout / matchQty;
            order->filledQty += trade->filledQty;
            if (order->filledQty == order->orderQty) {
                order->status = agcommon::OrderStatus::FILLED;
            }
        }
        else {
            if (isPegFilled) {

                queueVol = queueVol - tradeVol;

                if (queueVol < -100) {
                    trade->filledQty = std::min(std::abs(queueVol), order->orderQty - order->filledQty);
                    trade->price     = order->orderPrice;
                    trade->filledAmt = trade->price * trade->filledQty;
                    order->filledQty += trade->filledQty;
                    queueVol = 0;
                    if (order->filledQty >= order->orderQty) {
                        order->status = agcommon::OrderStatus::FILLED;
                        order->filledQty = order->orderQty;
                    }
                    else {
                        if (order->filledQty > 0) {
                            order->status = agcommon::OrderStatus::PARTIALFILLED;
                        }
                    }
                }
            }
        }
        order->filledAmt += trade->filledAmt;
        if (order->filledQty > 0) {
            order->avgPrice = order->filledAmt / order->filledQty;
        }
    }

    else {

        auto isAggressFilled = order->orderPrice <= md->bidPrice1 && order->orderPrice > 0;
        auto isPegFilled = order->orderPrice > md->bidPrice1 && order->orderPrice <= md->askPrice1 && (md->price - order->orderPrice >= 0.00001);
        SPDLOG_DEBUG("[aId:{}]{},isAggressFilled:{},isPegFilled:{},orderPrice:{},{}/{},fillq:{}", order->algoOrderId, order->orderId, isAggressFilled, isPegFilled
            , order->orderPrice, md->bidPrice1, md->askPrice1, order->filledQty);
        if (isAggressFilled){
            auto [matchQty, matchAmout] = md->tryMathWithinPrice(agcommon::QuoteSide::Bid, order->orderPrice, order->orderQty - order->filledQty);
            trade->filledQty = matchQty;
            trade->filledAmt = matchAmout;
            trade->price = matchAmout / matchQty;
            order->filledQty += trade->filledQty;
            if (order->filledQty == order->orderQty) {
                order->status = agcommon::OrderStatus::FILLED;
            }
        }
        else  {
            if (isPegFilled) {
                queueVol = queueVol - tradeVol;

                if (queueVol < -100) {
                    trade->filledQty = std::min(std::abs(queueVol), order->orderQty - order->filledQty);
                    trade->price     = order->orderPrice;
                    trade->filledAmt = trade->price * trade->filledQty;
                    order->filledQty += trade->filledQty;
                    queueVol = 0;
                    if (order->filledQty >= order->orderQty) {
                        order->status = agcommon::OrderStatus::FILLED;
                        order->filledQty = order->orderQty;
                    }
                    else {
                        if (order->filledQty > 0) {
                            order->status = agcommon::OrderStatus::PARTIALFILLED;
                        }
                    }
                }
            }
        }

        order->filledAmt += trade->filledAmt;
        if (order->filledQty > 0) {
            order->avgPrice = order->filledAmt / order->filledQty;
        }
    }
    if (queueVol > 0) {
        auto marketVolPeging = md->getPricezVol(order->orderPrice);
        if(marketVolPeging >0)
            queueVol = std::min(marketVolPeging, queueVol);
    }
    SPDLOG_DEBUG("[aId:{}]{},ordPrice:{:.3f},tradePrice:{:.3f},tradeVol:{},qr:{},tradeFilledQty:{}"
        , order->algoOrderId,order->orderId,order->orderPrice,md->price
        ,tradeVol, queueVol, trade->filledQty);
    
    if (order->isNewerThan(*preOrder)) {

        order->updTime = isBackTest() ? md->quoteTime : agcommon::now();

        it->second.first = order;

        localOrderId2Order[order->orderId] = std::pair(order, it);

        _onOrderUpdate(order.get(), preOrder.get());

        if (trade->filledQty > 0) {

            SPDLOG_DEBUG("[OrderMatch]{}" + order->to_string());

            _onTrade(trade.get());
        }

    }

}
