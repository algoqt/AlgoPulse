#pragma once

#include <iostream>
#include <vector>
#include <tuple>
#include <filesystem>
#include <boost/asio.hpp>
#include <boost/date_time.hpp>

#include <spdlog/spdlog.h>
#include "AlgoMessages.pb.h"

#include <iostream>

#if defined(__GNUC__) || defined(__clang__)
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#endif

namespace asio = boost::asio;

using AsioContextPtr = std::shared_ptr<asio::io_context>;

using strand_type = asio::strand<asio::io_context::executor_type>;
using OrderDate_t = boost::gregorian::date;
using OrderTime_t = boost::posix_time::ptime;
using QuoteTime_t = boost::posix_time::ptime;

#define MIN_DATE_TIME  boost::posix_time::min_date_time;

using AlgoDate_t    = boost::gregorian::date;
using AlgoTime_t    = boost::posix_time::ptime;
using MarketTime_t  = boost::posix_time::ptime;

using OrderDateStr_t    = std::string;
using OrderTimeStr_t    = std::string;

using OrderId_t         = uint64_t;
using AlgoOrderId_t     = uint64_t;

using ClientAlgoOrderId_t = std::string;

using OrderType_t       = char;
using ClientAlgoOrderId_t = std::string;

using Symbol_t      = std::string;
using PositionKey_t = std::pair<std::string, std::string>;

using Broker_t = uint64_t;
using Acct_t = std::string;

struct AcctKey_t {

    AlgoMsg::AcctType acctType;
    Acct_t   acct        ;
    Broker_t broker      ;

    bool operator<(const AcctKey_t& rhs) const {
        if (acctType < rhs.acctType) {
            return true;
        }
        if (acctType == rhs.acctType) {
            if (acct < rhs.acct)
                return true;
            if (acct == rhs.acct)
                return broker < rhs.broker;
        }
        return false;
    }
    bool operator==(const AcctKey_t& rhs) const {
        return acctType == rhs.acctType && acct == rhs.acct && broker == rhs.broker;
    }

    struct Hash {
        size_t operator() (const AcctKey_t& key) const {
            std::size_t res = 17;
            res = res * 31 + std::hash<std::string>()(key.acct);
            res = res * 31 + std::hash<AlgoMsg::AcctType>()(key.acctType);
            res = res * 31 + std::hash<uint64_t>()(key.broker);
            return res;
        }
    };

    friend std::ostream& operator<<(std::ostream& os, const AcctKey_t& key) {

        os << "acct:" << key.acct 
            << ",acctType:" << AlgoMsg::AcctType_Name(key.acctType) 
            << ",broker:" << key.broker;
        return os;
    }

};

template<>
struct fmt::formatter<AcctKey_t> : fmt::formatter<std::string>
{
    auto format(const AcctKey_t& input, format_context& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "acct:{},acctType:{},brokerid:{}", input.acct, AlgoMsg::AcctType_Name(input.acctType), input.broker);
    }
};

template<>
struct fmt::formatter<AlgoMsg::AcctType> : fmt::formatter<std::string>
{
	auto format(const AlgoMsg::AcctType& input, format_context& ctx) const -> decltype(ctx.out())
	{
		return fmt::format_to(ctx.out(), "{}", AlgoMsg::AcctType_Name(input));
	}
};

using ClientAlgoOrderKey_t = std::pair<AcctKey_t, ClientAlgoOrderId_t>;

using OrderBookKey_t = AcctKey_t;

using SymbolKey_t = std::pair<AcctKey_t,Symbol_t>;

using AlgoErrorMessage_t = std::string;

enum class AlgoErrorCode {

    ALGO_OK                 = 0,
    ALGO_QUOTE_ERROR        = 101,
    ALGO_DUPLICATE_ERROR      = 103,
    ALGO_ORDERSERVICE_ANORMAL = 105,
    ALGO_ORDERCHECK_FAILED  = 107,
    ALGO_ACCT_NOT_LOGIN     = 109,
    ALGO_ACCT_ALREADY_LOGIN = 110,
    ALGO_ORDERROUTER_ERROR  = 111,
    ALGO_NOT_EXISTS = 113,
    ALGO_ERROR = 199,
};
