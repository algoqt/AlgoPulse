#pragma once

#include "typedefs.h"
#include <highfive/highfive.hpp>

namespace h5data {

    struct TradeCalendar {

        int32_t date = 0;
        int32_t trade_date = 0;  // 非交易日为0
        int32_t next_trade_date = 0;
        int32_t pre_trade_date = 0;
    };

    /*h5 */
    struct Tick {
        uint32_t symbol;
        uint8_t exchange;
        float   open;
        float   high;
        float   low;
        float   price;
        float   bid_price1;
        int32_t bid_volume1;
        float   bid_price2;
        int32_t bid_volume2;
        float   bid_price3;
        int32_t bid_volume3;
        float   bid_price4;
        int32_t bid_volume4;
        float   bid_price5;
        int32_t bid_volume5;
        float   ask_price1;
        int32_t ask_volume1;
        float   ask_price2;
        int32_t ask_volume2;
        float   ask_price3;
        int32_t ask_volume3;
        float   ask_price4;
        int32_t ask_volume4;
        float   ask_price5;
        int32_t ask_volume5;
        int64_t cum_volume;
        double  cum_amount;
        double  last_amount;  // 元
        int64_t last_volume;  // 股
        uint32_t created_at; // timestamp
    };


    /* h5 */
    struct DailyBarRaw {
        int32_t trade_date = 0;
        char symbol[12]{ '0' };
        double open = 0.0f;
        double high = 0.0f;
        double low = 0.0f;
        double close = 0.0f;
        int64_t volume = 0;
        double amount = 0.0f;
        double pre_close = 0.0f;
        double turnrate = 0.0f;
        double market_cap = 0.0f;
        double market_cap_circ = 0.0f;
        int32_t sec_type = 0;
        double upper_limit = 0.0f;
        double lower_limit = 0.0f;
        double adj_factor = 0.0f;
        int32_t belong_index = 0;
    };

    /* h5 */
    struct MinuteBarRaw {
        int32_t trade_date = 0;
        char symbol[12];
        float open = 0.0f;
        float high = 0.0f;
        float low = 0.0f;
        float close = 0.0f;
        int64_t volume = 0;
        double amount = 0.0;
        float pre_close = 0.0f;
        int32_t bob = 0;
        int32_t eob = 0;
    };

    /* h5 */
    struct StockInfo {
        int32_t trade_date;  // 交易日期
        char    symbol[12];     // 股票代码 (11个字符 + 1个终止符)
        int32_t sec_level;   // 股票级别
        int32_t sec_type;    // 证券类型     int32_t sec_type;    1: 股票, 2: 基金, 3: 指数, 4: 期货, 5: 期权, 8：可转债， 10: 虚拟合约
        char    exchange[5];    // 交易所 (4个字符 + 1个终止符)
        char    sec_id[7];      // 交易所证券代码 (6个字符 + 1个终止符)
        char    sec_name[64];   // 证券名称 (32个字符 + 1个终止符)
        float   upper_limit;   // 涨停价
        float   lower_limit;   // 跌停价
        float   pre_close;     // 昨收盘价
        float   adj_factor;    // 复权因子
        int8_t  is_suspended;   // 是否停牌
        float   price_tick;    // 最小变动价位
        int32_t listed_date;    // 上市日期
        int32_t delisted_date; // 退市日期
        int32_t trade_n;        // 交易日数
        int32_t board;          // 所属板块
        int32_t belong_index;   // 所属指数 hs300 1 zz500 2 zz1000 4
        char     underlying_symbol[12];         // 标的证券代码
        int32_t  conversion_start_date;     // # 转换起始日
        int64_t  ttl_shr = 0;             //# 总股本
        int64_t  a_shr_unl = 0;             // # 流通股本
    };



    HighFive::CompoundType create_compound_type_for_tick();

    HighFive::CompoundType create_compound_type_for_stock_info();

    HighFive::CompoundType create_compound_type_for_tarde_calendar();

    HighFive::CompoundType create_compound_type_for_dailybar_raw();

    HighFive::CompoundType create_compound_type_for_minutebar_raw();

}

HIGHFIVE_REGISTER_TYPE(h5data::DailyBarRaw, h5data::create_compound_type_for_dailybar_raw);

HIGHFIVE_REGISTER_TYPE(h5data::StockInfo, h5data::create_compound_type_for_stock_info);

HIGHFIVE_REGISTER_TYPE(h5data::TradeCalendar, h5data::create_compound_type_for_tarde_calendar);

HIGHFIVE_REGISTER_TYPE(h5data::MinuteBarRaw, h5data::create_compound_type_for_minutebar_raw);

HIGHFIVE_REGISTER_TYPE(h5data::Tick, h5data::create_compound_type_for_tick);