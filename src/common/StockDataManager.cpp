#include "StockDataManager.h"
#include "StockDataTypes.h"
#include <highfive/highfive.hpp>

#include "Configs.h"

std::string symbolH5Key(const std::string& symbol) {
    if (symbol.ends_with("SZ")) {
        return "/SZSE" + symbol.substr(0, 6);
    }
    if (symbol.ends_with("SH")) {
        return "/SHSE" + symbol.substr(0, 6);
    }
    return "/" + symbol;
}

const SecurityInfoHashMap StockDataManager::getSecurityBlockInfo(const uint32_t trade_dt) {

    std::scoped_lock<std::mutex> lock(m_mutex);

    auto it = date2securityStaticInfoMap.find(trade_dt);

    if (it == date2securityStaticInfoMap.end()) {

        auto s2info = cacheSecurityStaticInfo(trade_dt);
        return s2info;
    }
    else {
        return it->second;
    }
}

const SecurityStaticInfo* StockDataManager::getSecurityStaticInfo(const Symbol_t& symbol, const uint32_t trade_dt) {

    std::scoped_lock<std::mutex> lock(m_mutex);

    auto it = date2securityStaticInfoMap.find(trade_dt);

    if (it == date2securityStaticInfoMap.end()) {

        auto securityStaticInfos=cacheSecurityStaticInfo(trade_dt);

        if (auto it = securityStaticInfos.find(symbol);it != securityStaticInfos.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    auto& securityStaticInfos = it->second;
    if (auto it = securityStaticInfos.find(symbol);it != securityStaticInfos.end()) {
        return it->second.get();
    }
    return nullptr;
}

const SecurityInfoHashMap StockDataManager::getSecurityInfoBatch(const std::unordered_set<Symbol_t>& symbolSet, const uint32_t trade_dt) {

    auto securityStaticInfos = getSecurityBlockInfo(trade_dt);

    SecurityInfoHashMap result;

    for (auto& [s, ssinfo] : securityStaticInfos) {
        //SPDLOG_DEBUG("{},{},{}", ssinfo->symbol, ssinfo->securityType, ssinfo->tradeDate);
        if (symbolSet.contains(ssinfo->symbol)) {

            result.emplace(s, ssinfo);
        }
    }
    return result;
}

const SecurityInfoHashMap StockDataManager::getSecurityBlockInfo(const uint32_t trade_dt, const std::unordered_set<int>& securityTypeSets) {
    // 1: 股票, 2: 基金, 3: 指数, 4: 期货, 5: 期权, 8：可转债， 10: 虚拟合约

    auto securityStaticInfos = getSecurityBlockInfo(trade_dt);

    SecurityInfoHashMap result;

    for (auto& [s,ssinfo]: securityStaticInfos) {

        if (securityTypeSets.contains(ssinfo->securityType)) {
            result.emplace(s, ssinfo);
        }
    }
    return result;
}

const SecurityInfoHashMap StockDataManager::getSecurityBlockInfo(const uint32_t trade_dt, const std::string& specialName) {

    auto index_tag = -1;

    std::unordered_set<int> securityTypeSets;

    auto securityStaticInfos = getSecurityBlockInfo(trade_dt);

    SecurityInfoHashMap result;

    if (specialName == "hs300") {
        result.reserve(300);
        index_tag = h5data::IndexTag::HS300;
    }
    else if (specialName == "zz500") {
        result.reserve(500);
        index_tag = h5data::IndexTag::ZZ500;
    }
    else if (specialName == "zz1000") {
        result.reserve(1000);
        index_tag = h5data::IndexTag::ZZ1000;
    }
    else if (specialName == "All") {

        result.reserve(8000);
        securityTypeSets.insert(h5data::SecurityType::STOCK);
        securityTypeSets.insert(h5data::SecurityType::CONVERTBOND);
    }

    else if (specialName == "Stock") {
        result.reserve(6000);
        securityTypeSets.insert(h5data::SecurityType::STOCK);
    }

    else if (specialName == "Conbond") {
        result.reserve(1000);
        securityTypeSets.insert(h5data::SecurityType::CONVERTBOND);
    }
    else if (specialName == "other") {
        result.reserve(5000);
        for (auto& [s, ssinfo] : securityStaticInfos) {
            if (ssinfo->securityType == h5data::SecurityType::STOCK and 
                ssinfo->belongIndex != h5data::IndexTag::HS300 and 
                ssinfo->belongIndex != h5data::IndexTag::ZZ500 and 
                ssinfo->belongIndex != h5data::IndexTag::ZZ1000) {
                result.emplace(s, ssinfo);
            }
        }
        return result;
    }

    for (auto& [s, ssinfo] : securityStaticInfos) {
        //SPDLOG_DEBUG("{},{},{}", ssinfo->symbol, ssinfo->securityType, ssinfo->tradeDate);
        if (ssinfo->belongIndex == index_tag or securityTypeSets.contains(ssinfo->securityType)) {
            result.emplace(s, ssinfo);
        }
    }
    return result;
}


const SecurityInfoHashMap StockDataManager::cacheSecurityStaticInfo(const int trade_dt) {

    auto fileName = agcommon::Configs::getConfigs().getSecurityInfoFile().string();

    if (not std::filesystem::exists(fileName)) {

        SPDLOG_ERROR("SecurityStaticInfo FILE DOESNOT EXISTS:{}", fileName);
        return SecurityInfoHashMap{};
    }

    HighFive::File stockInfoFile(fileName, HighFive::File::ReadOnly);

    auto dataSetName = std::format("/I{}", trade_dt);  // format: Iyyyymmdd

    if (not stockInfoFile.exist(dataSetName)) {
        SPDLOG_ERROR("{} dataset not exist", trade_dt);
        return {};
    }

    auto dataset = stockInfoFile.getDataSet(dataSetName);

    std::vector<size_t> dims = dataset.getDimensions();

    size_t num_records = dims[0];

    if (num_records == 0) {
        SPDLOG_ERROR("{} dataset has 0 data.", trade_dt);
        return {};
    }

    std::vector<h5data::StockInfo> stockInfos;

    stockInfos.reserve(num_records);

    {
        agcommon::TimeCost tc(std::format("read SecurityStaticInfo:{},records:{}", trade_dt, num_records));

        dataset.read<std::vector<h5data::StockInfo>>(stockInfos);

        auto& securityStaticInfoMap = date2securityStaticInfoMap[trade_dt];

        for (const auto& stockInfo : stockInfos) {

            if (stockInfo.trade_date == trade_dt) {

                //dailyStockInfos.push_back(stockInfo);
                auto symbol = agcommon::toLocalSymbol(stockInfo.symbol);

                if (symbol=="") {
                    SPDLOG_ERROR("symbol:{} not supported", stockInfo.symbol);
					continue;
                }

                auto ssinfo = std::make_shared<SecurityStaticInfo>(symbol);

                ssinfo->SecurityName    = stockInfo.sec_name;
                ssinfo->tradeDate       = stockInfo.trade_date;
                ssinfo->adjFactor       = stockInfo.adj_factor;
                ssinfo->securityType    = stockInfo.sec_type;
                ssinfo->highLimitPrice  = stockInfo.upper_limit;
                ssinfo->lowLimitPrice   = stockInfo.lower_limit;
                ssinfo->preClosePrice   = stockInfo.pre_close;
                ssinfo->isSuspended     = false;

                if (stockInfo.is_suspended == 1 or ssinfo->delistedDate <= trade_dt) {
                    ssinfo->isSuspended = true;
                }
                ssinfo->tickSize    = stockInfo.price_tick;
                ssinfo->listedDate   = stockInfo.listed_date;
                ssinfo->delistedDate = stockInfo.delisted_date;
                ssinfo->belongIndex = stockInfo.belong_index;
                ssinfo->underlyingSymbol = stockInfo.underlying_symbol;
                ssinfo->totalShares = stockInfo.ttl_shr;
                ssinfo->unLAShare = stockInfo.a_shr_unl;
                if (ssinfo->securityType == h5data::SecurityType::STOCK and 
                    (ssinfo->totalShares == 0 or ssinfo->unLAShare == 0 or stockInfo.pre_close==0.0)) {
                    SPDLOG_WARN("ssinfo field missing!{},{},totalShares:{},unLAShare:{},preClose:{:.3f}", trade_dt, symbol
                        , ssinfo->totalShares, ssinfo->unLAShare,stockInfo.pre_close);
                }
                securityStaticInfoMap.insert({ symbol,ssinfo });
            }
        }

        return securityStaticInfoMap;
    }
}

const std::unordered_set<Symbol_t> StockDataManager::getIndexConstituents(const uint32_t trade_dt,const std::string& indexName) {

    std::unordered_set<Symbol_t> symbols;

    auto securityStaticInfos = getSecurityBlockInfo(trade_dt);

    auto [index_tag, size] = getIndexName2Tag(indexName);
    symbols.reserve(size);
    
    if(index_tag<0) {
		SPDLOG_ERROR("indexName:{} not supported", indexName);
		return symbols;
	}

    for (const auto& [symbol, ssinfo] : securityStaticInfos) {

        if (ssinfo == nullptr) {
            SPDLOG_ERROR("{},{}, has nullptr securityStaticInfo", trade_dt,symbol);
            continue;
        }
        if (index_tag > 0) {

            if (ssinfo->belongIndex == index_tag) {
                symbols.insert(symbol);
            }
        }
        else {
            // other stocks 
            if (ssinfo->securityType == h5data::SecurityType::STOCK and 
                ssinfo->belongIndex != h5data::IndexTag::HS300 and
                ssinfo->belongIndex != h5data::IndexTag::ZZ500 and
                ssinfo->belongIndex != h5data::IndexTag::ZZ1000) {
                symbols.insert(symbol);
            }
        }
	}
    return symbols;
}

const uint32_t StockDataManager::getNextTradeDate(const uint32_t trade_dt) {

    std::scoped_lock lock(m_mutex);

    if (tradeCalendar.size()==0) {

        cacheTradeCalendar();
    }

    auto it = tradeCalendar.find(trade_dt);

    if (it != tradeCalendar.end()) {
        return it->second.next_trade_date;
    }
    return 19700101;
}

const uint32_t StockDataManager::getPreTradeDate(const uint32_t trade_dt) {

    std::scoped_lock lock(m_mutex);

    if (tradeCalendar.size() == 0) {
        cacheTradeCalendar();
    }
    auto it = tradeCalendar.find(trade_dt);
    if (it != tradeCalendar.end()) {
        return it->second.pre_trade_date;
    }
    return 19700101;
}

/*0 :[end_date,end_date],N:[T-N,T] */
const uint32_t StockDataManager::getPreTradeDate_N(const uint32_t end_date,const std::size_t lastN) {

    std::scoped_lock lock(m_mutex);

    if (tradeCalendar.size() == 0) {
        cacheTradeCalendar();
    }
    auto it = tradeCalendar.find(end_date);

    if (it != tradeCalendar.end()) {
        int count = 0;
        while (it != tradeCalendar.begin() and count < lastN) {
            --it;
            if (it->second.date == it->second.trade_date) {
                ++count;
            }
        }

        return it->second.date == it->second.trade_date ? it->second.trade_date: it->second.pre_trade_date;
    }
    return 19700101;
}

const std::vector<h5data::TradeCalendar> StockDataManager::getTradeDates(const uint32_t begin_date,const uint32_t end_date) {

    std::scoped_lock lock(m_mutex);

    if (tradeCalendar.size() == 0) {
        cacheTradeCalendar();
    }
    std::vector<h5data::TradeCalendar> result;

    auto lower = tradeCalendar.lower_bound(begin_date);
    auto upper = tradeCalendar.upper_bound(end_date);

    for (auto it = lower; it != upper; it++) {
        if (it->second.date == it->second.trade_date)
            result.push_back(it->second);
    }

    return result;
}

const std::set<uint32_t> StockDataManager::getTradeDateInts(const uint32_t begin_date, const uint32_t end_date) {
    
    std::scoped_lock lock(m_mutex);

    if (tradeCalendar.size() == 0) {
        cacheTradeCalendar();
    }
    
    std::set<uint32_t> result;

    if (begin_date > end_date) {
        return result;
    }

    auto lower = tradeCalendar.lower_bound(begin_date);

    for (auto it = lower; it->second.date <= end_date; it++) {
        if(it->second.date == it->second.trade_date)
            result.insert(it->second.trade_date);
    }

    return result;
}

void StockDataManager::cacheTradeCalendar() {

    agcommon::TimeCost tc(std::format("cacheTradeCalendar"));

    if (tradeCalendar.size() == 0) {
        auto fileName = agcommon::Configs::getConfigs().getTradeCalendarFile().string();

        if (not std::filesystem::exists(fileName)) {

            SPDLOG_ERROR("cacheTradeCalendar DOESNOT EXISTS:{}", fileName);
            return;
        }
        HighFive::File calendarFile(fileName, HighFive::File::ReadOnly);
        auto dataSetName = "/TradeCalendar";

        if (not calendarFile.exist(dataSetName)) {
            SPDLOG_ERROR("TradeCalendar dataset not exist");
            return;
        }
        auto dataset = calendarFile.getDataSet(dataSetName);

        std::vector<size_t> dims = dataset.getDimensions();

        size_t num_records = dims[0];

        if (num_records == 0) {
            SPDLOG_ERROR("TradeCalendar dataset has 0 data.");
            return;
        }

        std::vector<h5data::TradeCalendar> tradeDates(num_records);

        dataset.read<std::vector<h5data::TradeCalendar>>(tradeDates);

        for (auto& td : tradeDates) {

            SPDLOG_DEBUG("{},{},{}", td.trade_date, td.next_trade_date, td.next_trade_date);
            tradeCalendar.insert({ td.date,std::move(td) });
        }
        SPDLOG_INFO("TradeCalendar dataset read {} record,tradeCalendar size {}.", num_records, tradeCalendar.size());
    }
}

const std::shared_ptr<DailyBar> StockDataManager::getDailyBar(const Symbol_t& symbol, const uint32_t trade_dt) {

    std::scoped_lock<std::mutex> lock(m_mutex);

    auto it = date2DailBarMap.find(trade_dt);

    if (it == date2DailBarMap.end()) {

        const auto dailyBarMap = cacheDailyBar(trade_dt);

        if (auto it = dailyBarMap.find(symbol); it != dailyBarMap.end())
            return it->second;
        else
            return nullptr;
    }
    else {

        const auto& dailyBarMap = it->second;

        if (auto it = dailyBarMap.find(symbol); it != dailyBarMap.end())
            return it->second;
        else
            return nullptr;
    }

}

const Symbol2DailyBarHashMap StockDataManager::getDailyBarBlock(const uint32_t trade_dt) {

    std::scoped_lock<std::mutex> lock(m_mutex);

    auto it = date2DailBarMap.find(trade_dt);
    if (it == date2DailBarMap.end()) {
        auto dailyBarMap = cacheDailyBar(trade_dt);
        return dailyBarMap;
    }
    else {
        return it->second;
    }
}

const Symbol2DailyBarHashMap StockDataManager::getDailyBarBlock(const uint32_t trade_dt, const std::unordered_set<Symbol_t>& symbolSets) {

    auto dailyBar = getDailyBarBlock(trade_dt);

    Symbol2DailyBarHashMap result;
    result.reserve(symbolSets.size());

    for (auto& [s, bar] : dailyBar) {
        if (symbolSets.contains(s)) {
            result.emplace(s, bar);
        }
    }
    return result;

}

const Symbol2DailyBarHashMap StockDataManager::getDailyBarBlock(const uint32_t trade_dt, const std::string& indexName) {

    auto dailyBar = getDailyBarBlock(trade_dt);

    Symbol2DailyBarHashMap result;


    auto [index_tag, size] = getIndexName2Tag(indexName);
    result.reserve(size);

    if (index_tag < 0) {
        SPDLOG_ERROR("indexName:{} not supported", indexName);
        return result;
    }

    for (auto& [s, bar] : dailyBar) {
        if (bar->belongIndex == index_tag) {
            result.emplace(s, bar);
        }
    }
    return result;
}

const Symbol2DailyBarHashMap StockDataManager::getDailyBarBlock(const uint32_t trade_dt, const std::unordered_set<int>& securityTypes) {

    auto dailyBar = getDailyBarBlock(trade_dt);

    Symbol2DailyBarHashMap result;

    for (auto& [s, bar] : dailyBar) {
        if (securityTypes.contains(bar->securityType)) {
            result.emplace(s, bar);
        }
    }
    return result;
}

const Symbol2DailyBarHashMap StockDataManager::cacheDailyBar(const int trade_dt) {

    if (trade_dt > agcommon::getDateInt(agcommon::now())) {
        return Symbol2DailyBarHashMap{};
    }
    agcommon::TimeCost tc(std::format("cacheDailyBar {}", trade_dt));

    auto fileName = agcommon::Configs::getConfigs().getDailyBarFile().string();

    if (not std::filesystem::exists(fileName)) {
        SPDLOG_ERROR("cacheDailyBar DOESNOT EXISTS:{}", fileName);
        return Symbol2DailyBarHashMap{};
    }

    HighFive::File file(fileName, HighFive::File::ReadOnly);

    auto dataSetName = std::format("/D{}", trade_dt);   // format:/Dyyyymmdd

    if (not file.exist(dataSetName)) {  

        SPDLOG_ERROR("DailyBar dataset {} not exist", dataSetName);

        return Symbol2DailyBarHashMap{};
    }

    auto dataset = file.getDataSet(dataSetName);
    std::vector<size_t> dims = dataset.getDimensions();
    size_t num_records = dims[0];

    if (num_records == 0) {

        SPDLOG_INFO("DailyBar dataset {} have 0 records.", dataSetName);

        return Symbol2DailyBarHashMap{};
    }

    std::vector<h5data::DailyBarRaw>   dailyRawBars(num_records);

    dataset.read<std::vector<h5data::DailyBarRaw>>(dailyRawBars);

    auto& dailyBarMap = date2DailBarMap[trade_dt];

    for (auto& rawBar : dailyRawBars) {
        auto symbol = agcommon::toLocalSymbol(rawBar.symbol);
        if (symbol == "") {
            SPDLOG_ERROR("symbol:{} not supported", rawBar.symbol);
            continue;
        }
        SPDLOG_DEBUG("{},{},{:.3f},{:.3f},{:.3f},{}", rawBar.symbol, rawBar.trade_date, rawBar.open, rawBar.pre_close, rawBar.close, rawBar.turnrate);

        auto barPtr = std::make_shared<DailyBar>();

        barPtr->symbol = std::move(symbol);
        barPtr->tradeDate = rawBar.trade_date;
        barPtr->open            = rawBar.open;
        barPtr->close           = rawBar.close;
        barPtr->high            = rawBar.high;
        barPtr->low             = rawBar.low;
        barPtr->amount          = rawBar.amount;
        barPtr->volume          = rawBar.volume;
        barPtr->preclose        = rawBar.pre_close;
        barPtr->turnRate        = rawBar.turnrate;
        barPtr->marketCap       = rawBar.market_cap;
        barPtr->marketCapCirc   = rawBar.market_cap_circ;
        barPtr->highLimitPrice  = rawBar.upper_limit;
        barPtr->lowLimitPrice   = rawBar.lower_limit;
        barPtr->belongIndex     = rawBar.belong_index;
        barPtr->securityType    = rawBar.sec_type;
        barPtr->adjFactor       = rawBar.adj_factor;

        SPDLOG_DEBUG("{},{},{:.3f},{:.3f},{:.3f},{}", barPtr->symbol, barPtr->tradeDate, barPtr->open, barPtr->preclose, barPtr->close, barPtr->turnRate);
        
        dailyBarMap.insert({ barPtr->symbol,barPtr});
    }

    SPDLOG_INFO("DailyBar dataset {} read {} record.", dataSetName,num_records);

    return dailyBarMap;

}

const Time2Symbol2MinuteBarMap& StockDataManager::getMinuteBar(const uint32_t trade_dt) {

    std::scoped_lock<std::mutex> lock(m_mutex);

    auto it = date2Time2MinuteBars.find(trade_dt);

    if (it == date2Time2MinuteBars.end()) {
        return cacheMinuteBar(trade_dt);
    }
    else {
        if (it->second.empty()) {
            return cacheMinuteBar(trade_dt);  // cache again
        }
        else {
            return it->second;
        }
    }
}

const Time2Symbol2MinuteBarMap& StockDataManager::cacheMinuteBar(const int trade_dt) {

    agcommon::TimeCost tc(std::format("cacheMinuteBar {}", trade_dt));

    auto fileName = agcommon::Configs::getConfigs().getMinuteBarFile().string();

    if (not std::filesystem::exists(fileName)) {
        SPDLOG_ERROR("cache MinuteBar DOESNOT EXISTS:{}", fileName);
        auto& time2MinuteBars = date2Time2MinuteBars[trade_dt];
        return time2MinuteBars;
    }

    HighFive::File file(fileName, HighFive::File::ReadOnly);

    auto dataSetName = std::format("/M{}", trade_dt);   // format:/Myyyymmdd

    auto& time2MinuteBars = date2Time2MinuteBars[trade_dt];

    if (not file.exist(dataSetName)) {
        SPDLOG_ERROR("MinuteBar dataset {} not exist", dataSetName);
        return time2MinuteBars;
    }

    auto dataset = file.getDataSet(dataSetName);

    std::vector<size_t> dims = dataset.getDimensions();

    size_t num_records = dims[0];

    if (num_records == 0) {

        SPDLOG_INFO("DailyBar dataset {} have 0 records.", dataSetName);

        return time2MinuteBars;
    }

    std::vector<h5data::MinuteBarRaw> bars(num_records);

    dataset.read<std::vector<h5data::MinuteBarRaw>>(bars);

    auto lastQuoteTime = agcommon::now();
    bool isInserted = false;
    auto minuteBars = time2MinuteBars.end();

    for (auto& bar : bars) {

        auto symbol = agcommon::toLocalSymbol(bar.symbol);

        if (symbol == "") {
            SPDLOG_ERROR("symbol:{} not supported", bar.symbol);
            continue;
        }

        auto barPtr = std::make_shared<MinuteBar>();

        barPtr->symbol = std::move(symbol);
        barPtr->tradeTime = *agcommon::parseDateTimeInteger(static_cast<uint64_t>(bar.trade_date) * 1000000 + static_cast<uint64_t>(bar.eob));
        barPtr->preClose = bar.pre_close;
        barPtr->open   = bar.open;
        barPtr->close  = bar.close;
        barPtr->high   = bar.high;
        barPtr->low    = bar.low;
        barPtr->amount = bar.amount;
        barPtr->volume = bar.volume;

        SPDLOG_DEBUG("{},{},{:.3f},{:.3f},{:.3f}", barPtr->symbol,agcommon::getDateTimeStr(barPtr->tradeTime), barPtr->open, barPtr->preClose, barPtr->close);

        if (lastQuoteTime == barPtr->tradeTime) {

            minuteBars->second.emplace(barPtr->symbol, barPtr);
        }
        else {
            std::tie(minuteBars, isInserted) = time2MinuteBars.try_emplace(barPtr->tradeTime);

            if (isInserted) {
                minuteBars->second.reserve(8000);
            }

            minuteBars->second.emplace(barPtr->symbol, barPtr);

            lastQuoteTime = barPtr->tradeTime;
        }

    }
    SPDLOG_INFO("minuteBars dataset {} read {} record.", dataSetName, num_records);

    return time2MinuteBars;

}

/* use hdf5[threadsafe],so is threadsafe */
size_t StockDataManager::cacheFromH5Tick(const std::unordered_set<Symbol_t>& symbols
    , const QuoteTime_t& startTime
    , const QuoteTime_t& endTime
    , std::map<QuoteTime_t, UnorderMarketDepthRawPtrMap>& quoteTime2Symbol2md)
{
    auto marketCloseAuctionBeginTime = agcommon::AshareMarketTime::getClosingCallAuctionBeginTime(endTime);
    std::string startDateStr = agcommon::geISODateStr(startTime); //yyyymmdd
    std::string endDateStr   = agcommon::geISODateStr(endTime);

    auto tickH5FilePath = agcommon::Configs::getConfigs().getTickH5Dir();

    if (not std::filesystem::exists(tickH5FilePath)) {

        SPDLOG_ERROR("Tick H5 Dir DOESNOT EXISTS:{}", tickH5FilePath);
        return 0;
    }

    auto dir = std::filesystem::directory_iterator(tickH5FilePath);

    size_t totalLines = 0;
    std::vector<h5data::Tick> ticks(5000);
    auto files = std::filesystem::directory_iterator(dir);

    for (const auto& file : files)
    {
        auto fileNameWithPath = file.path().string();
        auto fileName = file.path().filename().string();
        if (fileName.size() < 11) {
            continue;
        }
        auto dt = fileName.substr(fileName.size() - 11).substr(0, 8);

        if (file.is_regular_file() and fileName.ends_with(".h5") and fileName.starts_with("Tick") and dt >= startDateStr and dt <= endDateStr)
        {

            HighFive::File pfile(fileNameWithPath, HighFive::File::ReadOnly);

            auto tmp = agcommon::parseDateTimeStr(dt + "T093000");
            if (not tmp) {
                continue;
            }
            auto& refTime = *tmp;
            auto mo = agcommon::AshareMarketTime::getOpeningCallAuctionBeginTime(refTime);
            auto mc = agcommon::AshareMarketTime::getMarketMorningCloseTime(refTime);
            auto ao = agcommon::AshareMarketTime::getMarketAfternoonOpenTime(refTime);
            auto ac = agcommon::AshareMarketTime::getMarketCloseTime(refTime);

            //SPDLOG_INFO("{},{},{},{}", agcommon::getDateTimeStr(mo), agcommon::getDateTimeStr(mc), agcommon::getDateTimeStr(ao), agcommon::getDateTimeStr(ac));
            auto dtint   = agcommon::get_int(dt);
            auto ssinfos = getSecurityBlockInfo(dtint);

            for (auto& symbol : symbols) {

                auto ssinfo_it = ssinfos.find(symbol);
                if (ssinfo_it == ssinfos.end()) {
                    continue;
                }
                auto& ssinfo = ssinfo_it->second;
                auto dataSetName = symbolH5Key(symbol);

                if (not pfile.exist(dataSetName)) {
                    SPDLOG_WARN("dataset not exist {},{}", dt, dataSetName);
                    continue;
                }

                auto dataset = pfile.getDataSet(dataSetName);
                std::vector<size_t> dims = dataset.getDimensions();

                dataset.read<std::vector<h5data::Tick>>(ticks);
                SPDLOG_DEBUG("file:{},dataSet:{},date:{},length:{},closePrice:{:.3f}", fileName, dataSetName, dt, dims[0], ssinfo->preClosePrice);

                for (const auto& tick : ticks) {
                    auto quoteTime = agcommon::AshareMarketTime::convert2ShanghaiTZ(tick.created_at);
                    if (quoteTime >= startTime and quoteTime <= endTime) {
                        if (quoteTime > mc and quoteTime < ao) {
                            continue;
                        }
                        auto md = MarketDepth::create(&tick, ssinfo->preClosePrice, ssinfo->unLAShare);

                        auto [qit, qInserted] = quoteTime2Symbol2md.try_emplace(md->quoteTime);

                        auto& symbol2md = qit->second;

                        if (qInserted) {
                            symbol2md.reserve(symbols.size() / 8);
                        }
                        auto [sit,mdInserted] = symbol2md.try_emplace(symbol, md);
                        if (mdInserted) {
                            ++totalLines;
                        }
                        else {
                            SPDLOG_WARN("double tick:, {},{}", symbol, agcommon::getDateTimeInt(md->quoteTime));
                        }
                     
                    }
                }

                ticks.clear();
            }
        }
    }

    SPDLOG_INFO("read Tick {} symbols:{}-{},lines:{}"
        , symbols.size()
        , agcommon::getDateTimeStr(startTime), agcommon::getDateTimeStr(endTime)
        , totalLines
        );

    return totalLines;
}

size_t StockDataManager::cacheFromH5Tick(const std::unordered_set<Symbol_t>& symbols
    , const QuoteTime_t& startTime
    , const QuoteTime_t& endTime
    , boost::unordered_map<Symbol_t, std::map<QuoteTime_t,MarketDepthKeepAlivePtr>>& symbol2quoteTime2md)
{        
        auto marketCloseAuctionBeginTime = agcommon::AshareMarketTime::getClosingCallAuctionBeginTime(endTime);
        std::string startDateStr = agcommon::geISODateStr(startTime); //yyyymmdd
        std::string endDateStr   = agcommon::geISODateStr(endTime);
    
        auto tickH5FilePath = agcommon::Configs::getConfigs().getTickH5Dir();
    
        if (not std::filesystem::exists(tickH5FilePath)) {
    
            SPDLOG_ERROR("Tick H5 Dir DOESNOT EXISTS:{}", tickH5FilePath);
            return 0;
        }
    
        auto dir = std::filesystem::directory_iterator(tickH5FilePath);
    
        size_t totalLines = 0;
        std::vector<h5data::Tick> ticks(5000);
        auto files = std::filesystem::directory_iterator(dir);
    
        for (const auto& file : files)
        {
            auto fileNameWithPath = file.path().string();
            auto fileName = file.path().filename().string();
            if (fileName.size() < 11) {
                continue;
            }
            auto dt = fileName.substr(fileName.size() - 11).substr(0, 8);
    
            if (file.is_regular_file() and fileName.ends_with(".h5") and fileName.starts_with("Tick") and dt >= startDateStr and dt <= endDateStr)
            {
    
                HighFive::File pfile(fileNameWithPath, HighFive::File::ReadOnly);
    
                auto tmp = agcommon::parseDateTimeStr(dt + "T093000");
                if (not tmp) {
                    continue;
                }
                auto& refTime = *tmp;
                auto mo = agcommon::AshareMarketTime::getOpeningCallAuctionBeginTime(refTime);
                auto mc = agcommon::AshareMarketTime::getMarketMorningCloseTime(refTime);
                auto ao = agcommon::AshareMarketTime::getMarketAfternoonOpenTime(refTime);
                auto ac = agcommon::AshareMarketTime::getMarketCloseTime(refTime);
    
                //SPDLOG_INFO("{},{},{},{}", agcommon::getDateTimeStr(mo), agcommon::getDateTimeStr(mc), agcommon::getDateTimeStr(ao), agcommon::getDateTimeStr(ac));
                auto dtint   = agcommon::get_int(dt);
                auto ssinfos = getSecurityBlockInfo(dtint);
    
                for (auto& symbol : symbols) {
    
                    auto ssinfo_it = ssinfos.find(symbol);
                    if (ssinfo_it == ssinfos.end()) {
                        continue;
                    }
                    auto& ssinfo = ssinfo_it->second;
                    auto dataSetName = symbolH5Key(symbol);
    
                    if (not pfile.exist(dataSetName)) {
                        SPDLOG_WARN("dataset not exist {},{}", dt, dataSetName);
                        continue;
                    }
    
                    auto dataset = pfile.getDataSet(dataSetName);
                    std::vector<size_t> dims = dataset.getDimensions();
    
                    dataset.read<std::vector<h5data::Tick>>(ticks);
                    SPDLOG_DEBUG("file:{},dataSet:{},date:{},length:{},closePrice:{:.3f}", fileName, dataSetName, dt, dims[0], ssinfo->preClosePrice);
    
                    for (const auto& tick : ticks) {
                        auto quoteTime = agcommon::AshareMarketTime::convert2ShanghaiTZ(tick.created_at);
                        if (quoteTime >= startTime and quoteTime <= endTime) {
                            if (quoteTime > mc and quoteTime < ao) {
                                continue;
                            }
                            auto md = MarketDepth::make_intrusive(&tick, ssinfo->preClosePrice, ssinfo->unLAShare);
    
                            auto [qit, qInserted] = symbol2quoteTime2md.try_emplace(md->symbol());
    
                            auto& qutoeTime2md = qit->second;
    
                            auto sit = qutoeTime2md.try_emplace(qutoeTime2md.end(),md->quoteTime, md);

                            ++totalLines;
                        }
                    }
    
                    ticks.clear();
                }
            }
        }
    
        SPDLOG_INFO("read Tick {} symbols:{}-{},lines:{}"
            , symbols.size()
            , agcommon::getDateTimeStr(startTime), agcommon::getDateTimeStr(endTime)
            , totalLines
            );
    
        return totalLines;
 }


 size_t StockDataManager::cacheFromH5Tick(const std::unordered_set<Symbol_t>& symbols
     , const QuoteTime_t& startTime
     , const QuoteTime_t& endTime
     , std::map<QuoteTime_t, UnorderMarketDepthPtrMap>& quoteTime2Symbol2mdPtr)
 {
     auto marketCloseAuctionBeginTime = agcommon::AshareMarketTime::getClosingCallAuctionBeginTime(endTime);
     std::string startDateStr = agcommon::geISODateStr(startTime); //yyyymmdd
     std::string endDateStr = agcommon::geISODateStr(endTime);

     auto tickH5FilePath = agcommon::Configs::getConfigs().getTickH5Dir();

     if (not std::filesystem::exists(tickH5FilePath)) {

         SPDLOG_ERROR("Tick H5 Dir DOESNOT EXISTS:{}", tickH5FilePath);
         return 0;
     }

     auto dir = std::filesystem::directory_iterator(tickH5FilePath);

     size_t totalLines = 0;
     std::vector<h5data::Tick> ticks(5000);
     auto files = std::filesystem::directory_iterator(dir);

     for (const auto& file : files)
     {
         auto fileNameWithPath = file.path().string();
         auto fileName = file.path().filename().string();
         if (fileName.size() < 11) {
             continue;
         }
         auto dt = fileName.substr(fileName.size() - 11).substr(0, 8);

         if (file.is_regular_file() and fileName.ends_with(".h5") and fileName.starts_with("Tick") and dt >= startDateStr and dt <= endDateStr)
         {

             HighFive::File pfile(fileNameWithPath, HighFive::File::ReadOnly);

             auto tmp = agcommon::parseDateTimeStr(dt + "T093000");
             if (not tmp) {
                 continue;
             }
             auto& refTime = *tmp;
             auto mo = agcommon::AshareMarketTime::getOpeningCallAuctionBeginTime(refTime);
             auto mc = agcommon::AshareMarketTime::getMarketMorningCloseTime(refTime);
             auto ao = agcommon::AshareMarketTime::getMarketAfternoonOpenTime(refTime);
             auto ac = agcommon::AshareMarketTime::getMarketCloseTime(refTime);

             //SPDLOG_INFO("{},{},{},{}", agcommon::getDateTimeStr(mo), agcommon::getDateTimeStr(mc), agcommon::getDateTimeStr(ao), agcommon::getDateTimeStr(ac));
             auto dtint = agcommon::get_int(dt);
             auto ssinfos = getSecurityBlockInfo(dtint);

             for (auto& symbol : symbols) {

                 auto ssinfo_it = ssinfos.find(symbol);
                 if (ssinfo_it == ssinfos.end()) {
                     continue;
                 }
                 auto& ssinfo = ssinfo_it->second;
                 auto dataSetName = symbolH5Key(symbol);

                 if (not pfile.exist(dataSetName)) {
                     SPDLOG_WARN("dataset not exist {},{}", dt, dataSetName);
                     continue;
                 }

                 auto dataset = pfile.getDataSet(dataSetName);
                 std::vector<size_t> dims = dataset.getDimensions();

                 dataset.read<std::vector<h5data::Tick>>(ticks);
                 SPDLOG_DEBUG("file:{},dataSet:{},date:{},length:{},closePrice:{:.3f}", fileName, dataSetName, dt, dims[0], ssinfo->preClosePrice);

                 for (const auto& tick : ticks) {
                     auto quoteTime = agcommon::AshareMarketTime::convert2ShanghaiTZ(tick.created_at);
                     if (quoteTime >= startTime and quoteTime <= endTime) {
                         if (quoteTime > mc and quoteTime < ao) {
                             continue;
                         }
                         auto md = MarketDepth::make_intrusive(&tick, ssinfo->preClosePrice, ssinfo->unLAShare);

                         auto [qit, qInserted] = quoteTime2Symbol2mdPtr.try_emplace(quoteTime);

                         auto& symbol2Md = qit->second;

                         if (qInserted) {
                             symbol2Md.reserve(symbols.size() / 10);
                         }
                         symbol2Md.try_emplace(md->symbol(),md);

                         ++totalLines;
                     }
                 }

                 ticks.clear();
             }
         }
     }

     SPDLOG_INFO("read Tick {} symbols:{}-{},lines:{}"
         , symbols.size()
         , agcommon::getDateTimeStr(startTime), agcommon::getDateTimeStr(endTime)
         , totalLines
     );

     return totalLines;
}

std::unordered_map<Symbol_t, std::pair<double, double>> StockDataManager::getStockReturnAndRange(uint32_t beg_Date
    , uint32_t end_Date
    , const std::unordered_set<Symbol_t>& symbols) {

    std::unordered_map<Symbol_t, std::pair<double, double>> result;

    std::unordered_map<Symbol_t, std::pair<double, double>> symbol2MinMax;  // min,max
    std::unordered_map<Symbol_t, std::pair<double, double>> symbol24Return; // window begin

    auto dates = getTradeDates(beg_Date, end_Date);

    //std::sort(dates.begin(), dates.end());

    for (const auto& date : dates) {
        SPDLOG_DEBUG("pre:{} ,date:{}", date.pre_trade_date, date.trade_date);
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

//
//size_t StockDataManager::cacheFromH5Tick(const std::unordered_set<Symbol_t>& symbols
//    , const QuoteTime_t& startTime
//    , const QuoteTime_t& endTime
//    , std::map<QuoteTime_t, UnorderMarketDepthMap>& quoteTime2Symbol2md)
//{
//    auto marketCloseAuctionBeginTime = agcommon::AshareMarketTime::getClosingCallAuctionBeginTime(endTime);
//    std::string startDateStr = agcommon::geISODateStr(startTime); //yyyymmdd
//    std::string endDateStr   = agcommon::geISODateStr(endTime);
//
//    auto tickH5FilePath = agcommon::Configs::getConfigs().getTickH5Dir();
//
//    if (not std::filesystem::exists(tickH5FilePath)) {
//
//        SPDLOG_ERROR("Tick H5 Dir DOESNOT EXISTS:{}", tickH5FilePath);
//        return 0;
//    }
//
//    auto dir = std::filesystem::directory_iterator(tickH5FilePath);
//
//    size_t totalLines = 0;
//    std::vector<h5data::Tick> ticks(5000);
//
//    for (const auto& file : std::filesystem::directory_iterator(dir))
//    {
//        auto filePath = file.path().string();
//        auto fileName = file.path().filename().string();
//        if (fileName.size() < 11) {
//            continue;
//        }
//        auto dt = fileName.substr(fileName.size() - 11).substr(0, 8);
//
//        if (file.is_regular_file() and fileName.ends_with(".h5") and fileName.starts_with("Tick") and dt >= startDateStr and dt <= endDateStr)
//        {
//            std::string h5file = filePath;
//            try {
//                HighFive::File pfile(h5file, HighFive::File::ReadOnly);
//
//                //SPDLOG_INFO("OPEN {}", filePath);
//                auto tmp = agcommon::parseDateTimeStr(dt + "T093000");
//                if (not tmp) {
//                    continue;
//                }
//                auto& refTime = *tmp;
//                auto mo = agcommon::AshareMarketTime::getOpeningCallAuctionBeginTime(refTime);
//                auto mc = agcommon::AshareMarketTime::getMarketMorningCloseTime(refTime);
//                auto ao = agcommon::AshareMarketTime::getMarketAfternoonOpenTime(refTime);
//                auto ac = agcommon::AshareMarketTime::getMarketCloseTime(refTime);
//                //SPDLOG_INFO("{},{},{},{}", agcommon::getDateTimeStr(mo), agcommon::getDateTimeStr(mc), agcommon::getDateTimeStr(ao), agcommon::getDateTimeStr(ac));
//                auto dtint = agcommon::get_int(dt);
//                auto ssinfos = getSecurityBlockInfo(dtint);
//
//                for (auto& symbol : symbols) {
//                    auto ssinfo_it = ssinfos.find(symbol);
//                    if (ssinfo_it == ssinfos.end()) {
//                        continue;
//                    }
//                    auto& ssinfo = ssinfo_it->second;
//                    auto dataSetName = symbolH5Key(symbol);
//
//                    if (not pfile.exist(dataSetName)) {
//                        SPDLOG_WARN("dataset not exist {},{}", dt, dataSetName);
//                        continue;
//                    }
//
//                    auto dataset = pfile.getDataSet(dataSetName);
//                    std::vector<size_t> dims = dataset.getDimensions();
//
//                    dataset.read<std::vector<h5data::Tick>>(ticks);
//                    SPDLOG_DEBUG("file:{},dataSet:{},date:{},length:{},closePrice:{:.3f}", fileName, dataSetName, dt, dims[0], ssinfo->preClosePrice);
//
//                    for (const auto& tick : ticks) {
//                        auto quoteTime = agcommon::AshareMarketTime::convert2ShanghaiTZ(tick.created_at);
//                        if (quoteTime >= startTime and quoteTime <= endTime) {
//                            if (quoteTime > mc and quoteTime < ao) {
//                                continue;
//                            }
//                            const auto tickPtr = &tick;
//
//                            auto [qit, qInserted] = quoteTime2Symbol2md.try_emplace(quoteTime);
//                            auto& symbol2Md = qit->second;
//
//                            if (qInserted) {
//                                symbol2Md.reserve(symbols.size() / 10);
//                            }
//                            symbol2Md.emplace(std::piecewise_construct,
//                                std::forward_as_tuple(symbol),
//                                std::forward_as_tuple(tickPtr, ssinfo->preClosePrice, ssinfo->unLAShare));    
//                        }
//                    }
//                    totalLines += dims[0];
//                    ticks.clear();
//                }
//            }
//            catch (std::exception e) {
//                SPDLOG_ERROR("{}", e.what());
//                return 0;
//            }
//        }
//    }
//    return totalLines;
//}