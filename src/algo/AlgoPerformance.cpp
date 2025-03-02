
#include "AlgoOrder.h"
#include "StockDataTypes.h"
#include "AlgoPerformance.h"
#include "common.h"


AlgoPerformance::AlgoPerformance(const AlgoOrder* _algoOrderPtr, const SecurityStaticInfo* _ssinfoPtr) :
    algoOrderPtr(_algoOrderPtr)
    , algoOrderId(_algoOrderPtr->algoOrderId)
    , symbol(_algoOrderPtr->symbol)
    , qtyTarget(_algoOrderPtr->qtyTarget)
    , ssinfoPtr(_ssinfoPtr)
    , mdsCount(0)
{
    isBuy = agcommon::isBuy(_algoOrderPtr->tradeSide);
}

void AlgoPerformance::performanceSummay(const MarketDepth* md) {

    timeProgress = agcommon::getTimeProgress(startTime, endTime);

    if (md and md->symbol == symbol) {

        if ((md->quoteTime - startTime).total_seconds() > -2 && (md->quoteTime - endTime).total_seconds() < -2) {

            if ((arrivePrice == 0 || arriveMarketAmount == 0 || arriveMarketVolume == 0) && md->amount > 0) {
                arriveMarketTime = boost::posix_time::to_simple_string(md->quoteTime);
                arrivePrice = md->price > 0 ? md->price : std::max(md->askPrice1, md->bidPrice1);
                arriveMarketAmount = md->amount;
                arriveMarketVolume = md->volume;
                SPDLOG_INFO("[{}]arriveMarketTime:{},arrivePrice:{:.2f}", algoOrderId, arriveMarketTime, arrivePrice);
            }
            if (md->price > 0) {
                marketTwapPrice = (marketTwapPrice * mdsCount + md->price) / (mdsCount + 1);
                mdsCount += 1;
            }

            auto marketTradeVol = md->volume - arriveMarketVolume;
            marketVwapPrice = marketTradeVol > 0 ? (md->amount - arriveMarketAmount) / marketTradeVol : 0.0;

            slippageArrivePrice = calculateSlippage(isBuy, avgPrice, arrivePrice);
            slippageVWAP        = calculateSlippage(isBuy, avgPrice, marketVwapPrice);
            slippageTWAP        = calculateSlippage(isBuy, avgPrice, marketTwapPrice);

            SPDLOG_DEBUG("aid:{},qt:{},avgPrice:{},marketVwapPrice:{:.4f},slippageVWAP:{:.4f}"
                ,algoOrderId,agcommon::getDateTimeInt(md->quoteTime),  avgPrice, marketVwapPrice, slippageVWAP);

            if (md->volume - arriveMarketVolume > 0) {
                actualParticipateRate = qtyFilled * 100.0 / marketTradeVol;
            }
            momentum = arrivePrice > 0 ? (md->price - arrivePrice) / arrivePrice * 100.0 : 0.0;
        }
    }
}

void AlgoPerformance::onOrderRequest(const OrderPtr& order, const MarketDepth* md) {

    orderId2Order[order->orderId] = order;

    orderCnt += 1;
    qty += order->orderQty;

    amt += order->orderQty * order->orderPrice;

    if ((order->isBuy() && order->orderPrice < md->askPrice1) || (!order->isBuy() && order->orderPrice > md->bidPrice1 and md->bidPrice1>0)) {

        qtyMaker += order->orderQty;

        makerOrderId2QtyFilled[order->orderId] = 0;

    }

}

void AlgoPerformance::onOrderRecUpdate(const OrderPtr& order) {

    SPDLOG_DEBUG("AlgoPerfmance onOrderUpdate:{}", order->to_string());
    OrderPtr preOrder = nullptr;
    if (auto it = orderId2Order.find(order->orderId);it != orderId2Order.end()) {
        preOrder = it->second;
    }
    else {
        preOrder = Order::make_intrusive();
        preOrder->orderId = 0;
        preOrder->acctType = order->acctType;
        preOrder->acct = order->acct;
        preOrder->symbol = order->symbol;
        preOrder->tradeSide = -1;
        preOrder->status = agcommon::OrderStatus::REPORTING;
    }
    if (order->isNewerThan(*preOrder)) {

        orderId2Order[order->orderId] = order;

        onOrderRecUpdateTwo(*order, *preOrder);
        
    }

}

void AlgoPerformance::onOrderRecUpdateTwo(const Order& order, const Order& preOrder) {

    qtyFilled += (order.filledQty - preOrder.filledQty);
    amtFilled += order.filledAmt - preOrder.filledAmt;
    avgPrice = qtyFilled > 0 ? amtFilled / qtyFilled : 0.0;
    if (order.filledQty - preOrder.filledQty>0){
        lastFilledTime = order.updTime;
    }
    if (makerOrderId2QtyFilled.find(order.orderId) != makerOrderId2QtyFilled.end()) {
        makerOrderId2QtyFilled[order.orderId] = order.filledQty;
        qtyMakerFilled += (order.filledQty - preOrder.filledQty);
    }
    if (order.isFinalStatus()) {
        if (order.orderQty == order.filledQty) {
            orderCntFilled += 1;
        }
        else if (order.cancelQty > 0) {
            orderCntCanceled += 1;
            qtyCanceled += order.cancelQty;
            if (makerOrderId2QtyFilled.find(order.orderId) != makerOrderId2QtyFilled.end()) {
                orderCntMakerCanceled += 1;
            }
        }
        if (order.status == agcommon::OrderStatus::PARTIALCANCEL || order.status == agcommon::OrderStatus::CANCELED || order.status == agcommon::OrderStatus::REJECTED) {
            qty = qty - (order.orderQty - order.filledQty);
            amt = amt - (order.orderQty - order.filledQty) * order.orderPrice;
        }
        if (order.status == agcommon::OrderStatus::REJECTED) {
            orderCntRejected += 1;
            qtyRejected += order.orderQty;
            orderCntContinueRejected += 1;
        }
        else {
            orderCntContinueRejected = 0;
        }
    }

    timeProgress = agcommon::getTimeProgress(startTime, endTime);

    double totalQty = static_cast<double>(qty + qtyCanceled);
    filledRate = qty + qtyCanceled > 0 ? (qtyFilled * 100.0)   / totalQty : 0.0;
    cancelRate = qty + qtyCanceled > 0 ? (qtyCanceled * 100.0) / totalQty : 0.0;
    makerRate = qty + qtyCanceled > 0 ? (qtyMaker * 100.0) / totalQty : 0.0;
    makerFilledRate = qtyMaker > 0 ? qtyMakerFilled * 100.0 / static_cast<double>(qtyMaker) : 0.0;

}


int AlgoPerformance::getQtyAtLimitPrice(double limitPrice) {

    int qty = 0;
    for (const auto& [orderId, order] : orderId2Order) {
        if (order->orderPrice == limitPrice && !order->isFinalStatus())
            qty += order->orderQty - order->filledQty;
    }
    return qty;
}


bool AlgoPerformance::checkAllowMakePolicy(const MarketDepth& md) const {

    double Threshold_filledRate = 60.0;
    double Threshold_worstCancelRate = 40.0;
    if (isLowPrice()) {
        Threshold_filledRate = 30;
        Threshold_worstCancelRate = 50;
    }
    auto e_orderCnt = orderCnt - orderCntRejected + 1;
    auto worstCancelRate = e_orderCnt > 0 ? (e_orderCnt - orderCntFilled) * 100.0 / e_orderCnt : 0.0;
    if (orderCnt > 10) {
        if (worstCancelRate > Threshold_worstCancelRate and filledRate < Threshold_filledRate) {
            return false;
        }
    }

    return true;
}

bool AlgoPerformance::checkShouldTakePolicy(const MarketDepth& md) const {
    double Threshold_filledRate = 50.0;
    double Threshold_timeProgress = 0.15;
    if (isLowPrice()) {
        Threshold_filledRate = 30;
        Threshold_timeProgress = 0.2;
    }
    if (orderCnt > 6 or timeProgress > Threshold_timeProgress) {
        if (filledRate < Threshold_filledRate) {
            return true;
        }
        //auto d = agcommon::getMarketDuration( lastOrderTime, md->quoteTime);
        //if (d > 90) {
        //    SPDLOG_INFO("[{}]index,lastOrderTime:{},quoteTime:{}",algoOrderId,boost::posix_time::to_iso_string(lastOrderTime), boost::posix_time::to_iso_string(md->quoteTime));
        //    return true;
        //}
    }

    return false;
}

int AlgoPerformance::checkShouldCancelPolicy(int maxCumQty, const MarketDepth& md, bool excludeBestPrice) {
    int cancelQtyTarget = 0;
    if (qty > maxCumQty) {
        cancelQtyTarget = qty - maxCumQty;
        if (excludeBestPrice) {
            int qtyAtBest, qtyOutOfBest;
            std::tie(qtyAtBest, qtyOutOfBest) = getQtyPendingAtBestPrice(&md);
            cancelQtyTarget = std::min(cancelQtyTarget, qtyOutOfBest);
        }
    }
    return ssinfoPtr->floor2FitLotSize(cancelQtyTarget);
}

std::vector<OrderId_t> AlgoPerformance::getAllPendingOrderIds() {

    std::vector<OrderId_t> orderIds;
    for (const auto& [orderId, order] : orderId2Order) {
        if (not order->isFinalStatus()) {
            orderIds.push_back(order->orderId);
        }
    }
    return orderIds;
}

std::vector<OrderId_t> AlgoPerformance::getCancelOrderIds(int cancelQtyTarget, bool skipResidualFill) {

    std::vector<OrderPtr> orders;
    for (const auto& [orderId, order] : orderId2Order) {
        if (!order->isFinalStatus()) {
            orders.push_back(order);
        }
    }
    std::sort(orders.begin(), orders.end(), [](const OrderPtr& a, const OrderPtr& b) {
        return a->orderPrice < b->orderPrice;
        });
    int cumQty = 0;
    std::vector<OrderId_t> cancelOrderIds;
    for (const auto& order : orders) {
        int qtyPending = order->orderQty - order->filledQty;
        cumQty += qtyPending;
        if (cumQty > cancelQtyTarget) {
            break;
        }
        if (skipResidualFill) {
            if (qtyPending == ssinfoPtr->floor2FitLotSize(qtyPending)) {
                cancelOrderIds.push_back(order->orderId);
            }
        }
        else {
            cancelOrderIds.push_back(order->orderId);
        }
    }
    return cancelOrderIds;
}

std::pair<int, int> AlgoPerformance::getQtyPendingAtBestPrice(const MarketDepth* md) {

    return std::make_pair(0, 0);
    int qtyAtBest = 0;
    int qtyOutOfBest = 0;
    //std::cout << "getQtyPendingAtBestPrice:" << orderId2Order.size() << std::endl;
    if (isBuy) {
        for (const auto& [orderId, order] : orderId2Order) {
            qtyAtBest += order->orderPrice == md->bidPrice1 && !order->isFinalStatus() ? (order->orderQty - order->filledQty) : 0;
            qtyOutOfBest += order->orderPrice < md->bidPrice1 && !order->isFinalStatus() ? (order->orderQty - order->filledQty) : 0;
        }
    }
    else {
        for (const auto& [orderId, order] : orderId2Order) {
            qtyAtBest    += order->orderPrice == md->askPrice1 && !order->isFinalStatus() ? (order->orderQty - order->filledQty) : 0;
            qtyOutOfBest += order->orderPrice > md->askPrice1 && !order->isFinalStatus() ? (order->orderQty - order->filledQty) : 0;
        }
    }
    return std::make_pair(qtyAtBest, qtyOutOfBest);
}

std::vector<OrderId_t> AlgoPerformance::getOrderIdsOutOfBestPrice(const MarketDepth* md) {
    int qtyAtBest = 0, qtyOutOfBest = 0;
    std::vector<OrderId_t> orderIds;
    std::tie(qtyAtBest, qtyOutOfBest, orderIds) = getOrderDataXXBestPrice(md);
    return orderIds;
}

std::tuple<int, int, std::vector<OrderId_t>> AlgoPerformance::getOrderDataXXBestPrice(const MarketDepth* md) {

    int qtyAtBest = 0;
    int qtyOutOfBest = 0;
    std::vector<OrderId_t> outOfBestPriceOrderIds;

    if (isBuy) {
        for (auto it = orderId2Order.begin(); it != orderId2Order.end();it++) {
            const auto& [orderId, order] = *it;
            qtyAtBest += order->orderPrice >= md->bidPrice1 && !order->isFinalStatus() ? (order->orderQty - order->filledQty) : 0;
            if (order->orderPrice < md->bidPrice1 && !order->isFinalStatus()) {
                qtyOutOfBest += (order->orderQty - order->filledQty);
                outOfBestPriceOrderIds.push_back(order->orderId);
            }
        }
    }
    else {
        for (auto it = orderId2Order.begin(); it != orderId2Order.end(); it++) {
            const auto& [orderId, order] = *it;
            qtyAtBest += order->orderPrice <= md->askPrice1 && !order->isFinalStatus() ? (order->orderQty - order->filledQty) : 0;
            if (order->orderPrice > md->askPrice1 && !order->isFinalStatus()) {
                qtyOutOfBest += (order->orderQty - order->filledQty);
                outOfBestPriceOrderIds.push_back(order->orderId);
            }
        }
    }

    return std::make_tuple(qtyAtBest, qtyOutOfBest, outOfBestPriceOrderIds);
}

std::string AlgoPerformance::to_string() const {

    constexpr auto formatPrice = [](double price) { return fmt::format("{:.3f}", price); };
    constexpr auto formatRate =  [](double rate) { return fmt::format("{:.2f}", rate); };

    auto totalQty = qty + qtyCanceled;

    return fmt::format(
        "[algoPerf][aid:{}]symbol:{}, {}, side:{}, qtyTarget:{}, qty:{}, qtyFilled:{}, qtyCanceled:{}, "
        "orderCnt:{}, orderCntFilled:{}, orderCntCanceled:{}, orderCntRejected:{}, arrivePrice:{}, marketVwap:{}, "
        "avgPrice:{}, slippageArrivePrice:{}, slippageVWAP:{}, slippageTWAP:{}, timeProgress:{}, filledRate:{}, "
        "cancelRate:{}, makerFilledRate:{}, pov:{}, momentum:{}, algoStatus:{}, errMsg:{}, clientOrderId:{}",
        algoOrderId,
        symbol,
        algoOrderPtr->getAcctKey(),
        agcommon::TradeSide::name(algoOrderPtr->tradeSide),
        qtyTarget,
        totalQty,
        qtyFilled,
        qtyCanceled,
        orderCnt,
        orderCntFilled,
        orderCntCanceled,
        orderCntRejected,
        formatPrice(arrivePrice),
        formatPrice(marketVwapPrice),
        formatPrice(avgPrice),
        formatRate(slippageArrivePrice),
        formatRate(slippageVWAP),
        formatRate(slippageTWAP),
        fmt::format("{:.1f}", timeProgress),
        formatRate(filledRate),
        formatRate(cancelRate),
        formatRate(makerFilledRate),
        formatRate(actualParticipateRate),
        formatRate(momentum),
        algoStatus,
        errMsg,
        algoOrderPtr->clientAlgoOrderId
    );
}
