#pragma once

#include <iostream>
#include <vector>
#include <cmath>
#include <numeric>
#include "typedefs.h"
#include "ContextService.h"

class AlgoOrder;
class SecurityStaticInfo;
class MarketDepth;
struct Order;
struct Trade;


enum class AlgoType {
    TWAP,
    VWAP,
    POV
};

class TWAP {

public:
    static std::vector<int> allocateCumulativeQty(
        int qtyOfTarget
        , int numOfSlicers
        , const SecurityStaticInfo* ssinfo);
};

class VWAP {

public:
    static std::vector<double> volCurveDefault;

    static std::vector<int> allocateCumulativeQty(
        int qtyOfTarget
        , int numOfSlicers
        , const SecurityStaticInfo* ssinfo
        , boost::posix_time::ptime startTime
        , boost::posix_time::ptime endTime
        , const std::vector<double>& volCurve = {}
    );
};

class AlgoPerformance;

typedef std::function<void(const AlgoPerformance& algoPerf)> OnAlgoPerformanceUpdate;


class Trader : public std::enable_shared_from_this<Trader>,boost::noncopyable {

public:

    Trader(const std::shared_ptr<asio::io_context>& c = nullptr) :contextPtr(c), hasStarted{ false }  {

        if (contextPtr == nullptr) {    // 如果不指定 将使用随机
            contextPtr = ContextService::getInstance().getRandomWorkerContext();
        }
    }

    virtual std::shared_ptr<AlgoOrder>  getAlgoOrder() const = 0;

    virtual AlgoOrderId_t  getAlgoOrderId() const = 0;

    virtual AlgoMsg::MsgAlgoCategory getAlgoCategory() const = 0;

    virtual AlgoErrorMessage_t preStartCheck() { return ""; };

    virtual boost::asio::awaitable<void> start() { co_return; };

    virtual void stop(const std::string& reason = "") {};  // 内部算法停止接口

    virtual void cancel() {};   // 客户端 取消/停止 接口

    virtual bool isStopped() const =0 ;

    virtual void onMarketDepth(const MarketDepth* newMd) {};

    virtual void onMarketDepth(MarketDepth* newMd) {};

    virtual void onOrderUpdate(const Order* order) {};

    virtual void onTrade(const Trade* trade) {};

    virtual void onBackTestDone(const std::string& reason = "") {};

    virtual void publishAlgoperformance() const {}

    OnAlgoPerformanceUpdate onAlgoPerformanceUpdate{ nullptr };

    virtual void release() {};

public:

    std::shared_ptr<asio::io_context> contextPtr;

    std::atomic<bool>  hasStarted;

    template<class T>  
    requires std::is_base_of_v<Trader, T>
    std::shared_ptr<T> keep_alive_this() {

        return std::static_pointer_cast<T>(shared_from_this());
    }

};
