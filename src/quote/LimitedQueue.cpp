#include "LimitedQueue.h"
#include "ShotSignal.h"

void LimitedQueue::release() {

    while (not _shotQueue.empty()) {
        _shotQueue.pop_front();
    }
    while (not _referQueue.empty()) {
        _referQueue.pop_front();
    }

    lowPriceMd = nullptr;

    highPriceMd = nullptr;
}

void LimitedQueue::append(MarketDepth* newItem) {

    auto item = MarketDepthKeepAlivePtr(newItem);
    shotPeriodFlow += item->flow;
    _shotQueue.push_back(item);

    if (_shotQueue.size() > shotWindowSize) {

        auto drop = _shotQueue.front();
        _shotQueue.pop_front();

        shotPeriodAmount = item->amount - drop->amount;
        shotPeriodVol = item->volume - drop->volume;
        shotPeriodAvgPrice = shotPeriodVol > 0 ? shotPeriodAmount / shotPeriodVol : item->price;
        shotPeriodFlow -= drop->flow;
        shotPeriodInterval = agcommon::AshareMarketTime::getMarketDuration(drop->quoteTime, item->quoteTime);

        referPeriodFlow += item->flow;

        if (referWindowSize > 0) {
            _referQueue.push_back(drop);

            if (referWindowSize > 0 and _referQueue.size() > referWindowSize) {
                auto drop = _referQueue.front();
                _referQueue.pop_front();

                referPeriodAmount = item->amount - drop->amount;
                referPeriodVol = item->volume - drop->volume;
                referPeriodAvgPrice = referPeriodVol > 0 ? referPeriodAmount / referPeriodVol : item->price;
                referPeriodFlow -= drop->flow;

                referPeriodInterval = agcommon::AshareMarketTime::getMarketDuration(drop->quoteTime, item->quoteTime);
            }
        }
    }

}

const double LimitedQueue::getShotChageRate() const {
    if (_shotQueue.size() < shotWindowSize) {
        return 0;
    }
    double midPrice = _shotQueue.front()->getMidPrice();
    auto& md = _shotQueue.back();
    double shotChange = midPrice > 0 ? (md->price - midPrice) / midPrice : 0;
    return shotChange;
}

void LimitedQueue::resetLow(const MarketDepthKeepAlivePtr& newMd) {
    assert(lowPriceMd == nullptr);
    if (_shotQueue.size() > 0) {
        int lowIndex = 0;
        lowPriceMd = _shotQueue[lowIndex];
        for (auto i = 1; i < _shotQueue.size(); i++) {
            if (lowPriceMd->price < _shotQueue[i]->price) {
                lowPriceMd = _shotQueue[i];
            }
        }
    }
    else {
        lowPriceMd = newMd;
    }
}

bool LimitedQueue::shotConfirm(MarketDepth* _newMd
    , double rangeRatePercent/* = 2.0*/
    , double durationConfig /*= 300*/
    , ShotConfirm* sc) {

    auto newMd = MarketDepthKeepAlivePtr(_newMd);
    if (lowPriceMd == nullptr) {
        resetLow(newMd);
    }
    else {
        if (_shotQueue.size() > 0 and _shotQueue[0]->quoteTime > lowPriceMd->quoteTime and highPriceMd == nullptr) {
            lowPriceMd = nullptr;
            resetLow(newMd);
        }
        if (newMd->price <= lowPriceMd->price and newMd->quoteTime > lowPriceMd->quoteTime) {
            lowPriceMd = newMd;
            if (highPriceMd) {
                highPriceMd = nullptr; // 新低时 重置最高价
            }
        }
    }
    double rangeRate = lowPriceMd->price > 0 ? (newMd->price - lowPriceMd->price) / lowPriceMd->price * 100 : 0;
    //SPDLOG_INFO("{},{:.3f},{},low:{:.3f},{}", newMd->symbol, rangeRate, agcommon::getDateTimeStr(newMd->quoteTime), lowPriceMd->price, agcommon::getDateTimeStr(lowPriceMd->quoteTime));

    if (rangeRate > rangeRatePercent and highPriceMd == nullptr) {

        highPriceMd = newMd;
        trigVol = newMd->volume - _shotQueue[0]->volume;
    }

    bool confirm = false;

    if (highPriceMd and newMd->price > highPriceMd->price and newMd->quoteTime > lowPriceMd->quoteTime) {

        sc->confirmDuration = agcommon::AshareMarketTime::getMarketDuration(highPriceMd->quoteTime, newMd->quoteTime);

        if (sc->confirmDuration > durationConfig) {

            SPDLOG_INFO("trigger signal:{},{:.3f},qt:{},low:{:.3f}@{},high:{:.3f}@{},duration {:.0f}", newMd->symbol
                , rangeRate, newMd->quoteTime
                , lowPriceMd->price, lowPriceMd->quoteTime
                , highPriceMd->price, highPriceMd->quoteTime, sc->confirmDuration);

            confirm = true;
            /* restart watching */
            sc->lowMdPtr  = lowPriceMd;
            sc->highMdPtr = highPriceMd;
            sc->confirmMd = newMd;
            sc->shotVol   = trigVol;

            lowPriceMd  = nullptr;
            highPriceMd = nullptr;
        }
        else {
            highPriceMd = newMd;
            trigVol     = newMd->volume - _shotQueue[0]->volume;
        }
    }

    return confirm;
}
