#pragma once

#include "typedefs.h"
#include <sstream>
#include "common.h"

struct PositionInfo {

    AlgoMsg::AcctType   acctType    {};
    Acct_t              acct        {};
    Broker_t            brokerId{ 0 };

    Symbol_t    symbol {};
    int         currentQty  { 0 };
    int         enabledQty  { 0 };
    double      costPrice    { 0.0 };
    double      lastPrice    { 0.0 };
    double      mktValue     { 0.0 };

    std::string positionType{};

    OrderTime_t updTime{};

    std::string to_string() const {

        constexpr auto formatPrice = [](double price) { return fmt::format("{:.3f}", price); };

        auto formattedTime = agcommon::getTimeStr(updTime);

        return fmt::format(
            "[{}]acctType: {}, acct: {}, symbol: {}, currentQty: {}, enabledQty: {}, costPrice: {}, lastPrice: {}, mktValue: {:.2f}, positionType: {}",
            formattedTime
            ,acctType
            , acct
            , symbol
            , currentQty
            , enabledQty
            , formatPrice(costPrice)
            , formatPrice(lastPrice)
            , mktValue
            , positionType
        );
    }

};