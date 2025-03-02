#pragma once

#include "typedefs.h"
#include <cmath>

#include "MarketDepth.h"
#include "Order.h"
#include "AlgoConstants.h"
#include <boost/unordered_map.hpp>


class AlgoOrder;
class SecurityStaticInfo;

using OrderPtr = boost::intrusive_ptr<Order>;

class AlgoPerformance {

public:
    const AlgoOrder*    algoOrderPtr;

    AlgoOrderId_t       algoOrderId;

    Symbol_t            symbol{};

    int             qtyTarget{ 0 };

    bool            isBuy{ false };

    const SecurityStaticInfo* ssinfoPtr;

    OrderTime_t startTime{};
    OrderTime_t endTime  {};
    int qty = 0;
    int qtyFilled = 0;
    int qtyCanceled = 0;
    int qtyRejected = 0;

    int orderCntContinueRejected = 0;

    int orderCnt = 0;
    int orderCntFilled = 0;
    int orderCntCanceled = 0;
    int orderCntRejected = 0;

    double amt = 0.0;
    double amtFilled = 0.0;
    double avgPrice = 0.0;

    int qtyMaker = 0;
    int qtyMakerFilled = 0;
    int orderCntMaker = 0;
    int orderCntMakerCanceled = 0;

    boost::unordered_map<OrderId_t, OrderPtr>   orderId2Order{};
    boost::unordered_map<OrderId_t, int>        makerOrderId2QtyFilled{};

    double timeProgress = 0.0;
    double filledRate = 0.0;
    double makerRate = 0.0;
    double makerFilledRate = 0.0;
    double cancelRate = 0.0;

    double          arrivePrice = 0.0;
    OrderTimeStr_t  arriveMarketTime = "";
    double          arriveMarketAmount = 0.0;
    uint64_t        arriveMarketVolume = 0;

    double actualParticipateRate = 0.0;

    double marketVwapPrice = 0.0;
    double marketTwapPrice = 0.0;

    double slippageArrivePrice = 0.0;
    double slippageVWAP = 0.0;
    double slippageTWAP = 0.0;

    double momentum = 0.0;

    int algoStatus { agcommon::AlgoStatus::Creating};

    std::string errMsg = "";

    OrderTime_t lastFilledTime{};

    int mdsCount = 0;

public:
    AlgoPerformance(const AlgoOrder* _algoOrderPtr, const SecurityStaticInfo* _ssinfoPtr);

    ~AlgoPerformance() = default;

    inline void release() { orderId2Order.clear(); }

    void onOrderRequest(const OrderPtr& order, const MarketDepth* md);

    void onOrderRecUpdate(const OrderPtr& order);

    void onOrderRecUpdateTwo(const Order& _order, const Order& preOrder);

    void performanceSummay(const MarketDepth* md);

    inline double calculateSlippage(bool isBuy, double algoAvPrice, double benchmark) {
        if (benchmark <= 0.0 || algoAvPrice <= 0.0) {
            return 0.0;
        }
        if (isBuy) {
            return (benchmark - algoAvPrice) / benchmark * 10000.0;
        }
        else {
            return (algoAvPrice - benchmark) / benchmark * 10000.0;
        }
    }
    inline bool isLowPrice() const {
        return arrivePrice > 0 and arrivePrice < 6;
    }
    int getQtyAtLimitPrice(double limitPrice);

    bool checkAllowMakePolicy(const MarketDepth& md) const;

    bool checkShouldTakePolicy(const MarketDepth& md) const;

    int checkShouldCancelPolicy(int maxCumQty, const MarketDepth& md, bool excludeBestPrice = false);

    std::vector<OrderId_t>  getAllPendingOrderIds();

    std::vector<OrderId_t>  getCancelOrderIds(int cancelQtyTarget, bool skipResidualFill = true);

    std::pair<int, int>     getQtyPendingAtBestPrice(const MarketDepth* md);

    std::vector<OrderId_t>  getOrderIdsOutOfBestPrice(const MarketDepth* md);

    std::tuple<int, int, std::vector<OrderId_t>> getOrderDataXXBestPrice(const MarketDepth* md);

    std::string to_string() const;

};
