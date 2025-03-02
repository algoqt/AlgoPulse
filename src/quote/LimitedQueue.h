#pragma once

#include <deque>
#include "MarketDepth.h"

class LimitedQueue {

public:

    LimitedQueue(const std::size_t shotTickSize, const std::size_t referTickSize) :
        shotWindowSize(shotTickSize), referWindowSize(referTickSize) {}

    LimitedQueue() = delete;

    ~LimitedQueue() {

        release();
    }

    void release();

    void append(MarketDepth* newItem);

    inline std::size_t size() const {   return _shotQueue.size();    }

    const double getShotChageRate() const;

    void resetLow(const MarketDepthKeepAlivePtr& newMd);

    bool triggerShotMinMax(MarketDepth* _newMd, double rangeRatePercent = 2.0, double durationConfig = 300);

    inline std::pair<double, double> getShotAmount() const {

        double shotAvgAmount = shotPeriodInterval > 0 ? shotPeriodAmount / shotPeriodInterval : 0;
        return std::pair<double, double>(shotPeriodAmount, shotAvgAmount);
    }

    inline std::pair<double, double> getReferAmount() const {

        double referAvgAmout = referPeriodInterval > 0 ? referPeriodAmount / referPeriodInterval : 0;
        return std::pair<double, double>(referPeriodAmount, referAvgAmout);
    }

    inline const bool isReady() const {
        if (referWindowSize > 0) {
            return _referQueue.size() >= referWindowSize;
        }
        return _shotQueue.size() >= shotWindowSize;
    }

    inline std::vector<MarketDepthKeepAlivePtr> get_items() const {

        return std::vector<MarketDepthKeepAlivePtr>(_shotQueue.begin(), _shotQueue.end());
    }
    const MarketDepthKeepAlivePtr get_item(std::size_t index) const {
        if (index >= _shotQueue.size()) {
            throw std::out_of_range("Index out of range");
        }
        return _shotQueue[index];
    }

private:

    std::size_t shotWindowSize = 15;
    std::size_t referWindowSize = 0;

    std::deque<MarketDepthKeepAlivePtr> _shotQueue;

    std::deque<MarketDepthKeepAlivePtr> _referQueue;

    MarketDepthKeepAlivePtr lowPriceMd = nullptr;

    MarketDepthKeepAlivePtr highPriceMd = nullptr;


public:

    double shotPeriodAmount   = 0;
    double shotPeriodInterval = 0;
    int64_t shotPeriodVol     = 0;
    double shotPeriodAvgPrice = 0;
    double shotPeriodFlow     = 0.0;

    double referPeriodAmount   = 0;
    double referPeriodInterval = 0;
    int64_t referPeriodVol     = 0;
    double referPeriodAvgPrice = 0;
    double referPeriodFlow     = 0.0;

};
