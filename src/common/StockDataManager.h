#pragma once

#include "typedefs.h"
#include "common.h"
#include <cmath>
#include "StockDataTypes.h"
#include "H5DataTypes.h"
#include "MarketDepth.h"


using SecurityInfoHashMap = std::unordered_map<Symbol_t, std::shared_ptr<SecurityStaticInfo>>;

using Symbol2DailyBarHashMap = std::unordered_map<Symbol_t, std::shared_ptr<DailyBar>>;

using Symbol2MinuteBarVec = std::unordered_map<Symbol_t, std::vector<std::shared_ptr<MinuteBar>>>;

using Time2Symbol2MinuteBarMap = std::map<QuoteTime_t, std::unordered_map<Symbol_t, std::shared_ptr<MinuteBar>>>;

std::string symbolH5Key(const std::string& symbol);

//struct crossFeature {
//    double range{ 0 };
//    double ret{ 0 };
//    int touchHighLimits{ 0 };
//    int touchLowLimits{ 0 };
//    int closeAtHighLimits{ 0 };
//    int closeAtLowLimits{ 0 };
//    bool closeAtHighLimit_yesterday{ false };
//    bool closeAtLowLimit_yesterday { false };
//
//};

class StockDataManager {

public:

    static StockDataManager& getInstance() {
        static StockDataManager instance;
        return instance;
    }
    StockDataManager(const StockDataManager&) = delete;
    StockDataManager& operator=(const StockDataManager&) = delete;

    /*股票静态信息*/
    // xx.SH/SZ yyyymmdd 
    const SecurityStaticInfo* getSecurityStaticInfo(const Symbol_t& symbol, const uint32_t trade_dt);
    // symbolset  yyyymmdd
    const SecurityInfoHashMap getSecurityInfoBatch(const std::unordered_set<Symbol_t>& symbolSet, const uint32_t trade_dt);

    // yyyymmdd
    const SecurityInfoHashMap getSecurityBlockInfo(const uint32_t trade_dt);
    //1: 股票, 2: 基金, 3: 指数, 4: 期货, 5: 期权, 8：可转债， 10: 虚拟合约
    const SecurityInfoHashMap getSecurityBlockInfo(const uint32_t trade_dt, const std::unordered_set<int>& securityTypeSets);

    // hs300 zz500  zz1000 All Stock ....
    const SecurityInfoHashMap getSecurityBlockInfo(const uint32_t trade_dt, const std::string& specialName);

    /*指数成分信息 hs300   zz500   zz1000*/
    const std::unordered_set<Symbol_t> getIndexConstituents(const uint32_t trade_dt, const std::string& indexName);

    /*交易日历*/
    const uint32_t getNextTradeDate(const uint32_t trade_dt) ;
    const uint32_t getPreTradeDate(const uint32_t trade_dt) ;
    const uint32_t getPreTradeDate_N(const uint32_t end_date, const std::size_t lastN);

    const std::vector<h5data::TradeCalendar> getTradeDates(const uint32_t begin_date, const uint32_t end_date);

    const std::set<uint32_t> getTradeDateInts(const uint32_t begin_date, const uint32_t end_date);

    /*日K线*/
    const std::shared_ptr<DailyBar>   getDailyBar(const Symbol_t& symbol, const uint32_t trade_dt);
    const Symbol2DailyBarHashMap getDailyBarBlock(const uint32_t trade_dt);
    const Symbol2DailyBarHashMap getDailyBarBlock(const uint32_t trade_dt, const std::unordered_set<Symbol_t>& symbolSets);
    const Symbol2DailyBarHashMap getDailyBarBlock(const uint32_t trade_dt, const std::string& indexName);
    const Symbol2DailyBarHashMap getDailyBarBlock(const uint32_t trade_dt, const std::unordered_set<int>& securityTypes);
    /*1分钟K线*/
    const Time2Symbol2MinuteBarMap& getMinuteBar(const uint32_t trade_dt);

    /*Tick*/
    size_t cacheFromH5Tick(const std::unordered_set<Symbol_t>& symbols
        , const QuoteTime_t& startTime
        , const QuoteTime_t& endTime
        , std::map<QuoteTime_t, UnorderMarketDepthRawPtrMap>& quoteTime2Symbol2md
    );

    size_t cacheFromH5Tick(const std::unordered_set<Symbol_t>& symbols
        , const QuoteTime_t& startTime
        , const QuoteTime_t& endTime
        , std::map<QuoteTime_t, UnorderMarketDepthPtrMap>& quoteTime2Symbol2mdPtr);

    size_t cacheFromH5Tick(const std::unordered_set<Symbol_t>& symbols
        , const QuoteTime_t& startTime
        , const QuoteTime_t& endTime
        , boost::unordered_map<Symbol_t, std::map<QuoteTime_t,MarketDepthKeepAlivePtr>>& symbol2quoteTime2md
    );
    //size_t cacheFromH5Tick(const std::unordered_set<Symbol_t>& symbols
    //    , const QuoteTime_t& startTime
    //    , const QuoteTime_t& endTime
    //    , std::map<QuoteTime_t, UnorderMarketDepthMap>& quoteTime2Symbol2md
    //);

    std::unordered_set<Symbol_t> filterReturnTop(size_t windowDays, uint32_t topN,uint32_t end_Date) {

        std::unordered_set<int> stockType{ h5data::SecurityType::STOCK };
        auto beg_Date    = getPreTradeDate_N(end_Date, windowDays);
        auto begDailyBar = getDailyBarBlock(beg_Date, stockType);
        auto endDailyBar = getDailyBarBlock(end_Date, stockType);

        std::unordered_set<Symbol_t> result;
        std::vector<std::pair<Symbol_t, double>> vec;
        vec.reserve(endDailyBar.size());
        for (const auto& [symbol,bar] : endDailyBar) {
            const auto it = begDailyBar.find(symbol);
            if (it != begDailyBar.end()) {
                if (it->second->close * it->second->adjFactor > 0) {
                    double ret = bar->close * bar->adjFactor / (it->second->close * it->second->adjFactor) - 1;
                    vec.push_back({ symbol,ret });
                }
            }
        }
        topN = std::min((size_t)topN, vec.size());
        std::partial_sort(vec.begin(), vec.begin() + topN, vec.end(), [](auto& a, auto& b) {
            return a.second > b.second; 
            });

        if (topN > 0) {
            for (auto i = 0; i < topN; i++) {

                result.insert(vec[i].first);
            }
            SPDLOG_INFO("TOP RETURN,fisrt:{},{:.4f},tail:{},{:.4f}", vec[0].first, vec[0].second, vec[topN - 1].first, vec[topN - 1].second);
        }
        return result;
    }

    std::unordered_set<Symbol_t> filterRetrunRangeUpper(size_t windowDays, double range, uint32_t end_Date) {

        std::unordered_set<int> stockType{ h5data::SecurityType::STOCK };
        auto beg_Date = getPreTradeDate_N(end_Date, windowDays);

        std::unordered_set<Symbol_t> result;
        std::unordered_map<Symbol_t,std::pair<double,double>> symbol2MinMax;

        auto dates = getTradeDates(beg_Date, end_Date);
        for (const auto& date : dates) {
            auto bars = getDailyBarBlock(beg_Date, stockType);

            for (const auto& [symbol, bar] : bars) {

                auto [it,inserted] = symbol2MinMax.try_emplace(symbol, std::make_pair(1000000.0,0));
                if (it->second.first > bar->low * bar->adjFactor)
                    it->second.first = bar->low * bar->adjFactor;
                if (it->second.second < bar->high * bar->adjFactor)
                    it->second.second = bar->high * bar->adjFactor;
            }

        }

        for (const auto& [symbol, minMax] : symbol2MinMax) {
            auto& [_min, _max] = minMax;
            if (_min > 0 and _max / _min - 1 < range) {
                result.insert(symbol);
            }
        }

        return result;
    }


    std::unordered_map<Symbol_t,std::pair<double,double>> getStockReturnRange(uint32_t beg_Date
        , uint32_t end_Date
        , const std::unordered_set<Symbol_t>& symbols) {

        std::unordered_map<Symbol_t, std::pair<double, double>> result;

        std::unordered_map<Symbol_t, std::pair<double, double>> symbol2MinMax;  // min,max
        std::unordered_map<Symbol_t, std::pair<double, double>> symbol24Return; // window begin

        auto dates = getTradeDates(beg_Date, end_Date);

        //std::sort(dates.begin(), dates.end());

        for (const auto& date : dates) {
            SPDLOG_INFO("pre:{} ,date:{}", date.pre_trade_date,date.trade_date);
            auto bars = getDailyBarBlock(date.trade_date, symbols);

            for (const auto& [symbol, bar] : bars) {

                auto [it, inserted] = symbol2MinMax.try_emplace(symbol, std::make_pair(10000000.0, 0));
                if (it->second.first > bar->low * bar->adjFactor)
                    it->second.first = bar->low * bar->adjFactor;
                if (it->second.second < bar->high * bar->adjFactor)
                    it->second.second = bar->high * bar->adjFactor;

                auto [it2, inserted2] = symbol24Return.try_emplace(symbol, std::make_pair(0.0, 0.0));
                if (it2->second.first == 0.0) {
                    it2->second.first = bar->preclose * bar->adjFactor;
                }
                it2->second.second = bar->close * bar->adjFactor;
            }
        }
        for (const auto& symbol : symbols) {
            double ret = 0;
            double range = 0;
            auto& retClose = symbol24Return[symbol];
            if (retClose.first > 0) {
                ret = retClose.second / retClose.first - 1.0;
            }
            auto& minMax = symbol2MinMax[symbol];
            if (minMax.first > 0) {
                range = minMax.second / minMax.first - 1.0;
            }
            result.emplace(symbol, std::make_pair(ret, range));
        }

        return result;
    }

private:

    StockDataManager() = default;
    ~StockDataManager() = default;

    std::mutex m_mutex;

    const SecurityInfoHashMap cacheSecurityStaticInfo(const int trade_dt);
    void cacheTradeCalendar();
    const Symbol2DailyBarHashMap       cacheDailyBar(const int trade_dt);

    const Time2Symbol2MinuteBarMap&  cacheMinuteBar(const int trade_dt);

    std::unordered_map<int, SecurityInfoHashMap>    date2securityStaticInfoMap{};

    std::map<uint32_t, h5data::TradeCalendar>       tradeCalendar{};

    std::unordered_map<int, Symbol2DailyBarHashMap> date2DailBarMap{};

    std::unordered_map<Symbol_t, std::vector<DailyBar>> symbol2DailyBars{};

    std::unordered_map<int, Time2Symbol2MinuteBarMap>   date2Time2MinuteBars{};

    inline std::pair<int32_t,int32_t> getIndexName2Tag(const std::string& indexName) {
        auto index_tag = -9;
        auto size_hint = 0;
        if (indexName == "hs300") {
            index_tag = h5data::IndexTag::HS300;
            size_hint = 300;
        }
        else if (indexName == "zz500") {
            index_tag = 2;
            size_hint = h5data::IndexTag::ZZ500;
        }
        else if (indexName == "zz1000") {
            index_tag = 4;
            size_hint = h5data::IndexTag::ZZ1000;
        }
        return std::make_pair(index_tag, size_hint);
    }
};
