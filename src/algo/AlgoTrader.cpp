#include "AlgoTrader.h"
#include "StockDataManager.h"
#include "OrderService.h"
#include "TCPSession.h"
#include "MarketDepth.h"

using AlgoStatus        = agcommon::AlgoStatus;
using AshareMarketTime  = agcommon::AshareMarketTime;

AlgoTrader::AlgoTrader(const std::shared_ptr<AlgoOrder>& _algoOrderPtr
    , const AsioContextPtr& c):

     Trader(c ? c : ContextService::getInstance().getHashedWorkerContext(_algoOrderPtr->symbol))
    , algoOrderPtr(_algoOrderPtr)
    , algoOrderId(_algoOrderPtr->algoOrderId)
    , ssinfoPtr(nullptr)
    , algoPerf{ _algoOrderPtr.get(), ssinfoPtr }

    , timer(*contextPtr)
    , wait2Resume(*contextPtr, _algoOrderPtr->algoOrderId)
    , curSlicerIndex(-1)
    , startTime(_algoOrderPtr->startTime)
    , endTime(_algoOrderPtr->endTime)
    , m_md(MarketDepth::create(algoOrderPtr->symbol), false)
    , replayNextQuoteTime(agcommon::now()) {
}


void AlgoTrader::setQuoteFeed(const std::shared_ptr<QuoteFeed>& ptr) {

    if (quoteFeedPtr != nullptr)
        return;

    if (ptr)
        quoteFeedPtr = ptr;

}

void AlgoTrader::setOrderBook(const std::shared_ptr<OrderBook>& ptr) {

    if (orderBookPtr != nullptr)
        return;

    if (ptr)
        orderBookPtr = ptr;
}


std::string AlgoTrader::preStartCheck() {

    auto& dataSource = StockDataManager::getInstance();

    auto now = agcommon::now();

    auto isBackTest    = algoOrderPtr->isBackTestOrder();
    auto isNotBackTest = not isBackTest;


    auto dtint = isNotBackTest ? agcommon::getDateInt(now) : agcommon::getDateInt(algoOrderPtr->startTime);

    if (algoOrderPtr->qtyTarget <= 0 ||  algoOrderPtr->acct.empty()) {

        return "Params Should NOT EMPTY,qtyTarget:" + std::to_string(algoOrderPtr->qtyTarget) +
            ",acctType:" + AlgoMsg::AcctType_Name(algoOrderPtr->acctType) + ",acct:" + algoOrderPtr->acct;
    }

    ssinfoPtr = dataSource.getSecurityStaticInfo(algoOrderPtr->symbol, dtint);

    if (not ssinfoPtr) {

        auto algoErrMessage = std::format("[aId:{}]{} at {},ssinfo NOT EXISTS", algoOrderPtr->algoOrderId, algoOrderPtr->symbol, dtint);
        return algoErrMessage;
    }

    algoPerf.ssinfoPtr = ssinfoPtr;

    try{
        if (ssinfoPtr->isSuspended) {
            return algoOrderPtr->symbol + " is Suspended." ;
        }

        if (not agcommon::TradeSide::isValid(algoOrderPtr->tradeSide)) {
            return std::format("tradeSide {} is not valid.",algoOrderPtr->tradeSide);
        }
        if (algoPerf.isBuy){
            if (algoOrderPtr->qtyTarget < ssinfoPtr->orderInitQtySize)
                return "qyt Target Should Greater than " + std::to_string(ssinfoPtr->orderInitQtySize);
        }

        if (algoOrderPtr->execDuration <= 0) {
            if (algoOrderPtr->startTime >= algoOrderPtr->endTime) {
                return "startTime:" + agcommon::getDateTimeStr(algoOrderPtr->startTime) + "Should Greater Than endTime:" + agcommon::getDateTimeStr(algoOrderPtr->endTime);
            }
        }

        startTime = AshareMarketTime::getAvailableStartMarketTime(algoOrderPtr->startTime);
        endTime   = algoOrderPtr->endTime;

        if (startTime != algoOrderPtr->startTime) {
            SPDLOG_INFO("[aId:{}]startTime:{} adjust to available marketTime: {}", algoOrderPtr->algoOrderId
                , agcommon::getDateTimeInt(algoOrderPtr->startTime)
                , agcommon::getDateTimeInt(startTime));
        }

        if (startTime > AshareMarketTime::getClosingCallAuctionBeginTime(startTime)) {
            return std::format("startTime:{} should not greater than Closing Call Auction Begin Time", agcommon::getDateTimeInt(startTime));
        }

        if (isNotBackTest) {

            if (algoOrderPtr->execDuration > 0 ) {
                startTime = AshareMarketTime::getAvailableStartMarketTime();
                endTime   = AshareMarketTime::addMarketDuration(startTime, algoOrderPtr->execDuration);
                algoDuration = algoOrderPtr->execDuration;
            }

            if (agcommon::getDateInt(startTime) != agcommon::getDateInt(now)
                    or agcommon::getDateInt(endTime) != agcommon::getDateInt(endTime)) {
                return std::format("startTime/endTime should be in today {},{}", agcommon::getDateTimeInt(startTime), agcommon::getDateTimeInt(startTime));
            }

            if (endTime <= now) {
                return std::format("endTime:{} should not greater than now: {}", agcommon::getDateTimeInt(endTime), agcommon::getDateTimeInt(now));
            }

            if (startTime < agcommon::addDuration(now, -3)) {
                startTime = now;
                SPDLOG_WARN("[aId:{}]startTime:{} adjust to now: {}", algoOrderPtr->algoOrderId
                    , agcommon::getDateTimeInt(startTime)
                    , agcommon::getDateTimeInt(now));
            }
        }

        if (endTime > AshareMarketTime::getClosingCallAuctionBeginTime(startTime)) {
            endTime = AshareMarketTime::getClosingCallAuctionBeginTime(startTime);
        }
        if (AshareMarketTime::isAfternoonBreakTime(endTime)) {
            endTime = AshareMarketTime::getMarketMorningCloseTime(startTime);
        }

        algoDuration = AshareMarketTime::getMarketDuration(startTime, endTime);

        algoPerf.startTime = startTime;
        algoPerf.endTime   = endTime;

        minQtyPerOrder = ssinfoPtr->lowLimitPrice > 0 ? (int)(algoOrderPtr->minAmountPerOrder / ssinfoPtr->lowLimitPrice) : 0;

        if (isBackTest) {

            replayNextQuoteTime = startTime;
            SPDLOG_INFO("[aId:{}]init replayNextQuoteTime:{}",algoOrderPtr->algoOrderId, agcommon::getDateTimeInt(replayNextQuoteTime));
        }

    } catch (const std::exception& e) {
        return e.what();
    }

    return "";
}

asio::awaitable<void> AlgoTrader::start() {

    if (hasStarted) {
        co_return;
    }
    hasStarted = true;

    algoPerf.errMsg = AlgoTrader::preStartCheck();

    if (algoPerf.errMsg != "") {
        SPDLOG_ERROR("[aId:{}]preStartCheck error:{}", algoOrderId, algoPerf.errMsg);
        stop(algoPerf.errMsg);
        co_return;
    }

    SPDLOG_INFO("[aId:{}]AlgoTrader Start", algoOrderId);

    algoPerf.algoStatus = AlgoStatus::Running;

    if (quoteFeedPtr == nullptr) {

        QuoteFeedRequest df_req(algoOrderPtr->getQuoteMode(), algoOrderPtr->algoOrderId, { algoOrderPtr->symbol }, startTime, endTime, contextPtr);

        quoteFeedPtr = QuoteFeedService::getInstance().createQuoteFeed(df_req, algoPerf.errMsg);

        if (quoteFeedPtr == nullptr) {

            SPDLOG_ERROR("[aId:{}]AlgoTrader setQuoteFeed error:{}", algoOrderId, algoPerf.errMsg);
            stop(algoPerf.errMsg);
            co_return;
        }
    }

    if (orderBookPtr == nullptr) {

        OrderBookRequest orderRouter_req(algoOrderPtr->getOrderBookKey(), algoOrderPtr->getQuoteMode(), contextPtr, quoteFeedPtr);

        orderBookPtr = OrderService::getInstance().createOrderBookIfNotExist(orderRouter_req, algoPerf.errMsg);

        if (orderBookPtr == nullptr) {
            SPDLOG_ERROR("[aId:{}]AlgoTrader setOrderBook error:{}", algoOrderId, algoPerf.errMsg);
            stop(algoPerf.errMsg);
            co_return;
        }
    }
    
    auto self = keep_alive_this<AlgoTrader>();

    auto onOrderUpdate  = [self](const Order* order) { self->onOrderUpdate(order); };
    auto onTrade        = nullptr;

    auto onMarketDepth = [self](MarketDepth* md) { self->onMarketDepth(md); };
    co_OnMarketDepthCallback co_onMarketDepth = [self](MarketDepth* md) -> asio::awaitable<int> { co_return co_await self->co_onMarketDepth(md); };

    auto subscribeOrderUpdate = std::make_shared<SubscribeOrder_t>(algoOrderId, onOrderUpdate, onTrade,contextPtr);

    if (orderBookPtr->subscribe(subscribeOrderUpdate) != subscribeOrderUpdate->subscribeKey) {

        SPDLOG_ERROR("[aId:{}]subscribe error.", algoOrderId);

        stop("orderBook subscribe failed.");

        co_return;
    }

    auto subscribeMarketDepth = std::make_shared<SubMarketDepth_t>(algoOrderId
        , std::unordered_set<Symbol_t>{algoOrderPtr->symbol}
        , onMarketDepth
        , co_onMarketDepth
        , algoOrderPtr->getAcctKey()
        , false
        ,contextPtr);
    auto result = quoteFeedPtr->subscribe(subscribeMarketDepth);
    if (result != algoOrderId) {

        SPDLOG_ERROR("[aId:{}]subscribe error,subscribe key:{}.", algoOrderId, result);
        stop("dataFeed subscribe failed.");
        co_return;
    }

    quoteFeedPtr->regesterOnQuoteFeedFinish( [self]() { self->stop(); }, contextPtr );

    try{

        co_await schedual();

        stop();
    }
    catch (std::exception& e) {
        SPDLOG_ERROR("[aId:{}]AlgoTrader Exception,{}", algoOrderId,e.what());
        throw e;
    }
}

void AlgoTrader::stop(const std::string& reason) {

    if (AlgoStatus::isFinalStatus(algoPerf.algoStatus))
        return;
    
    if (!AlgoStatus::isFinalStatus(algoPerf.algoStatus)) {

        if (algoPerf.qtyFilled >= algoPerf.qtyTarget) {
            algoPerf.algoStatus = AlgoStatus::Finished;
            algoPerf.errMsg = "";
        }
        else {
            if (algoPerf.algoStatus == AlgoStatus::Canceling) {
                algoPerf.algoStatus = AlgoStatus::Terminated;
                algoPerf.errMsg = "USER CANCEL.";
            }
            else {
                if (not reason.empty()) {
                    algoPerf.algoStatus = AlgoStatus::Error;
                    algoPerf.errMsg = reason;
                }
                else {
                    algoPerf.algoStatus = AlgoStatus::Expired;
                }
            }
        }
    }

    wait2Resume.clear();

    SPDLOG_INFO("{},mds:{},last md:{}", algoPerf.to_string(), mdcounts,agcommon::getDateTimeInt(m_md->quoteTime));
    
    if (quoteFeedPtr) {
        quoteFeedPtr->unSubscribe(algoOrderId);
    }

    if (orderBookPtr) {
        orderBookPtr->unSubscribe(algoOrderId);
    }

    orderBookPtr= nullptr;
    quoteFeedPtr = nullptr;

    if (m_md) {
        m_md.reset();
    }

    if (onAlgoPerformanceUpdate) {
        onAlgoPerformanceUpdate(algoPerf);
        onAlgoPerformanceUpdate = nullptr;
    }

    publishAlgoperformance();

    algoPerf.release();

}

void AlgoTrader::cancel() {

    SPDLOG_INFO("[aId:{}]算法取消", algoOrderId);

    if (!AlgoStatus::isRunning(algoPerf.algoStatus) ) {

        algoPerf.algoStatus = AlgoStatus::Canceling;
        algoPerf.errMsg = "USER CANCELING";

        auto orderIds = algoPerf.getAllPendingOrderIds();

        if (not orderIds.empty()) {

            SPDLOG_INFO("[aId:{}]CANCEL ALL PENDING ORDERS,TOTAL COUNT:{}", algoOrderId, orderIds.size());
            cancelOrder(orderIds);
        }
        else {
            algoPerf.algoStatus = AlgoStatus::Terminated;
            algoPerf.errMsg = "USER CANCEL.";
        }
    }
}

void AlgoTrader::placeOrder(const int qty, const double price,bool checkMinAmtPerOrder) {
    double _price = price;
    if (qty > 0 && _price > 0) {
        if (algoOrderPtr->priceLimit > 0) {
            if (algoPerf.isBuy) {
                _price = std::min(_price, algoOrderPtr->priceLimit);
            }
            else {
                _price = std::max(_price, algoOrderPtr->priceLimit);
            }
        }
        if (ssinfoPtr->highLimitPrice > 0) {
            _price = std::min(_price, ssinfoPtr->highLimitPrice);
        }
        if (ssinfoPtr->lowLimitPrice > 0) {
            _price = std::max(_price, ssinfoPtr->lowLimitPrice);
        }
        if (algoPerf.isBuy) {
            if ((_price == algoOrderPtr->priceLimit or _price == ssinfoPtr->highLimitPrice) and algoOrderPtr->notPegOrderAtLimitPrice) {
                SPDLOG_INFO("[OrderSkip:PegLimit][aid:{}]{},price:{},priceLimit:{:.3f},highLimitPrice:{:.3f}", algoOrderId,algoOrderPtr->symbol,_price, algoOrderPtr->priceLimit, ssinfoPtr->highLimitPrice);
                return;
            }
            if (( _price == ssinfoPtr->lowLimitPrice) and algoOrderPtr->notBuyOnLLOrSellOnHL) {
                SPDLOG_INFO("[OrderSkip:BuyOnLL][aid:{}]{},price:{},priceLimit:{:.3f},lowLimitPrice:{:.3f}", algoOrderId, algoOrderPtr->symbol,_price, algoOrderPtr->priceLimit, ssinfoPtr->lowLimitPrice);
                return;
            }
        }
        else {
            if ((_price == algoOrderPtr->priceLimit or _price == ssinfoPtr->lowLimitPrice) and algoOrderPtr->notPegOrderAtLimitPrice) {
                SPDLOG_INFO("[OrderSkip:PegLimit][aid:{}]{},price:{},priceLimit:{:.3f},lowLimitPrice:{:.3f}", algoOrderId, algoOrderPtr->symbol, _price, algoOrderPtr->priceLimit, ssinfoPtr->lowLimitPrice);
                return;
            }
            if ((_price == ssinfoPtr->highLimitPrice) and algoOrderPtr->notBuyOnLLOrSellOnHL) {
                SPDLOG_INFO("[OrderSkip:SellOnHL][aid:{}]{},price:{},priceLimit:{:.3f},highLimitPrice:{:.3f}", algoOrderId, algoOrderPtr->symbol, _price, algoOrderPtr->priceLimit, ssinfoPtr->highLimitPrice);
                return;
            }
        }
        if (checkMinAmtPerOrder and algoOrderPtr->minAmountPerOrder > 0) {
            if (price * qty < algoOrderPtr->minAmountPerOrder) {
                SPDLOG_INFO("[OrderSkip:MinAmount][aid:{}]{},price:{},qty:{}", algoOrderId, algoOrderPtr->symbol, _price,qty);
                return;
            }
        }
        auto order = Order::make_intrusive();
        order->orderId = OrderIdGenerator::getInstance().NewId();
        order->acct = algoOrderPtr->acct;
        order->acctType = algoOrderPtr->acctType;
        order->symbol = algoOrderPtr->symbol;
        order->tradeSide = algoOrderPtr->tradeSide;
        order->orderQty = qty;
        order->orderPrice = _price;
        order->algoOrderId = algoOrderId;
        order->orderTime = agcommon::now();
        order->quoteTime = m_md->quoteTime;
        order->updTime = agcommon::now();

        algoPerf.onOrderRequest(order, m_md.get());

        SPDLOG_DEBUG("[OrderNew]{},bid1/ask1:{},{},qtime:{},total qty:{}", order->to_string(), m_md->bidPrice1, m_md->askPrice1, agcommon::getTimeStr(m_md->quoteTime),algoPerf.qty);
        
        if (not orderBookPtr->placeOrder(order.get())) {

            SPDLOG_WARN("[OrderFailed]{},bid1/ask1:{},{},qtime:{}", order->to_string(), m_md->bidPrice1, m_md->askPrice1, agcommon::getTimeStr(m_md->quoteTime));
            
            order->status = agcommon::OrderStatus::REJECTED;

            algoPerf.onOrderRecUpdate(order);
        }
    }
};

void AlgoTrader::cancelOrder(const std::vector<OrderId_t>& orderIds) {

    for (const auto& orderId : orderIds) {

        SPDLOG_INFO("[OrderCancel][aid:{}]OrderId:{}", algoOrderId, orderId);
        if (not orderBookPtr->cancelOrderByOrderId(orderId)) {
            SPDLOG_DEBUG("[OrderCancel][aid:{}]OrderId:{} failed,may have final status.", algoOrderId, orderId);
        }
    }
};

void AlgoTrader::onOrderUpdate(const Order* order) {
    
    auto orderCp = Order::make_intrusive(*order);

    algoPerf.onOrderRecUpdate(orderCp);

    if (cancelAllOrderFlag && order->cancelQty > 0) {
        SPDLOG_INFO("[aId:{}][cancelAll]pending:{},remain:{},order:{}", algoOrderId
            , algoPerf.qty - algoPerf.qtyFilled
            , algoPerf.qtyTarget - algoPerf.qty
            , order->to_string_simple());
    }
    else {
        if (order->isFinalStatus()) {
            SPDLOG_INFO("OrderUpdate:{}", order->to_string_simple());
        }
    }

    algoPerf.performanceSummay(m_md.get());

    if (onAlgoPerformanceUpdate) {
        onAlgoPerformanceUpdate(algoPerf);
    }

    publishAlgoperformance();

    if (cancelAllOrderFlag && algoPerf.qty == algoPerf.qtyFilled and algoPerf.qtyFilled != algoPerf.qtyTarget) {  // not pending order

        timer.cancel();
    }

};

void AlgoTrader::onMarketDepth(MarketDepth* newMd) {  //shoud dispatch in trader's context
    
    if (isStopped()) {
        return;
    }

    algoPerf.performanceSummay(newMd);

    //executeSignal(newMd,m_md.get());      // quick act on predict LOB change if possible

    m_md = MarketDepthKeepAlivePtr(newMd);

    mdcounts++;

};

/*executeSignal: demo only */
void AlgoTrader::executeSignal(const MarketDepth* newMd, const MarketDepth* last_md) {

    if (newMd->flow > 200 * 10000 and newMd->askPrice1 > last_md->askPrice1) {

        if (algoPerf.isBuy) {
            slicePolicyOnSignal(PolicyAction::TAKE, newMd->askPrice1);  //take now
        }
        else {
            slicePolicyOnSignal(PolicyAction::MAKE, newMd->askPrice3);  //make at high
        }
    }
    if (newMd->flow < -200 * 10000 and newMd->bidPrice1 < last_md->bidPrice1) {

        if (algoPerf.isBuy) {

            slicePolicyOnSignal(PolicyAction::MAKE, newMd->bidPrice3);  //make at low
        }
        else {
            slicePolicyOnSignal(PolicyAction::TAKE, newMd->bidPrice1); // take now
        }
    }

}

asio::awaitable<int> AlgoTrader::co_onMarketDepth(const MarketDepth* _md) {

        if (algoOrderPtr->isBackTestOrder()) {
            if (!AlgoStatus::isRunning(algoPerf.algoStatus)) {
                SPDLOG_DEBUG("[aId:{}]{}", algoOrderId, algoPerf.algoStatus);
                wait2Resume.clear();
            }
            else {
                if (agcommon::addDuration(_md->quoteTime,3.0) >= replayNextQuoteTime) {
                    SPDLOG_DEBUG("[aId:{}]onMarketDepth quoteTime:{} > strategy:{}", algoOrderId
                        , agcommon::getDateTimeStr(_md->quoteTime), agcommon::getDateTimeStr(replayNextQuoteTime));
                    co_await wait2Resume.wait();
                }
            }
        }

        co_return algoPerf.algoStatus;
};

asio::awaitable<void> AlgoTrader::schedual() {
        
        publishAlgoperformance();

        int durationPerSlicer, roundsPerSlicer;

        if (algoDuration < 5 * 60) {

            durationPerSlicer = 40;
            roundsPerSlicer = 2;

        } else {

            durationPerSlicer = 60;
            roundsPerSlicer = 3;
        }
        if (ssinfoPtr->preClosePrice < 6) {

            durationPerSlicer = 60;
            roundsPerSlicer = 2;
        }

        double rand_d = (std::rand() % 500) * 0.01;

        algoDuration = std::round((algoDuration - rand_d) * 1000.0) / 1000.0;

        int numOfSlicers = static_cast<int>(std::ceil(algoDuration / durationPerSlicer));

        auto cumQtyOfSlicers = TWAP::allocateCumulativeQty(algoOrderPtr->qtyTarget, numOfSlicers, ssinfoPtr);

        if (algoOrderPtr->algoStrategy == AlgoMsg::MsgAlgoStrategy::VWAP) {
            cumQtyOfSlicers = VWAP::allocateCumulativeQty(algoOrderPtr->qtyTarget, numOfSlicers, ssinfoPtr, startTime, endTime);
        }

        SPDLOG_INFO("[aId:{}]{},startTime:{},endTime:{},algoDuration:{},numOfSlicer:{}",algoOrderId,algoOrderPtr->symbol,
            agcommon::getDateTimeStr(startTime), agcommon::getDateTimeStr(endTime), algoDuration, numOfSlicers);

        double remainTime = algoDuration - durationPerSlicer * (cumQtyOfSlicers.size() - 1);
        double bias = 0.05;
        if (ssinfoPtr->preClosePrice < 6) {
            bias = 0.10;
        }
        std::vector<double> ExecProgressAllowedBias(cumQtyOfSlicers.size(), bias);

        for (std::size_t idx = 0; idx < cumQtyOfSlicers.size(); idx++) {

            int cumQty = cumQtyOfSlicers[idx];
            double progessBias = ExecProgressAllowedBias[idx];
            auto   duration_remain = durationPerSlicer;

            Slicer slicer(idx, cumQty, progessBias, false, durationPerSlicer, 0, 0, duration_remain, roundsPerSlicer,0,0);

            if (idx == cumQtyOfSlicers.size() - 1) {
                slicer.isLast = true;
                slicer.duration         = remainTime;
                slicer.duration_remain  = remainTime;
            }

            slicers.push_back(slicer);
            SPDLOG_DEBUG(slicer.to_string());
        }

        double start2Go = agcommon::getSecondsDiff(agcommon::now(), startTime);

        if (start2Go > 0) {
            SPDLOG_INFO("[aId:{}] WAIT {:.2f} SECONDS TO Execution", algoOrderId, start2Go);
            co_await awaitMarketTime(start2Go);
        }

        SPDLOG_INFO("[aId:{}]Begin Fisrt Slicer.{:.3f}", algoOrderPtr->algoOrderId, rand_d);
        co_await awaitMarketTime(rand_d);

        for (auto& slicer : slicers) {

            if (!AlgoStatus::isRunning(algoPerf.algoStatus)) {
                break;
            }

            curSlicerIndex = slicer.index;

            co_await executeSlicer(slicer);
        }

        // 处理上游可能订单未及时到达的情况...
        auto pendingOrderIds = algoPerf.getAllPendingOrderIds();

        if (not pendingOrderIds.empty()) {
            SPDLOG_WARN("[aId:{}] has {} PendingOrders,wait few seconds to expired", algoOrderId, pendingOrderIds.size());

            co_await awaitMarketTime(10);

            auto pendingOrderIds = algoPerf.getAllPendingOrderIds();

            for (const auto& orderId : pendingOrderIds) {

                auto& order = algoPerf.orderId2Order[orderId];

                SPDLOG_INFO("[aId:{}]PendingOrder:{}", algoOrderId, order->to_string_simple());
            }
        }
};

asio::awaitable<void> AlgoTrader::executeSlicer(Slicer& slicer) {

    if (!AlgoStatus::isRunning(algoPerf.algoStatus)) {
        co_return;
    }
    if (not slicer.isLast) {
        int cancelqty = algoPerf.checkShouldCancelPolicy(static_cast<int>(slicer.cumQty * (1 + slicer.bias)), *m_md, true);
        if (ssinfoPtr->floor2FitLotSize(cancelqty)>0) {
            std::vector<OrderId_t> cancelOrderIds = algoPerf.getCancelOrderIds(cancelqty);
            cancelOrder(cancelOrderIds);
            co_await awaitMarketTime(2);
            slicer.duration_remain = std::max(slicer.duration_remain - 2.0, 1.0);
        }

        int32_t qty2Make = 0;
        int32_t qty2Take = 0;
        int qtyAtBestPrice, qtyOutOfBest;
        std::tie(qtyAtBestPrice, qtyOutOfBest) = algoPerf.getQtyPendingAtBestPrice(m_md.get());
        int marketVolAtLimitPrice = 0;
        if (algoOrderPtr->priceLimit > 0) {
            int qtyAtLimitPrice   = algoPerf.getQtyAtLimitPrice(algoOrderPtr->priceLimit);
            marketVolAtLimitPrice = m_md->getPricezVol(algoOrderPtr->priceLimit);
            qtyAtBestPrice = std::max(qtyAtBestPrice, qtyAtLimitPrice);
        }
        int32_t beforLastUpper = algoOrderPtr->qtyTarget - ssinfoPtr->getOrderInitQty();
        int32_t upper = std::min(int32_t(slicer.cumQty * (1 + slicer.bias)), beforLastUpper);
        int32_t lower = std::min((int)(slicer.cumQty * (1 - slicer.bias)), beforLastUpper);
        if (algoOrderPtr->participateRate > 0 and m_md->volume - algoPerf.arriveMarketVolume >0) {
            int32_t max_pov_vol = (algoOrderPtr->participateRate / 100.0 * (m_md->volume - algoPerf.arriveMarketVolume));
            upper = std::min(upper, max_pov_vol);
            lower = std::min(lower, max_pov_vol);
        }
        if (algoPerf.qtyFilled + qtyAtBestPrice < upper) {
            qty2Make = upper - algoPerf.qtyFilled - qtyAtBestPrice;
            qty2Make = std::min(qty2Make, std::min(upper,beforLastUpper) - algoPerf.qty);
        };

        if (algoPerf.qtyFilled < lower) {
            qty2Take = (lower - algoPerf.qtyFilled);
            qty2Take = std::max(0,std::min(qty2Take, beforLastUpper - algoPerf.qty));
        }
        slicer.qty2make = qty2Make;
        slicer.qty2take = qty2Take;
        slicer.qty2make_remain = slicer.qty2make;
        slicer.qty2take_remain = slicer.qty2take;

        auto [lotSize,stepSize] = ssinfoPtr->getMinOrderQty();
        if (algoOrderPtr->minAmountPerOrder > 0 and ssinfoPtr->lowLimitPrice > 0) {
            lotSize = std::max(lotSize,int(algoOrderPtr->minAmountPerOrder / ssinfoPtr->lowLimitPrice));
        }

        slicer.rounds_remain = std::max(std::min(slicer.rounds_remain, (int)std::max(qty2Make / lotSize, qty2Take / lotSize)),1);
        SPDLOG_INFO("[aId:{}]slicePlocy:[{}]qty:{},qtyFilled:{}", algoOrderId,slicer.to_string(), algoPerf.qty, algoPerf.qtyFilled);

        for (auto round = slicer.rounds_remain; round > 0; round--) {

            slicer.rounds_remain = round;
            co_await slicePolicy(slicer);
        }
    } 
    else {

        std::vector<OrderId_t> orderIds = algoPerf.getOrderIdsOutOfBestPrice(m_md.get());
        if (!orderIds.empty()) {
            SPDLOG_INFO("[aId:{}]cancel outof best:{}", algoOrderId, orderIds.size());
            cancelOrder(orderIds);
            double _duration = slicer.duration_remain;
            slicer.duration_remain = std::max(slicer.duration_remain - 3, 1.0);
            co_await awaitMarketTime(_duration - slicer.duration_remain);
        }
        int qtyAtBestPrice, qtyOutOfBest;
        auto cumQty = slicer.cumQty;
        if (algoOrderPtr->participateRate > 0 and m_md->volume - algoPerf.arriveMarketVolume > 0) {
            int max_pov_vol = static_cast<int>(algoOrderPtr->participateRate / 100.0 * (m_md->volume - algoPerf.arriveMarketVolume));
            cumQty = std::min(cumQty, max_pov_vol);
        }

        std::tie(qtyAtBestPrice, qtyOutOfBest) = algoPerf.getQtyPendingAtBestPrice(m_md.get());
        int qty2Make = algoPerf.makerFilledRate > 80 ? cumQty - algoPerf.qtyFilled - qtyAtBestPrice : 0;
        int qty2Take = cumQty - algoPerf.qty;
        slicer.qty2make = qty2Make;
        slicer.qty2take = qty2Take;
        slicer.qty2make_remain = slicer.qty2make;
        slicer.qty2take_remain = slicer.qty2take;
        auto [lotSize, stepSize] = ssinfoPtr->getMinOrderQty();

        slicer.rounds_remain = std::max(std::min(slicer.rounds_remain, std::max(qty2Make / lotSize, qty2Take / lotSize)), 1);

        SPDLOG_INFO("[aId:{}]slicePlocy:[{}]", algoOrderId, slicer.to_string());

        for (auto round = slicer.rounds_remain; round > 0; round--) {
            slicer.rounds_remain = round;
            co_await lastSlicePolicy(slicer);
        }
    }
};

asio::awaitable<void> AlgoTrader::slicePolicy(Slicer& slicer) {

    if (!AlgoStatus::isRunning(algoPerf.algoStatus)) {
        co_return;
    }
    std::string lob = std::format("{},bid/ask:{:.3f}@{},{:.3f}@{}"
        , agcommon::getTimeInt(m_md->quoteTime)
        , m_md->bidPrice1
        , m_md->bidVol1
        , m_md->askPrice1
        , m_md->askVol1);

    int qty2Make_round = slicer.rounds_remain > 0 ? slicer.qty2make_remain / slicer.rounds_remain : slicer.qty2make_remain;
    int qty2Take_round = slicer.rounds_remain > 0 ? slicer.qty2take_remain / slicer.rounds_remain : slicer.qty2take_remain;
    double duration_round = slicer.rounds_remain > 0 ? slicer.duration_remain / slicer.rounds_remain : slicer.duration_remain;

    int qtyTotalRemain = algoOrderPtr->qtyTarget - algoPerf.qty;
    qty2Make_round = std::min(qty2Make_round, qtyTotalRemain);
    qty2Take_round = std::min(qty2Take_round, qtyTotalRemain);
    bool allow2Take = algoPerf.checkShouldTakePolicy(*m_md);
    bool allow2Make = algoPerf.qtyFilled < slicer.cumQty - slicer.qty2take_remain;  //algoPerf.checkAllowMakePolicy(*m_md);

    if (allow2Take) {
        allow2Make = allow2Make && (algoPerf.qty + qty2Take_round + qty2Make_round) < algoOrderPtr->qtyTarget;
    }

    int maxMarketImpactVol = 0;
    double currentProgress = slicer.cumQty>0 and algoPerf.qty >0 ? algoPerf.qtyFilled * 1.0 / std::min(slicer.cumQty,algoPerf.qty) : 0;

    SPDLOG_INFO("[aId:{}][index{},R{}][pg:{:.2f},pr:{:.2f},fr:{:.2f}]duration:{:.2f}s,allow2Make:{},allow2Take:{},qty2Take_round:{},qty2Make_round:{},{}",
                    algoOrderId, slicer.index, slicer.rounds_remain,currentProgress,algoPerf.makerFilledRate, algoPerf.filledRate,duration_round, allow2Make, allow2Take, qty2Take_round, qty2Make_round, lob);
   
    qty2Make_round = ssinfoPtr->floor2FitLotSize(qty2Make_round);

    if (allow2Make && qty2Make_round > 0) {
        double price2Make_round = getPrice4Make(qty2Make_round);
        SPDLOG_INFO("[aId:{}][Maker]{}@{:.3f}", algoOrderId,qty2Make_round, price2Make_round);
        placeOrder(qty2Make_round, price2Make_round);
        slicer.qty2make_remain = slicer.qty2make_remain - qty2Make_round;
    } 
    {
        qty2Take_round = getQtyShinkImpact(qty2Take_round);
        double price2Take_round = getPriceCover4Take(qty2Take_round,0.5);
        if (allow2Take and qty2Take_round > 0) {
            SPDLOG_INFO("[aId:{}][Taker]{}@{:.3f}", algoOrderId,qty2Take_round, price2Take_round);
            placeOrder(qty2Take_round, price2Take_round);
            slicer.qty2take_remain = slicer.qty2take_remain - qty2Take_round;
        }
    }

    slicer.duration_remain = slicer.duration_remain - duration_round;

    co_await awaitMarketTime(duration_round);
};

asio::awaitable<void> AlgoTrader::lastSlicePolicy(Slicer& slicer) {
    if (!AlgoStatus::isRunning(algoPerf.algoStatus)) {
        co_return;
    }

    std::string lob = std::format("{},bid/ask:{:.3f}@{},{:.3f}@{}"
        , agcommon::getTimeInt(m_md->quoteTime)
        , m_md->bidPrice1
        , m_md->bidVol1
        , m_md->askPrice1
        , m_md->askVol1);

    if (slicer.rounds_remain == 1) {

        auto orderIds = algoPerf.getAllPendingOrderIds();

        if (!orderIds.empty()) {
            SPDLOG_INFO("[aId:{}]CANCEL ALL PENDING ORDERS:{}", algoOrderId,orderIds.size());
            cancelOrder(orderIds);
            cancelAllOrderFlag = true;
            auto wait4Cancel = 3.1;
            slicer.duration_remain = std::max(slicer.duration_remain - wait4Cancel, 3.0);
            co_await awaitMarketTime(wait4Cancel);
        }

        auto qtyTarget = algoOrderPtr->qtyTarget;
        if (algoOrderPtr->participateRate > 0 and m_md->volume - algoPerf.arriveMarketVolume > 0) {
            int max_pov_vol = static_cast<int>(algoOrderPtr->participateRate / 100.0 * (m_md->volume - algoPerf.arriveMarketVolume));
            qtyTarget = std::min(qtyTarget, (uint32_t)max_pov_vol+200);
            if (qtyTarget < algoOrderPtr->qtyTarget) {
                SPDLOG_INFO("[aId:{}]max pov limit {}", algoOrderId,qtyTarget);
            }
        }
        int qtyTotalRemain = qtyTarget - algoPerf.qty;
        if (algoPerf.isBuy) {
            qtyTotalRemain = ssinfoPtr->floor2FitLotSize(qtyTotalRemain);
        }
        double currentProgress = slicer.cumQty - slicer.qty2make_remain > 0 ? algoPerf.qtyFilled * 1.0 / (slicer.cumQty - slicer.qty2make_remain) : 0;
        double price2Take = getPriceCover4Take(qtyTotalRemain, 0.1);
        SPDLOG_INFO("[aId:{}][index{},R{}][pg:{:.2f},pr:{:.2f}],duration:{:.2f},qtyTotalRemain:{},price2Take:{:.3f},{}",
                            algoOrderId, slicer.index, slicer.rounds_remain, currentProgress, algoPerf.makerFilledRate, slicer.duration_remain, qtyTotalRemain, price2Take, lob);
        placeOrder(qtyTotalRemain, price2Take, false);
        co_await awaitMarketTime(slicer.duration_remain);
        co_return;
    }

    int qty2Make_round = slicer.rounds_remain > 0 ? slicer.qty2make_remain / slicer.rounds_remain : slicer.qty2make_remain;
    int qty2Take_round = slicer.rounds_remain > 0 ? slicer.qty2take_remain / slicer.rounds_remain : slicer.qty2take_remain;
    double duration_round = slicer.rounds_remain > 0 ? slicer.duration_remain / slicer.rounds_remain : slicer.duration_remain;

    int qtyTotalRemain = algoOrderPtr->qtyTarget - algoPerf.qty;
    qty2Make_round = std::min(qty2Make_round, qtyTotalRemain);
    qty2Take_round = std::min(qty2Take_round, qtyTotalRemain);

    bool allow2Make = algoPerf.checkAllowMakePolicy(*m_md);
    bool allow2Take = algoPerf.checkShouldTakePolicy(*m_md);

    int maxMarketImpactVol = 0;
    double currentProgress = slicer.cumQty - slicer.qty2make_remain > 0 ? algoPerf.qtyFilled * 1.0 / (slicer.cumQty - slicer.qty2make_remain) : 0;

    SPDLOG_INFO("[aId:{}][index{},R{}][pg:{:.2f},pr:{:.2f}]duration:{:.2f},allow2Make:{},allow2Take:{},qty2Take_round:{},qty2Make_round:{},{}",
        algoOrderId, slicer.index,slicer.rounds_remain, currentProgress, algoPerf.makerFilledRate, duration_round, allow2Make, allow2Take, qty2Take_round, qty2Make_round, lob);
    
    qty2Make_round = ssinfoPtr->floor2FitLotSize(qty2Make_round);

    if (allow2Make && qty2Make_round > 0) {
        double price2Make_round = getPrice4Make(qty2Make_round);
        SPDLOG_INFO("[aId:{}][Maker]{}@{:.3f}", algoOrderId,qty2Make_round, price2Make_round);
        placeOrder(qty2Make_round, price2Make_round);
        slicer.qty2make_remain = slicer.qty2make_remain - qty2Make_round;
    } else 
    {
        qty2Take_round = getQtyShinkImpact(qty2Take_round);
        double price2Take_round = getPriceCover4Take(qty2Take_round,0);
        if (qty2Take_round > 0) {
            SPDLOG_INFO("[aId:{}][Taker]{}@{:.3f}", algoOrderId,qty2Take_round, price2Take_round);
            placeOrder(qty2Take_round, price2Take_round);
            slicer.qty2take_remain = slicer.qty2take_remain - qty2Take_round;
        }
    }
    slicer.duration_remain = slicer.duration_remain - duration_round;
    co_await awaitMarketTime(duration_round);
};

void AlgoTrader::slicePolicyOnSignal(const PolicyAction& action, const double price) {

    if (curSlicerIndex < 0 or !AlgoStatus::isRunning(algoPerf.algoStatus)) {
        return;
    }

    auto& slicer = slicers[curSlicerIndex];

    int qtyTotalRemain = algoOrderPtr->qtyTarget - algoPerf.qty;

    switch (action) {

        case PolicyAction::MAKE: {

            int qty2Make_round = slicer.rounds_remain > 0 ? slicer.qty2make_remain / slicer.rounds_remain : slicer.qty2make_remain;

            qty2Make_round = std::min(qty2Make_round, qtyTotalRemain);
            qty2Make_round = ssinfoPtr->floor2FitLotSize(qty2Make_round);

            if (qty2Make_round > 0) {

                SPDLOG_INFO("[aId:{}][Signal:Make]{}@{:.3f}", algoOrderId, qty2Make_round, price);
                placeOrder(qty2Make_round, price);
                slicer.qty2make_remain = slicer.qty2make_remain - qty2Make_round;
            }
            break;
        }
        case PolicyAction::TAKE: {

            int qty2Take_round = slicer.rounds_remain > 0 ? slicer.qty2take_remain / slicer.rounds_remain : slicer.qty2take_remain;

            qty2Take_round = std::min(qty2Take_round, qtyTotalRemain);
            qty2Take_round = getQtyShinkImpact(qty2Take_round);

            if (qty2Take_round > 0) {

                SPDLOG_INFO("[aId:{}][Signal:Take]{}@{:.3f},{},{}", algoOrderId, qty2Take_round, price, slicer.rounds_remain,slicer.qty2take_remain);
                placeOrder(qty2Take_round, price);
                slicer.qty2take_remain = slicer.qty2take_remain - qty2Take_round;
            }
            break;
        }
        default:
            break;
    }
}

double AlgoTrader::getPriceCover4Take(int qty, double coverRate) {

    coverRate = coverRate != 0 ? coverRate : AlgoConstants::DefaultCoverRate;

    int volsum = 0;
    double price = 0.0;

    if (algoPerf.isBuy) {

        price = m_md->askPrice1;
        auto ask_price2Vol = m_md->getAskQuotes();

        for (const auto& item : ask_price2Vol) {
            if (item.first > 0) {
                if ((volsum + item.second) * coverRate > qty) {
                    return item.first;
                }
                else {
                    volsum += item.second;
                    price = item.first;
                }
            }
        }
    } else {
        price = m_md->bidPrice1;
        auto  bid_price2Vol = m_md->getBidQuotes();

        for (const auto& item : bid_price2Vol) {
            if (item.first > 0) {
                if ((volsum + item.second) * coverRate > qty) {
                    return item.first;
                }
                else {
                    volsum += item.second;
                    price = item.first;
                }
            }
        }
    }
    return price;
};

int32_t AlgoTrader::getQtyShinkImpact(int32_t qty2take) {

    int32_t maxMarketImpactVol = 0;

    if (algoPerf.isBuy) {
        maxMarketImpactVol = (int32_t)((m_md->askVol1 + m_md->askVol2) * AlgoConstants::MAXShotImpactVolRate);
    }
    else {
        maxMarketImpactVol = (int32_t)((m_md->bidVol1 + m_md->bidVol2) * AlgoConstants::MAXShotImpactVolRate);
    }

    if (qty2take - maxMarketImpactVol >= ssinfoPtr->getOrderInitQty()) {
        qty2take = std::min(qty2take, maxMarketImpactVol);
    }

    return ssinfoPtr->floor2FitLotSize(qty2take);
}

double AlgoTrader::getPrice4Make(int qty2Make) {

    if (algoPerf.makerFilledRate > 80.0) {
        if (agcommon::isBuy(algoOrderPtr->tradeSide)) {
            return m_md->bidPrice2;
        } else {
            return m_md->askPrice2;
        }
    } else {
        if (agcommon::isBuy(algoOrderPtr->tradeSide)) {
            return m_md->bidPrice1;
        } else {
            return m_md->askPrice1;
        }
    }
}

asio::awaitable<void> AlgoTrader::awaitMarketTime(double seconds) {

    if (algoOrderPtr->isBackTestOrder()) {

        auto now = replayNextQuoteTime;

        replayNextQuoteTime = agcommon::AshareMarketTime::addMarketDuration(replayNextQuoteTime, seconds);

        SPDLOG_DEBUG("[aId:{}] move {:.2f} secs from {} to {}", algoOrderId, seconds
            , agcommon::getDateTimeInt(now)
            , agcommon::getDateTimeInt(replayNextQuoteTime));

        if (not AlgoStatus::isFinalStatus(algoPerf.algoStatus)) {

            if (replayNextQuoteTime > m_md->quoteTime) {
                co_await wait2Resume.wait();
            }
            else {
                co_await asio::post(*contextPtr, asio::use_awaitable);
            }
        }
    }
    else {

        auto now = agcommon::now();
        auto end = agcommon::addDuration(now, seconds);
        auto moniingCloseTime = AshareMarketTime::getMarketMorningCloseTime(now);

        if (now <= moniingCloseTime && end >= moniingCloseTime) {

            auto marketBreakDuration = AshareMarketTime::getMarketAfternoonOpenTime(now) - AshareMarketTime::getMarketMorningCloseTime(now);
            seconds = seconds + marketBreakDuration.total_seconds();
            end = end + marketBreakDuration;
        }

        int64_t remainMilliSeconds = (int64_t)(seconds * 1000);
        SPDLOG_DEBUG("[aId:{}]{:.4f},remainMilliSeconds:{}", algoOrderId, seconds, remainMilliSeconds);

        while (remainMilliSeconds > 10) {

            if (not AlgoStatus::isRunning(algoPerf.algoStatus)) {
                auto end = agcommon::addDuration(agcommon::now(), 2);
            }

            int64_t this_sleep = std::min(static_cast<int64_t>(2000), remainMilliSeconds);

            timer.expires_after(std::chrono::milliseconds(this_sleep));

            auto [ec] = co_await timer.async_wait(boost::asio::as_tuple(boost::asio::use_awaitable));

            if (!ec) {

                remainMilliSeconds = (end - agcommon::now()).total_milliseconds();

            }
            else if (ec == asio::error::operation_aborted) {

                SPDLOG_INFO("[aId:{}]{:.4f},remainMilliSeconds:{},timer canceled", algoOrderId, seconds, remainMilliSeconds);
                break;
            }
            else {
                SPDLOG_ERROR("[aId:{}]{:.4f},remainMilliSeconds:{},{}", algoOrderId, seconds, remainMilliSeconds, ec.message());
            }
        }
    }
}

void AlgoTrader::publishAlgoperformance() const {

    auto msgAlgoInstanceInfo = encode2AlgoMessage();

    if (msgAlgoInstanceInfo) {
        auto shouldCache = AlgoStatus::isFinalStatus(algoPerf.algoStatus);
        TCPSessionManager::getInstance().sendNotify2C(algoOrderPtr->getAcctKey(), AlgoMsg::CMD_NOTIFY_AlgoExecutionInfo
            , msgAlgoInstanceInfo, shouldCache);
    }
}

std::shared_ptr<AlgoMsg::MsgAlgoPerformance> AlgoTrader::encode2AlgoMessage() const {

    auto msgPtr = std::make_shared<AlgoMsg::MsgAlgoPerformance>();

    msgPtr->set_algo_order_id(algoOrderPtr->algoOrderId);
    msgPtr->set_client_algo_order_id(algoOrderPtr->clientAlgoOrderId);
    msgPtr->set_acct(algoOrderPtr->acct);
    msgPtr->set_acct_type(algoOrderPtr->acctType);
    msgPtr->set_vendor_id(::AlgoMsg::MsgAlgoVendorId::VendorId_DEFAULT);
    msgPtr->set_algo_category(algoOrderPtr->algoCategory);
    msgPtr->set_algo_strategy(algoOrderPtr->algoStrategy);

    msgPtr->set_symbol(algoOrderPtr->symbol);
    msgPtr->set_qty_target(algoOrderPtr->qtyTarget);
    msgPtr->set_order_side(algoOrderPtr->tradeSide);
    msgPtr->set_start_time(agcommon::ptime2Integer(startTime));
    msgPtr->set_end_time  (agcommon::ptime2Integer(endTime));
    msgPtr->set_exec_duration(algoOrderPtr->execDuration);
    msgPtr->set_qty(algoPerf.qty);
    msgPtr->set_qty_filled(algoPerf.qtyFilled);
    msgPtr->set_qty_canceled(algoPerf.qtyCanceled);
    msgPtr->set_qty_rejected(algoPerf.qtyRejected);
    msgPtr->set_ordercnt(algoPerf.orderCnt);
    msgPtr->set_ordercnt_filled(algoPerf.orderCntFilled);
    msgPtr->set_ordercnt_canceled(algoPerf.orderCntCanceled);
    msgPtr->set_ordercnt_rejected(algoPerf.orderCntRejected);
    msgPtr->set_amt(algoPerf.amt);
    msgPtr->set_avg_price(algoPerf.avgPrice);

    msgPtr->set_filled_rate(algoPerf.filledRate);
    msgPtr->set_maker_filled_rate(algoPerf.makerFilledRate);
    msgPtr->set_maker_rate(algoPerf.makerRate);
    msgPtr->set_cancel_rate(algoPerf.cancelRate);
    msgPtr->set_arrive_price(algoPerf.arrivePrice);
    msgPtr->set_market_vwap(algoPerf.marketVwapPrice);
    msgPtr->set_market_twap(algoPerf.marketTwapPrice);

    msgPtr->set_slippage_arrive_price(algoPerf.slippageArrivePrice);
    msgPtr->set_slippage_market_vwap(algoPerf.slippageVWAP);
    msgPtr->set_slippage_market_twap(algoPerf.slippageTWAP);
    msgPtr->set_actual_pov(algoPerf.actualParticipateRate);
    msgPtr->set_algo_status(algoPerf.algoStatus);
    msgPtr->set_algo_msg(algoPerf.errMsg);

    return msgPtr;
}
