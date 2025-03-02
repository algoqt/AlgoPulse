#include "StockDataTypes.h"

SecurityStaticInfo::SecurityStaticInfo(const Symbol_t& _symbol) :
    symbol(_symbol)
{
    if (not (orderInitQtySize > 0 && lotSize > 0)) {
        if (symbol.starts_with("68")) {
            orderInitQtySize = 200;
            lotSize = 1;
        }
        else {
            orderInitQtySize = 100;
            lotSize = 100;
        }
    }

    if (tickSize == 0.0) {
        if (symbol.ends_with("SZ")) {
            if (symbol.substr(0, 1) == "0" || symbol.substr(0, 1) == "2" || symbol.substr(0, 1) == "3") {
                tickSize = 0.01;
            }
            else {
                tickSize = 0.001;
            }
        }
        if (symbol.ends_with("SH")) {
            if (symbol.substr(0, 1) == "6") {
                tickSize = 0.01;
            }
            else {
                tickSize = 0.001;
            }
        }
    }
}
