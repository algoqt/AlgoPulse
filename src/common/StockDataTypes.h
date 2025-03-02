#pragma once

#include "typedefs.h"
#include <boost/unordered_map.hpp>

struct DailyBar {

    int32_t tradeDate = 0;
    Symbol_t symbol;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    int64_t volume = 0;
    double amount = 0.0;
    double preclose = 0.0;
    double turnRate = 0.0;
    double marketCap = 0.0;
    double marketCapCirc = 0.0;
    int32_t securityType = 0;
    double highLimitPrice = 0.0;
    double lowLimitPrice = 0.0;
    double adjFactor = 0.0;
    int32_t belongIndex = 0;  // hs300 zz500 zz1000 -> 1 2 4 
};

struct MinuteBar {

    QuoteTime_t tradeTime{};
    Symbol_t    symbol;
    double open = 0.0f;
    double high = 0.0f;
    double low = 0.0f;
    double close = 0.0f;
    int64_t volume = 0;
    double amount = 0.0f;
    double preClose = 0.0f;
};

class SecurityStaticInfo {

public:

    explicit SecurityStaticInfo(const Symbol_t& _symbol);

    SecurityStaticInfo() = delete;

    ~SecurityStaticInfo() = default;

public:

    inline int getOrderInitQty() const { return orderInitQtySize; };

    inline std::pair<int, int> getMinOrderQty() const {

        if (orderInitQtySize > 0 && lotSize > 0) {
            return std::make_pair(orderInitQtySize, lotSize);
        }
        if (symbol.starts_with("68") and symbol.ends_with("SH")) {
            return std::make_pair(200, 1);
        }
        else {
            return std::make_pair(100, 100);
        }
    }

    inline int32_t floor2FitLotSize(int32_t qty) const {
        auto [orderInitQtySize, lotSize] = getMinOrderQty();
        if (qty < orderInitQtySize) {
            return 0;
        }
        return orderInitQtySize + (qty - orderInitQtySize) / lotSize * lotSize;
    }

    inline double roundPrice(double price) const {
        if (tickSize > 0.0) {
            return std::round(std::round(price / tickSize) * 10000 * tickSize) / 10000.0;
        }
        else {
            return 0.0;
        }
    }

    inline double roundPriceUp(double price) const {
        if (tickSize > 0.0) {
            return ceil(std::round(price / tickSize) * 10000 * tickSize) / 10000.0;
        }
        else {
            return 0.0;
        }
    }

    inline double roundPriceDown(double price) const {
        if (tickSize > 0.0) {
            return floor(std::round(price / tickSize) * 10000 * tickSize) / 10000.0;
        }
        else {
            return 0.0;
        }
    }
    inline std::string getIndexBelongName() const {

        if (belongIndex == 1) {
            return "hs300";
        }
        else if (belongIndex == 2) {
            return "zz500";
        }
        else if (belongIndex == 4) {
            return "zz1000";
        }
        else if (securityType == 8) {
            return "Cbond";
        }
        return "oth";
    }

public:

    int32_t     tradeDate{ 0 };
    Symbol_t    symbol{};
    std::string SecurityName{};
    int32_t     securityType{};
    std::string exchange{};

    int         orderInitQtySize{ 0 };
    int         lotSize{ 0 };
    double      tickSize{ 0 };

    double      preClosePrice{ 0.0 };
    double      lowLimitPrice{ 0.0 };
    double      highLimitPrice{ 0.0 };

    bool        isSuspended{ false };
    float       adjFactor{ 1.0 };

    int32_t     listedDate{ 19910101 };     // 上市日期
    int32_t     delistedDate{ 29990101 };   // 退市日期
    int32_t     belongIndex{ 0 };           // 所属指数 hs300 1 zz500 2 zz1000 4

    std::string underlyingSymbol;
    int64_t     totalShares = 0;
    int64_t     unLAShare = 0;

};
