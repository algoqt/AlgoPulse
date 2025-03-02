#include "common.h"
#include <algorithm>

namespace agcommon
{
    const std::array<std::string, 6> TradeSide::maptoName = { "BUY","SELL","MARGINBUY","MARGINSELL","DEBITBUY","DEBITSELL" };

    std::string getMarketExchangeStrCode(const MarketExchange& ex) {
        switch (ex) {
        case MarketExchange::SZSE:
            return "SZ";
        case MarketExchange::SHSE:
            return "SH";
        default:
            return "Invalid";
        }
    }

 /*!
   SHSE和SZSE开头的股票代码转为.SH和.SZ 的形式
*/
    Symbol_t toLocalSymbol(const std::string& code) {
        Symbol_t result;

        if (code.substr(0, 5) == "SHSE.") {
            result = code.substr(5) + ".SH";
        }
        else if (code.substr(0, 5) == "SZSE.") {
            result = code.substr(5) + ".SZ";
        }
        else {
            return "";
        }
        return result;
    }

    double getSecondsDiff(const posix_time::ptime& startTime, const posix_time::ptime& endTime) {

        double duration = (endTime - startTime).total_milliseconds();
        return duration / 1000.0;
    }
    double getTimeProgress(const posix_time::ptime& startTime, const posix_time::ptime& endTime) {

		double duration = (endTime - startTime).total_milliseconds()*1.0;
        double passed = (posix_time::second_clock::local_time() - startTime).total_milliseconds()*1.0;
		return duration > 0 ? std::min(passed/duration,1.0) : 0;
	}

    posix_time::ptime addDuration(const posix_time::ptime& startTime, double _secs) {

        int secs = floor(_secs);
        int fractionalPart = (_secs - secs) * 1000*1000;
        posix_time::time_duration td(0, 0, secs, fractionalPart);
        return startTime + td;
    }

    posix_time::ptime now() {
        return boost::posix_time::microsec_clock::local_time();
    };


/*
    字符串转时间, YYYYMMDD-HHMMSS || YYYYMMDD HHMMSS  || YYYYMMDDTHHMMSS  || YYYY-MM-DDTHH:MM:SS  
 */
    std::optional<posix_time::ptime> parseDateTimeStr(const std::string& input)
    {
        try {
            if (input.find(' ') != std::string::npos && input.find('-') != std::string::npos) {
                return posix_time::time_from_string(input);
            }
            if (input.find('T') != std::string::npos && input.find(':') == std::string::npos && input.find('-') == std::string::npos)
            {
                return posix_time::from_iso_string(input);
            }
            if (input.find('T') != std::string::npos && (input.find(':') != std::string::npos || input.find('-') != std::string::npos))
            {
                return posix_time::from_iso_extended_string(input);
            }

            return boost::date_time::parse_iso_time<posix_time::ptime>(input, ' ');  // from_iso_extended_string(input);
        }
        catch (...) {
            return std::nullopt;
        }
    }

/*!
    整数 yyyymmddhhmmss 转时间
*/
    std::optional<posix_time::ptime> parseDateTimeInteger(const uint64_t& yyyymmddhhmmss)
    {
        auto inputstr= std::to_string(yyyymmddhhmmss);
        try {
            return posix_time::from_iso_string(inputstr.substr(0, 8) + "T" + inputstr.substr(8));
        }
        catch (...) {
            return std::nullopt;
        }
    }

/*!
    当天凌晨至今的毫秒数
*/
    uint64_t getTotalMilliSeconds() {
        auto _date = now();
        return boost::posix_time::to_time_t(_date) * 1000 + _date.time_of_day().total_milliseconds();
    }
    

 /*
    字符串 -》日期
*/
    std::optional<OrderDate_t> parseDateStr(const std::string& ymdhms)
    {
        auto tmp = parseDateTimeStr(ymdhms);
        if (tmp)
            return (*tmp).date();

        return std::nullopt;
    }


/*
    返回pt的 日期字符串 yyyymmdd
*/
    std::string geISODateStr(const posix_time::ptime& pt) {

        return posix_time::to_iso_string(pt).substr(0, 8); //yyyymmdd
    }

/*
    返回pt的 日期字符串 yyyymmdd
*/
    std::string geDateStr(const posix_time::ptime& pt) {

        return posix_time::to_iso_string(pt).substr(0, 8); //yyyymmdd
    }

/*
    返回当前时间的日期字符串 yyyymmdd
*/
    std::string getTodayStr() {

        return posix_time::to_iso_string(posix_time::second_clock::local_time()).substr(0, 8);
    }

/*
    返回字符串 yyyy-mm-dd
*/
    std::string getSimpleDateStr(const posix_time::ptime& pt) {

        return posix_time::to_iso_string(pt).substr(0, 10);
    }

/*
    返回时间字符串 hhmmss
*/
    std::string  getTimeStr(const posix_time::ptime& pt) {
        return posix_time::to_iso_string(pt).substr(9,6); // hhmmss
    }

 /*
    返回字符串 yyyymmddhhmmss
 */
    std::string  getDateTimeStr(const posix_time::ptime& pt) {
        return geISODateStr(pt) + getTimeStr(pt);
    }

/*
    返回整数 yyyymmddhhmmss
*/
    uint64_t getDateTimeInt(const posix_time::ptime& pt) {
        return std::stoull(geISODateStr(pt) + getTimeStr(pt));
    }

/*
    返回整数 yyyymmddhhmmss
*/
    std::uint64_t ptime2Integer(const posix_time::ptime& pt) {

        return std::stoull(getDateTimeStr(pt));
    }

/*
    返回 yyyymmdd
*/
    uint32_t  getDateInt(const posix_time::ptime& pt) {
        return std::stoul(geISODateStr(pt));
    }

/*
    返回 hhmmss
*/
    uint64_t  getTimeInt(const posix_time::ptime& pt) {
        return std::stoull(getTimeStr(pt));
    }
    
/*
    字符串按 单字符 delimiter 分割
*/
    void textSplit(const std::string& s, const char delimiter, std::vector<std::string>& tokens) {
        std::string token;
        std::istringstream tokenStream(s);
        while (std::getline(tokenStream, token, delimiter)) {
            tokens.push_back(token);
        }
    }

    std::string toLower(const std::string& input) {
        std::string output = input;
        std::transform(input.begin(), input.end(), output.begin(), ::tolower);
        return output;
    }

    std::string toUpper(const std::string& input) {
        std::string output = input;
        std::transform(input.begin(), input.end(), output.begin(), ::toupper);
        return output;
    }

    std::unordered_map<std::string, std::string> parseStringToMap(const std::string& input) {
        std::unordered_map<std::string, std::string> result;
        std::istringstream ss(input);
        std::string pair;

        while (std::getline(ss, pair, ',')) {

            std::string::size_type pos = pair.find('=');

            if (pos != std::string::npos) {

                std::string key = pair.substr(0, pos);
                std::string value = pair.substr(pos + 1);

                // Remove leading and trailing spaces from key and value
                key.erase(key.begin()
                    ,std::find_if(key.begin(), key.end(), [](unsigned char ch) { return !std::isspace(ch); })
                );
                key.erase(std::find_if(key.rbegin(), key.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base()
                    , key.end());

                value.erase(value.begin()
                    ,std::find_if(value.begin(), value.end(), [](unsigned char ch) { return !std::isspace(ch); }));
                value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base()
                    , value.end());

                // Insert into map, if key already exists it will be updated with the new value
                result[key] = value;
                SPDLOG_DEBUG("parse key: {} = {}", key, value);
            }
        }

        return result;
    }

}
