#include "H5DataTypes.h"

namespace h5data {

    HighFive::CompoundType create_compound_type_for_stock_info() {

        return HighFive::CompoundType({

            {"trade_date", HighFive::create_datatype<int32_t>()},
            {"symbol", HighFive::FixedLengthStringType(12,HighFive::StringPadding::NullTerminated)},
            {"sec_level", HighFive::create_datatype<int32_t>()},
            {"sec_type", HighFive::create_datatype<int32_t>()},
            {"exchange", HighFive::FixedLengthStringType(5,HighFive::StringPadding::NullTerminated)},
            {"sec_id"  , HighFive::FixedLengthStringType(7,HighFive::StringPadding::NullTerminated)},
            {"sec_name", HighFive::FixedLengthStringType(64,HighFive::StringPadding::NullTerminated)},
             {"upper_limit", HighFive::create_datatype<float>()},
             {"lower_limit", HighFive::create_datatype<float>()},
             {"pre_close", HighFive::create_datatype<float>()},
             {"adj_factor", HighFive::create_datatype<float>()},
             {"is_suspended",HighFive::create_datatype<int8_t>()},
             {"price_tick", HighFive::create_datatype<float>()},
             {"listed_date", HighFive::create_datatype<int32_t>()},
             {"delisted_date", HighFive::create_datatype<int32_t>()},
             {"trade_n", HighFive::create_datatype<int32_t>()},
             {"board", HighFive::create_datatype<int32_t>()},
             {"belong_index", HighFive::create_datatype<int32_t>()},
             {"underlying_symbol", HighFive::FixedLengthStringType(12,HighFive::StringPadding::NullTerminated)},
             {"conversion_start_date", HighFive::create_datatype<int32_t>()},
             {"ttl_shr", HighFive::create_datatype<int64_t>()},
             {"a_shr_unl", HighFive::create_datatype<int64_t>()}
            }
        );
    };

    HighFive::CompoundType create_compound_type_for_tarde_calendar() {

        return HighFive::CompoundType({
            {"date", HighFive::create_datatype<int32_t>()},
            {"trade_date", HighFive::create_datatype<int32_t>()},
            {"next_trade_date", HighFive::create_datatype<int32_t>()},
            {"pre_trade_date", HighFive::create_datatype<int32_t>()}
            }
        );
    };

    HighFive::CompoundType create_compound_type_for_dailybar_raw() {

        return HighFive::CompoundType({

            {"trade_date", HighFive::create_datatype<int32_t>()},
            {"symbol", HighFive::FixedLengthStringType(12,HighFive::StringPadding::NullTerminated)},
            {"open", HighFive::create_datatype<double>()},
            {"high", HighFive::create_datatype<double>()},
            {"low", HighFive::create_datatype<double>()},
            {"close", HighFive::create_datatype<double>()},
            {"volume"  ,HighFive::create_datatype<int64_t>()},
            {"amount", HighFive::create_datatype<double>()},
            {"pre_close", HighFive::create_datatype<double>()},
            {"turnrate"  ,HighFive::create_datatype<double>()},
            {"market_cap", HighFive::create_datatype<double>()},
            {"market_cap_circ", HighFive::create_datatype<double>()},
            {"sec_type", HighFive::create_datatype<int32_t>()},
            {"upper_limit", HighFive::create_datatype<double>()},
            {"low_limit", HighFive::create_datatype<double>()},
            {"adj_factor", HighFive::create_datatype<double>()},
            {"belong_index", HighFive::create_datatype<int32_t>()}
            }
        );
    };

    HighFive::CompoundType create_compound_type_for_minutebar_raw() {

        return HighFive::CompoundType(
            {
                {"trade_date", HighFive::create_datatype<int32_t>()},
                {"symbol", HighFive::FixedLengthStringType(12,HighFive::StringPadding::NullTerminated)},
                {"open", HighFive::create_datatype<float>()},
                {"high", HighFive::create_datatype<float>()},
                {"low", HighFive::create_datatype<float>()},
                {"close", HighFive::create_datatype<float>()},
                {"volume"  ,HighFive::create_datatype<int64_t>()},
                {"amount", HighFive::create_datatype<double>()},
                {"pre_close", HighFive::create_datatype<float>()},
                {"bob", HighFive::create_datatype<int32_t>()},
                {"eob", HighFive::create_datatype<int32_t>()},
            }
        );
    };

    HighFive::CompoundType create_compound_type_for_tick() {

        return HighFive::CompoundType{
            {"symbol", HighFive::create_datatype<uint32_t>()},
            {"exchange", HighFive::create_datatype<uint8_t>()},
            {"open", HighFive::create_datatype<float>()},
            {"high", HighFive::create_datatype<float>()},
            {"low", HighFive::create_datatype<float>()},
            {"price", HighFive::create_datatype<float>()},
            {"bid_price1", HighFive::create_datatype<float>()},
            {"bid_volume1", HighFive::create_datatype<int32_t>()},
            {"bid_price2", HighFive::create_datatype<float>()},
            {"bid_volume2", HighFive::create_datatype<int32_t>()},
            {"bid_price3", HighFive::create_datatype<float>()},
            {"bid_volume3", HighFive::create_datatype<int32_t>()},
            {"bid_price4", HighFive::create_datatype<float>()},
            {"bid_volume4", HighFive::create_datatype<int32_t>()},
            {"bid_price5", HighFive::create_datatype<float>()},
            {"bid_volume5", HighFive::create_datatype<int32_t>()},
            {"ask_price1", HighFive::create_datatype<float>()},
            {"ask_volume1", HighFive::create_datatype<int32_t>()},
            {"ask_price2", HighFive::create_datatype<float>()},
            {"ask_volume2", HighFive::create_datatype<int32_t>()},
            {"ask_price3", HighFive::create_datatype<float>()},
            {"ask_volume3", HighFive::create_datatype<int32_t>()},
            {"ask_price4", HighFive::create_datatype<float>()},
            {"ask_volume4", HighFive::create_datatype<int32_t>()},
            {"ask_price5", HighFive::create_datatype<float>()},
            {"ask_volume5", HighFive::create_datatype<int32_t>()},
            {"cum_volume", HighFive::create_datatype<int64_t>()},
            {"cum_amount", HighFive::create_datatype<double>()},
            {"last_amount", HighFive::create_datatype<double>()},
            {"last_volume", HighFive::create_datatype<int64_t>()},
            {"created_at", HighFive::create_datatype<uint32_t>()}
        };
    };

}