#include "QuoteFeedInternet.h"
#include "TCPSession.h"
#include "MarketDepth.h"

static void requestErrorCallback(const requests::Exception& e)
{
    SPDLOG_ERROR(e.what());
}

QuoteFeedInternet::QuoteFeedInternet(const QuoteFeedRequest& req) 
    : m_request(req)
    , QuoteFeed(req.dispatchContextPtr)
    , m_symbolSubCounts{}
    , m_keepRuning(false){};


void QuoteFeedInternet::stop() {

    if (m_keepRuning) {
        SPDLOG_INFO("QuoteFeedInternet CALL to stop ");
        m_keepRuning = false;
    }

    auto self = shared_from_this();

    asio::post(m_strand, [self]() {

        for (auto& [onRepalyFinished,contextPtr] : self->onQuoteFeedFinisheds) {
            if(contextPtr)
                asio::dispatch(*contextPtr,onRepalyFinished);
        }

        self->m_orderBookKey2CallBack.clear();
        self->m_symbol2SubscribeCallBack.clear();

        self->onQuoteFeedFinisheds.clear();
        self->onQuoteFeedStarts.clear();

        for (auto& [symbol, md] : self->m_symbol2md) {
            md->release();
        }
        self->m_symbol2md.clear();

        self->m_batchSymbolString2Fetch.clear();

        }
    );

}

SubscribeKey_t QuoteFeedInternet::subscribe(std::shared_ptr<SubMarketDepth_t> subPtr) {

    assert(subPtr->dispatchContextPtr != nullptr);
    assert(subPtr->subscribeKey > 0);

    auto self = shared_from_this();

    auto task = [self, subPtr]() {
        auto [it, isinserted] = self->m_subscribeKey2SymbolCallBack.insert({ subPtr->subscribeKey,subPtr });
        if (isinserted) {
            for (auto& symbol : subPtr->symbols) {
                auto& count = self->m_symbolSubCounts[symbol];
                count++;
                SPDLOG_INFO("[aId:{}]subscribe {},counts:{}", subPtr->subscribeKey, symbol, count);
                self->m_symbol2SubscribeCallBack[symbol].insert({ subPtr->subscribeKey,subPtr });
            }
        }
        };
    asio::post(m_strand, task);

    return subPtr->subscribeKey;
}

void QuoteFeedInternet::unSubscribe(const uint64_t subcribeKey) {

    assert(subcribeKey > 0);

    auto task = [self = shared_from_this(), subcribeKey]() {
        if (auto subPtrIt = self->m_subscribeKey2SymbolCallBack.find(subcribeKey); subPtrIt != self->m_subscribeKey2SymbolCallBack.end()) {
            auto& subPtr = subPtrIt->second;
            for (auto& symbol : subPtr->symbols) {
                if (self->m_symbolSubCounts.find(symbol) != self->m_symbolSubCounts.end()) {
                    if (self->m_symbolSubCounts[symbol] > 0)
                        self->m_symbolSubCounts[symbol]--;
                    if (self->m_symbolSubCounts[symbol] == 0) {
                        SPDLOG_INFO("symbol {} unSubscribe to 0", symbol);
                        self->m_symbolSubCounts.erase(symbol);
                    }
                }
                if (auto sym_it = self->m_symbol2SubscribeCallBack.find(symbol); sym_it != self->m_symbol2SubscribeCallBack.end()) {
                    sym_it->second.erase(subcribeKey);
                }
            }
            self->m_subscribeKey2SymbolCallBack.erase(subPtrIt);
        }
        };
    asio::post(m_strand, task);
}

void QuoteFeedInternet::run() {
    if (not m_keepRuning) {
        SPDLOG_INFO("QuoteFeedInternet launch....");

        asio::co_spawn(*m_contextPtr, co_run(), [self = shared_from_this()](std::exception_ptr ex) {
            if (!ex) {
                SPDLOG_INFO("[QuoteFeedInternet]co_spawn finished");
                self->stop();
            }
            else {
                SPDLOG_ERROR("[QuoteFeedInternet]co_spawn exception");
                try {
                    std::rethrow_exception(ex);
                }
                catch (std::exception& e) {
                    SPDLOG_ERROR("QuoteFeedInternet::run exception:{}", e.what());
                    self->m_keepRuning = false;
                    self->run();
                }
            }
            });
    }
}

asio::awaitable<void> QuoteFeedInternet::co_run() {

    if (m_keepRuning) co_return;

    SPDLOG_INFO(" QuoteFeedInternet start ");

    m_keepRuning = true;

    requests::AsyncRequest    asyncRequest(1);

    asio::steady_timer        timer(*m_contextPtr);

    std::unordered_map<std::string, std::string> params;
    requests::Url url(m_url);

    for (auto& [onQuoteFeedStart, contextPtr] : onQuoteFeedStarts) {
        if (contextPtr) {
            asio::dispatch(*contextPtr, onQuoteFeedStart);
        }
    }

    while (m_keepRuning) {

        double waitSecs = 0;
        auto now = agcommon::now();
        auto nowtMarketTime = agcommon::AshareMarketTime::getAvailableStartMarketTime(now);
        if (nowtMarketTime > now) {
            waitSecs = agcommon::getSecondsDiff(now, nowtMarketTime);
            timer.expires_after(std::chrono::milliseconds(int(waitSecs * 1000)));
            co_await timer.async_wait(asio::use_awaitable);
        }

        auto i = 1;
        
        asio::co_spawn(m_strand, [self = shared_from_this(), &params, &url, &i, &asyncRequest]()->asio::awaitable<void> {

            self->genSymbolBatchs2Fetch();

            for (auto& [batchSize, symbolsStr] : self->m_batchSymbolString2Fetch) {

                if (symbolsStr.size() > 1) {
                    params["q"] = symbolsStr;
                    asyncRequest.get(url, params
                        , std::bind(&QuoteFeedInternet::requestResponseHandler, self, batchSize, std::placeholders::_1)
                        , requestErrorCallback);
                }
                i++;
            }
            co_return;
            },asio::detached);
        
        waitSecs = 1.0;
        if (agcommon::getSecondsDiff(agcommon::AshareMarketTime::getMarketCloseTime(now), now) > 60*2) {
            break;
        }
        timer.expires_after(std::chrono::milliseconds(int(waitSecs * 1000)));
        co_await timer.async_wait(asio::use_awaitable);
    }

    co_return;
}

void QuoteFeedInternet::genSymbolBatchs2Fetch() {

    m_batchSymbolString2Fetch.clear();
    std::string symbolsSubStr = "";
    int size = 0;
    auto now = agcommon::now();
    for (auto& [symbol, count] : m_symbolSubCounts) {
        if (count > 0) {
            if (auto it = m_symbol2md.find(symbol); it != m_symbol2md.end()) {
                if (agcommon::getSecondsDiff(it->second->quoteTime, now) < 2.0) {
                    continue;
                }
            }
            if (size < 500) {
                symbolsSubStr += "," + symbol.substr(7) + symbol.substr(0, 6);
                size++;
            }
            else {
                m_batchSymbolString2Fetch.push_back({ size,symbolsSubStr });
                symbolsSubStr = symbol.substr(7) + symbol.substr(0, 6);
                size = 1;
            }
        }
    }
    m_batchSymbolString2Fetch.push_back({ size,symbolsSubStr });
    for (auto& [batchSize, symbolsStr] : m_batchSymbolString2Fetch) {
        if (symbolsStr.size() > 0) {
            std::transform(symbolsStr.begin(), symbolsStr.end(), symbolsStr.begin(), [](char v) { return std::tolower(v); });
            //symbolsStr.erase(symbolsStr.size() - 1, 1);
            SPDLOG_DEBUG("m_symbolSubStr:{}", symbolsStr);
        }
    }

}

void QuoteFeedInternet::onMarketDepth(std::shared_ptr<std::vector<MarketDepth*>> newMds) {

    QuoteTime_t qutoeTime{};
    std::set<AcctKey_t> acct_set{};

    auto newMdCount = 0;

    #if ENABLE_DELAY_STATS
        agcommon::TimeCost delay("[DELAY]", fmt::format("[mds:{}]", newMds->size()) ,false);
    #endif 

    for (auto& md : *newMds) {
        if (md == nullptr) {
            continue;
        }

        auto lastIt = m_symbol2md.find(md->symbol);

        if (lastIt == m_symbol2md.end()) {

            m_symbol2md.emplace(md->symbol,md);
            //SPDLOG_INFO("FirstMD:{}", md->to_string());
            qutoeTime = md->quoteTime;
        }
        else {
            auto& lastMd = lastIt->second;

            if (md->quoteTime > lastMd->quoteTime) {

                md->calDelta(lastMd);

                lastMd->release();

                lastIt->second = md;

                qutoeTime = md->quoteTime;
            }
            else {
                md->release();
                continue;
            }
        }

        newMdCount++;

        if (auto subCallBackMap_it = m_symbol2SubscribeCallBack.find(md->symbol); subCallBackMap_it != m_symbol2SubscribeCallBack.end()) {

            for (auto& [orderBookKey, onPair] : m_orderBookKey2CallBack) {

                auto& [onMarketDepth, onDelayTest] = onPair;

                if (onMarketDepth) {
                    onMarketDepth(md);
                }
            }

            for (auto& [subKey, subMarketDepth] : subCallBackMap_it->second) {
                if (subMarketDepth->dispatchContextPtr) {
                    acct_set.insert(subMarketDepth->acctKey);
                    md->retainAlive();
                    asio::post(*subMarketDepth->dispatchContextPtr, [subMarketDepth, md]() {subMarketDepth->onMarketDepth(md); md->release(); });
                }
            }
        }

        for (auto& acctKey : acct_set) {
            auto mdMessage = md->encode2AlgoMessage();
            TCPSessionManager::getInstance().sendNotify2C(acctKey, AlgoMsg::CMD_NOTIFY_MarketDepth, mdMessage,false);
        }

        acct_set.clear();

    }

    if (newMdCount > 0) {
        ++m_fetchCount;
        //SPDLOG_INFO("{}th,NEW MD SIZE:{}", m_fetchCount, newMdCount);
    }

    #if ENABLE_DELAY_STATS
        testDelay<QuoteFeedInternet>(delay);
    #endif


}

void QuoteFeedInternet::requestResponseHandler(int batchSize, requests::Response& resp) {

    auto& data  = resp.content();
    auto mdstrs = m_respDataVecPool.acquire(batchSize);

    agcommon::textSplit(data, ';', *mdstrs);

    auto newMds = m_marketDepthVecPool.acquire(mdstrs->size());

    for (auto& mdstr : *mdstrs) {
        if (mdstr.size() < 50) {
            continue;
        }
        auto md = QuoteFeedInternet::parseMarketDepth_tx(mdstr);
        SPDLOG_DEBUG(md->to_string());
        if (md->symbol.empty()) {
            md->release();
            continue;
        }
        newMds->push_back(md);
    }
    asio::dispatch(m_strand, [self = shared_from_this(), newMds]() {
        self->onMarketDepth(newMds);
        });
}


MarketDepth* QuoteFeedInternet::parseMarketDepth_tx(const std::string& text) {

    std::vector<std::string> data;
    agcommon::textSplit(text, '~', data);

    if (data.size() < 58) {
        SPDLOG_WARN("[parseMarketDepth]data size is less than 58,{}", data.size());
        return MarketDepth::create("");
    }
    auto quoteTime = agcommon::now();
    auto tmp = agcommon::parseDateTimeStr(data[30].substr(0, 8) + " " + data[30].substr(8));
    if (tmp)
        quoteTime = *tmp;

    auto exchange = data[0].find("sh") != std::string::npos ? "SH" : "SZ";
    auto symbol = data[2] + "." + exchange;

    MarketDepth* new_md = MarketDepth::create(symbol, quoteTime);

    //new_md->name = data[1];
    int volSize = new_md->symbol.starts_with("68") ? 1 : 100;

    new_md->price    = agcommon::get_double(data[3]);
    new_md->preClose = agcommon::get_double(data[4]);
    new_md->open     = agcommon::get_double(data[5]);
    new_md->high     = agcommon::get_double(data[33]);
    new_md->low      = agcommon::get_double(data[34]);

    new_md->change = agcommon::get_double(data[31]);
    new_md->changeP = agcommon::get_double(data[32]) / 100.0;

    std::string price = "", amt = "", vol = "";

    std::vector<std::string> p_v_a;

    agcommon::textSplit(data[35], '/', p_v_a);

    if (p_v_a.size() == 3) {
        price = p_v_a[0];
        vol = p_v_a[1];
        amt = p_v_a[2];
    }
    if (vol > "0")
        new_md->volume = uint64_t(agcommon::get_int(vol)     * volSize);       //股
    else
        new_md->volume = uint64_t(agcommon::get_int(data[36]) * volSize);  //股

    if (amt > "0")
        new_md->amount  = agcommon::get_double(amt);
    else
        new_md->amount  = agcommon::get_double(data[57]) * 10000.0;    //元

    new_md->avgPrice    = agcommon::get_float(data[51]);

    new_md->bidPrice1   = agcommon::get_float(data[9]);
    new_md->askPrice1   = agcommon::get_double(data[19]);
    new_md->bidVol1     = agcommon::get_int(data[10]) * 100;
    new_md->askVol1     = agcommon::get_int(data[20]) * 100;

    new_md->bidPrice2   = agcommon::get_float(data[11]);
    new_md->bidVol2     = agcommon::get_int(data[12]) * 100;
    new_md->bidPrice3   = agcommon::get_float(data[13]);
    new_md->bidVol3     = agcommon::get_int(data[14]) * 100;
    new_md->bidPrice4   = agcommon::get_float(data[15]);
    new_md->bidVol4     = agcommon::get_int(data[16]) * 100;
    new_md->bidPrice5   = agcommon::get_float(data[17]);
    new_md->bidVol5     = agcommon::get_int(data[18]) * 100;

    new_md->askPrice2   = agcommon::get_float(data[21]);
    new_md->askVol2     = agcommon::get_int(data[22]) * 100;
    new_md->askPrice3   = agcommon::get_float(data[23]);
    new_md->askVol3     = agcommon::get_int(data[24]) * 100;
    new_md->askPrice4   = agcommon::get_float(data[25]);
    new_md->askVol4     = agcommon::get_int(data[26]) * 100;
    new_md->askPrice5   = agcommon::get_float(data[27]);
    new_md->askVol5     = agcommon::get_int(data[28]) * 100;
    new_md->turnRate    = agcommon::get_float(data[38]);

    return new_md;
}
