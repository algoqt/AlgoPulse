#pragma once

#include<chrono>
#include<string>
#include"typedefs.h"
#include"common.h"
#include"Order.h"
#include "AlgoMessages.pb.h"

struct Slicer {

    size_t  index{ 0 };
    int     cumQty{ 0 };
    double  bias{ 0.05 };
    bool    isLast{ false };

    double  duration  { 0 };
    int     qty2make{ 0 };
    int     qty2take{ 0 };

    double  duration_remain{ 0 };
    int     rounds_remain{ 1 };

    int     qty2make_remain{ 0 };
    int     qty2take_remain{ 0 };

    std::string to_string();
};

class AlgoOrder {

public:

    AlgoOrderId_t       algoOrderId{};

    ClientAlgoOrderId_t clientAlgoOrderId{};

    AlgoMsg::AcctType acctType  {};

    Acct_t          acct      {};

    Broker_t        brokerId{ 0 };

    Symbol_t    symbol       {};

    std::string securityType {};

    AlgoMsg::MsgAlgoCategory algoCategory    { AlgoMsg::MsgAlgoCategory::Category_ALGO };

    AlgoMsg::MsgAlgoStrategy algoStrategy    { AlgoMsg::MsgAlgoStrategy::TWAP };

    int         tradeSide       { -1};

    uint32_t    qtyTarget       { 0 };

    double      amtTarget       { 0.0 };

    OrderTime_t startTime       {};

    OrderTime_t endTime         {};

    double execDuration     { 0.0 };

    double priceLimit       { 0.0 };

    double participateRate  { 0.0 };

    double minAmountPerOrder{ 0.0 };

    bool notPegOrderAtLimitPrice{ true };

    bool notBuyOnLLOrSellOnHL  { true };

    bool isParentOrder          { true };

    AlgoOrderId_t       parentAlgoOrderId {0};

    OrderTime_t createTime          { agcommon::now()};

    std::unordered_map<std::string, std::string> paramKeyconfig{};


public:

    void setAlgoOrderId(const AlgoOrderId_t _algoOrderId) {
        algoOrderId = _algoOrderId;
    }
    void setAcctType(const AlgoMsg::AcctType& _acctType) {
        acctType = _acctType;
    }
    void setAcct(const std::string& _acct) {
		acct = _acct;
	}
    void setBrokerId(const Broker_t& _brokerId) {
        brokerId = _brokerId;
    }
    void setSymbol(const Symbol_t& _symbol) {
        symbol = _symbol;
    }
    void setTradeSide(const int _tradeSide) {
        tradeSide = _tradeSide;
    }
    void setQtyTarget(const uint32_t _qtyTarget) {
        qtyTarget = _qtyTarget;
    }
    void setAmtTarget(const double _amtTarget) {
        amtTarget = _amtTarget;
    }
    void setAlgoStrategy(const AlgoMsg::MsgAlgoStrategy _algoStrategy) {
        algoStrategy = _algoStrategy;
	}
    void setAlgoCategory(const AlgoMsg::MsgAlgoCategory _algoCategory) {
        algoCategory = _algoCategory;
    }
    void setSecurityType(const std::string& _securityType) {
		securityType = _securityType;
	}
    void setStartTime(const OrderTime_t _startTime) {
        startTime = _startTime;
    }
    void setEndTime(const OrderTime_t _endTime) {
        endTime = _endTime;
    }
    void setExecDuration(const double _execDuration) {
        execDuration = _execDuration;
    }
    void setPriceLimit(const double _priceLimit) {
        priceLimit = _priceLimit;
    }
    void setParticipateRate(const double _participateRate) {
        participateRate = _participateRate;
    }
    void setMinAmountPerOrder(const double _minAmountPerOrder) {
        minAmountPerOrder = _minAmountPerOrder;
    }
    void setNotPegOrderAtLimitPrice(const bool _notPegOrderAtLimitPrice) {
        notPegOrderAtLimitPrice = _notPegOrderAtLimitPrice;
    }
    void setNotBuyOnLLOrSellOnHL(const bool _notBuyOnLLOrSellOnHL) {
        notBuyOnLLOrSellOnHL = _notBuyOnLLOrSellOnHL;
    }
    void setClientAlgoOrderId(const ClientAlgoOrderId_t& _clientAlgoOrderId) {
        clientAlgoOrderId = _clientAlgoOrderId;
    }

    std::string to_string() const;

    inline std::string getOrDefault(
        const std::string& key,
        const std::string& defaultValue) const {

        return paramKeyconfig.contains(key) ? paramKeyconfig.at(key) : defaultValue;
    }

    inline double getOrDefault(
        const std::string& key,
        const double defaultValue) const {

        return paramKeyconfig.contains(key) ? agcommon::get_double(paramKeyconfig.at(key), defaultValue) : defaultValue;
    }

    inline int32_t getOrDefault(
        const std::string& key,
        const int32_t defaultValue) const {

        return paramKeyconfig.contains(key) ? agcommon::get_int(paramKeyconfig.at(key), defaultValue) : defaultValue;
    }

    OrderBookKey_t getOrderBookKey() const;

    AcctKey_t      getAcctKey() const;

    agcommon::QuoteMode getQuoteMode() const;

    inline bool isBackTestOrder() const {
        return getQuoteMode() == agcommon::QuoteMode::REPLAY;
    }
};