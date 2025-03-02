#pragma once

#include <chrono>
#include "boost/date_time.hpp"
#include "typedefs.h"
#include "AlgoMessages.pb.h"
#include <source_location>

namespace agcommon {

    namespace  posix_time = boost::posix_time;

    struct AlgoStatus {
        static const int Routing = 0;
        static const int Creating = 1;
        static const int Running  = 2;
        static const int Canceling = 4;
        static const int Finished  = 6;
        static const int Expired   = 8;
        static const int Terminated = 10;
        static const int Error = 12;

        static bool isFinalStatus(int status){
            return status == AlgoStatus::Finished or status == AlgoStatus::Expired or status == AlgoStatus::Terminated or status == AlgoStatus::Error;
        }
        static bool isRunning(int status) {
            if (isFinalStatus(status))
                return false;
            return status != AlgoStatus::Canceling;
        }
    };

    struct AXAcctType {

        static constexpr  char UFXM_STOCK[]  = "UM0";   // 股票集中交易账户    
        static constexpr  char UFXM_CREDIT[] = "UMC";   // 信用集中交易账户

        static constexpr  char UFXF_STOCK[] = "UF0";    // 金证极速普通账户
        static constexpr  char UFXF_CREDIT[] = "UFC";   // 金证极速信用账户

        static constexpr  char ATPF_STOCK[] = "AT0";    // 华锐极速普通账户
        static constexpr  char ATPF_CREDIT[] = "ATC";   // 华锐极速信用账户

    };

    class OrderStatus {
    public:
        static const int UNSENT = -4;
        static const int SENTFAILED = -3;
        static const int SENTUNKNOWN = -2;
        static const int SENTOK = -1;
        static const int REPORTING = 7;
        static const int NEW = 0;
        static const int CANCELING = 8;
        static const int PARTIALFILLED = 1;
        static const int FILLED = 2;
        static const int PARTIALCANCEL = 3;
        static const int CANCELED = 4;
        static const int REJECTED = 5;

        static std::vector<int> getUnfinishStatus(){
            return { UNSENT, SENTUNKNOWN, SENTOK, REPORTING, NEW, CANCELING, PARTIALFILLED };
        }

        static bool isUnfinishStatus(int status){
            return not isFinalStatus(status);
        }

        static bool isCanceledStatus(int status){
            return (status == CANCELED || status == PARTIALCANCEL);
        }

        static bool isFinalStatus(int status){
            return (status == FILLED || status == REJECTED || status == CANCELED || status == PARTIALCANCEL);
        }
    };

    class TradeSide {
    public:
        static constexpr   int BUY = 1;
        static constexpr   int SELL = 2;
        static constexpr   int MARGINBUY = 3;
        static constexpr   int MARGINSELL = 4;
        static constexpr   int DEBITBUY = 5;
        static constexpr   int DEBITSELL = 6;

        static std::string name(size_t ts) {
            if (isValid(ts)) {
                return maptoName[ts-1];
            }
            return "unKnonw";
        }
        static bool isValid(size_t ts) {
            return ts >= 1 and ts < maptoName.size();
        }
    private:
        static const std::array<std::string, 6> maptoName ;
    };

    static bool isBuy(int tradeSide) {
        return tradeSide == TradeSide::BUY || tradeSide == TradeSide::MARGINBUY || tradeSide == TradeSide::DEBITBUY;
    }

    static bool isSell(int tradeSide) {
        return tradeSide == TradeSide::SELL || tradeSide == TradeSide::MARGINSELL || tradeSide == TradeSide::DEBITSELL;
    }

    enum class QuoteMode {
        LIVE,
        SIMLIVE,
        REPLAY,
        SIMCUST
    };

    static std::string getQuoteModeName(const QuoteMode& quoteMode) {

        switch (quoteMode) {

            case QuoteMode::LIVE:    return "LIVE";
            case QuoteMode::SIMLIVE: return "SIMLIVE";
            case QuoteMode::REPLAY:  return "REPLAY";
            case QuoteMode::SIMCUST: return "SIMCUST";
            default:                 return "UNKNOWN";
        }

    }

    static QuoteMode getQuoteMode(const AlgoMsg::AcctType& acctType) {

        auto& acctTypeName = AlgoMsg::AcctType_Name(acctType);

        if (acctTypeName.starts_with("BT"))      return QuoteMode::REPLAY;

        if (acctTypeName.starts_with("SIMLIVE")) return QuoteMode::SIMLIVE;

        if (acctTypeName.starts_with("SIMCUST")) return QuoteMode::SIMCUST;

        return QuoteMode::LIVE;
    }

    static bool isBackTestAcctType(const AlgoMsg::AcctType& acctType) {

        auto& acctTypeName = AlgoMsg::AcctType_Name(acctType);

        if (acctTypeName.starts_with("BT"))      
            return true;

        return false;
    }

    enum class QuoteSide {
        Bid,
        Ask,
        Both
    };

    enum class MarketExchange {
        SZSE = 0,
        SHSE = 1,
        unDefined = 99
    };


/*
交易所简码 SZ / SH
*/
    std::string getMarketExchangeStrCode(const MarketExchange& exchange);

 /*!
 SHSE和SZSE开头的股票代码转为.SH和.SZ 的形式
*/
    Symbol_t toLocalSymbol(const std::string& code);

    class AshareMarketTime {
    public:

        static posix_time::ptime getOpeningCallAuctionBeginTime(const posix_time::ptime& refTime = posix_time::second_clock::local_time()) {
            posix_time::time_duration td(9, 15, 0, 0);
            return refTime - refTime.time_of_day() + td;
        }

        static posix_time::ptime getOpeningCallAuctionEndTime(const posix_time::ptime& refTime = posix_time::second_clock::local_time()) {
            posix_time::time_duration td(9, 25, 0, 0);
            return refTime - refTime.time_of_day() + td;
        }

        static posix_time::ptime getMarketOpenTime(const posix_time::ptime& refTime = posix_time::second_clock::local_time()) {
            posix_time::time_duration td(9, 30, 0, 0);
            return refTime - refTime.time_of_day() + td;
        }

        static posix_time::ptime getMarketMorningCloseTime(const posix_time::ptime& refTime = posix_time::second_clock::local_time()) {
            posix_time::time_duration td(11, 30, 0, 0);
            return refTime - refTime.time_of_day() + td;
        }

        static bool isAfternoonBreakTime(const posix_time::ptime& refTime = posix_time::second_clock::local_time()) {
            return refTime > getMarketMorningCloseTime(refTime) and refTime < getMarketAfternoonOpenTime(refTime);
        }

        static posix_time::ptime getMarketAfternoonOpenTime(const posix_time::ptime& refTime = posix_time::second_clock::local_time()) {
            posix_time::time_duration td(13, 0, 0, 0);
            return refTime - refTime.time_of_day() + td;
        }

        static posix_time::ptime getClosingCallAuctionBeginTime(const posix_time::ptime& refTime = posix_time::second_clock::local_time()) {
            posix_time::time_duration td(14, 57, 0, 0);
            return refTime - refTime.time_of_day() + td;
        }

        static posix_time::ptime getClosingCallAuctionEndTime(const posix_time::ptime& refTime = posix_time::second_clock::local_time()) {
            posix_time::time_duration td(15, 0, 0, 0);
            return refTime - refTime.time_of_day() + td;
        }

        static posix_time::ptime getMarketCloseTime(const posix_time::ptime& refTime = posix_time::second_clock::local_time()) {
            posix_time::time_duration td(15, 0, 0, 0);
            return refTime - refTime.time_of_day() + td;
        }

        static bool isInMarketOpenAuctionPeriod(const posix_time::ptime& refTime = posix_time::second_clock::local_time()){
            return refTime>= getOpeningCallAuctionBeginTime(refTime) && refTime <= getClosingCallAuctionEndTime(refTime);
        }

        static bool isInMarketContinueTradingTime(const posix_time::ptime& refTime = posix_time::second_clock::local_time()) {
            return refTime >= getMarketOpenTime(refTime) && refTime <= getClosingCallAuctionBeginTime(refTime);
        }

        static bool isInMarketTime(const posix_time::ptime& refTime = posix_time::second_clock::local_time()) {
            return (refTime >= getOpeningCallAuctionBeginTime(refTime) && refTime <= getMarketMorningCloseTime(refTime)) or 
                (refTime >= getMarketAfternoonOpenTime(refTime) && refTime <= getMarketCloseTime(refTime));
        }

        /*!
        返回  startTime + 市场时长_secs(秒数) 对应的时点
        */
        static posix_time::ptime addMarketDuration(const posix_time::ptime& refTime, double _secs) {

            auto marketOpenTime = getMarketOpenTime(refTime);
            auto marketCloseTime = getMarketCloseTime(refTime);
            auto marketMorningCloseTime = getMarketMorningCloseTime(refTime);
            auto marketAfternoonOpenTime = getMarketAfternoonOpenTime(refTime);

            int secs = (int)floor(_secs);
            int fractionalPart = (int)((_secs - secs) * 1000);

            auto endTime = refTime + posix_time::seconds(secs) + posix_time::milliseconds(fractionalPart);

            if (refTime >= marketMorningCloseTime and refTime < marketAfternoonOpenTime) {
                if (secs > 0) {
                    endTime = marketAfternoonOpenTime + posix_time::seconds(secs) + posix_time::milliseconds(fractionalPart);
                }
                else {
                    endTime = marketMorningCloseTime + posix_time::seconds(secs) + posix_time::milliseconds(fractionalPart);
                }
            }
            else if (refTime < marketMorningCloseTime && endTime > marketMorningCloseTime) {
                endTime += (marketAfternoonOpenTime - marketMorningCloseTime);
            }
            else if (refTime > marketAfternoonOpenTime && endTime < marketAfternoonOpenTime) {
                endTime -= (marketAfternoonOpenTime - marketMorningCloseTime);
            }

            endTime = std::min(endTime, marketCloseTime);
            endTime = std::max(endTime, marketOpenTime);

            return endTime;
        }

        /*!
            返回  startTime 后的市场交易时点
        */
        static posix_time::ptime getAvailableStartMarketTime(const posix_time::ptime& startTime = posix_time::second_clock::local_time()) {

            auto marketOpenTime = getMarketOpenTime(startTime);
            auto afternoonOpenTime = getMarketAfternoonOpenTime(startTime);

            if (startTime < marketOpenTime) {
                return marketOpenTime;
            }
            else if (startTime >= getMarketMorningCloseTime(startTime) && startTime <= afternoonOpenTime) {
                return afternoonOpenTime;
            }
            return startTime;
        }

        /*!
            返回  endTime - startTime 的市场时长(秒数)
        */
        static double getMarketDuration(const posix_time::ptime& startTime, posix_time::ptime endTime) {

            if (startTime> endTime)
                return 0;

            auto marketOpenTime  = getMarketOpenTime(startTime);
            auto marketCloseTime = getMarketCloseTime(startTime);
            auto marketMorningCloseTime = getMarketMorningCloseTime(startTime);
            auto marketAfternoonOpenTime = getMarketAfternoonOpenTime(startTime);

            endTime = endTime < marketCloseTime ? endTime : marketCloseTime;

            int64_t duration = (endTime - startTime).total_milliseconds();

            if (startTime < marketOpenTime) {
                duration -= (marketOpenTime - startTime).total_milliseconds();
            }
            if (endTime > marketMorningCloseTime && endTime < marketAfternoonOpenTime) {
                duration = (marketMorningCloseTime - startTime).total_milliseconds();
            }
            else if (startTime < marketMorningCloseTime && endTime >= marketAfternoonOpenTime) {
                duration = duration - (marketAfternoonOpenTime - marketMorningCloseTime).total_milliseconds();
            }
            else if (startTime > marketMorningCloseTime && startTime <= marketAfternoonOpenTime && endTime >= marketAfternoonOpenTime) {
                duration = (endTime - marketAfternoonOpenTime).total_milliseconds();
            }
            return (double)duration / 1000.0;
        }

        /*!
           返回 当天 9:30开市到 curTime 的市场时长(秒数)，未开市为 0
        */
        static double getDurationSinceMarketOpen(const posix_time::ptime& curTime) {

            posix_time::ptime marketOpenTime = getMarketOpenTime(curTime);
            if (curTime <= marketOpenTime) {
                return 0;
            }
            return getMarketDuration(marketOpenTime, curTime);
        }

        static posix_time::ptime convert2ShanghaiTZ(uint32_t timestamp) {
            static boost::local_time::time_zone_ptr shanghai_zone(new boost::local_time::posix_time_zone("CST+8"));
            std::time_t time = static_cast<std::time_t>(timestamp);
            boost::posix_time::ptime pt = boost::posix_time::from_time_t(time);
            pt = pt + shanghai_zone->base_utc_offset();
            return pt;
        }

    };

/*!
       返回  startTime + _secs(秒数) 对应的时点
*/
    posix_time::ptime addDuration(const posix_time::ptime& startTime, double _secs);

/*!
   返回  endTime - startTime 的时长(秒数)
*/
    double getSecondsDiff(const posix_time::ptime& startTime, const posix_time::ptime& endTime );

    posix_time::ptime now();

    std::optional<posix_time::ptime> parseDateTimeStr(const std::string& input);

    std::optional<posix_time::ptime> parseDateTimeInteger(const uint64_t& yyyymmddhhmmss);

    std::optional<OrderDate_t> parseDateStr(const std::string& input);

    std::uint64_t ptime2Integer(const posix_time::ptime& pt);

    uint64_t  getDateTimeInt(const posix_time::ptime& pt);

    uint32_t  getDateInt(const posix_time::ptime& pt);

    uint64_t  getTimeInt(const posix_time::ptime& pt);

    uint64_t getTotalMilliSeconds();

    std::string getDateTimeStr(const posix_time::ptime& pt);

    std::string getTimeStr(const posix_time::ptime& pt);

    std::string getTodayStr();

    std::string getSimpleDateStr(const posix_time::ptime& pt);

/*
    返回pt的 日期字符串 yyyymmdd
*/
    std::string geISODateStr(const posix_time::ptime& pt);

/*
    返回pt的 日期字符串 yyyymmdd
*/
    std::string geDateStr(const posix_time::ptime& pt);

    void textSplit(const std::string& s, const char delimiter, std::vector<std::string>& tokens);

    std::unordered_map<std::string, std::string> parseStringToMap(const std::string& input);

    struct TimeCost {

        TimeCost(const std::string& _title, const std::string& describe = "",bool isDefaultPrint = true
            , std::source_location _loc = std::source_location::current()) :
            title(_title)
            , describe(describe)
            , start(std::chrono::steady_clock::now())
            , end(start)
            , isDefaultPrint(isDefaultPrint)
            , loc(_loc) {
        }

        TimeCost(const TimeCost& rhs) = default;

        ~TimeCost() {

            if (isDefaultPrint) {
                logTimeCost();
            }
        }

        void timeAt(const std::string& _desc = "") {

            if (_desc != "") describe += _desc;

            end = std::chrono::steady_clock::now();
        }

        void timeAt(std::chrono::steady_clock::time_point atTime) {

            end = atTime;
        }
        void logTimeCost(const std::string& suftext ="") {

            if (end == start) {
				end = std::chrono::steady_clock::now();
			}

            auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration_us);
            auto duration_s  = std::chrono::duration_cast<std::chrono::seconds>(duration_us);

            std::string time_str;
            if (duration_s.count() >= 10) {
                time_str = std::to_string(duration_s.count()) + "s";
            }
            else if (duration_ms.count() >= 1) {
                time_str = std::to_string(duration_ms.count()) + "ms";
            }
            else {
                time_str = std::to_string(duration_us.count()) + "us";
            }

            std::string file_name = std::filesystem::path(loc.file_name()).filename().string();

			SPDLOG_INFO("[TC]{}{}:{} {}@ {},{}", title, describe, time_str, suftext, file_name, loc.line());
		}

        std::string title;

        std::string describe;

        bool        isDefaultPrint{ true };

        std::source_location loc;

        std::chrono::steady_clock::time_point start{ std::chrono::steady_clock::now() };
        std::chrono::steady_clock::time_point end  { std::chrono::steady_clock::now() };
    };

    std::string toLower(const std::string& input);

    std::string toUpper(const std::string& input);

    double getTimeProgress(const posix_time::ptime& startTime, const posix_time::ptime& endTime);

    inline int32_t get_int(const std::string& str, int default_value = 0) {
        try {
            return std::stol(str);
        }
        catch (const std::exception& e) {
            SPDLOG_ERROR("[get_int]Invalid argument:{},{}", e.what(), str);
        }
        return default_value;
    }
    inline int64_t get_int64(const std::string& str, int default_value = 0) {
        try {
            return std::stoll(str);
        }
        catch (const std::exception& e) {
            SPDLOG_ERROR("[get_int]Invalid argument:{},{}", e.what(), str);
        }
        return default_value;
    }

    inline double get_double(const std::string& str, double default_value = 0.0) {
        try {
            return std::stod(str);
        }
        catch (const std::exception& e) {
            SPDLOG_ERROR("[get_double]Invalid argument:{},{}", e.what(), str);
        }
        return default_value;
    }
    inline float get_float(const std::string& str, float default_value = 0.0) {
        try {
            return std::stof(str);
        }
        catch (const std::exception& e) {
            SPDLOG_ERROR("[get_float]Invalid argument:{},{}", e.what(), str);
        }
        return default_value;
    }


} // namespace common
