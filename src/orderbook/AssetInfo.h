#pragma once

#include "typedefs.h"
#include <sstream>
#include "common.h"

struct AssetInfo {

    AcctKey_t  acctKey;
    std::string currency;
    std::string exchange;
    double totalAsset{ 0.0 };
    double enabledBalance{ 0.0 };
    double currentBalance{ 0.0 };
    double totalDebit{ 0.0 };
    double netAsset{ 0.0 };
    double assureRatio{ 0.0 };
    double assureEnbuyBalance{ 0.0 };
    double enableBailBalance{ 0.0 };
    double sloLeftBalance{ 0.0 };
    double finEnableQuota{ 0.0 };
    double sloEnableQuota{ 0.0 };
    OrderTime_t updTime{agcommon::now()};

};

static std::ostream& operator<< (std::ostream& os,const AssetInfo& ai) {
    os << "{acct:" << ai.acctKey << ",totalAsset:" << ai.totalAsset \
    << ",enabledBalance:" << ai.enabledBalance << ",currentBalance:" << ai.currentBalance << "}" ;
    return os;
}
