#include "QuoteFeedReplay.h"
#include "TCPSession.h"
#include "QuoteFeedService.h"

QuoteFeedReplay::QuoteFeedReplay(const QuoteFeedRequest& req) 
    : m_request(req)
    , QuoteFeed(req.dispatchContextPtr, req.startTime)
    , m_keepRuning(false) 
    , currentQuoteTime(req.startTime){
};


void QuoteFeedReplay::stop() {

    if (not m_keepRuning) {
        return;
    }

    m_keepRuning = false;

    asio::post(m_strand, [self = shared_from_this()]() {
        SPDLOG_INFO("[aId:{}]QuoteFeedReplay on finish,size:{}", self->m_request.algoOrderId, self->onQuoteFeedFinisheds.size());

        for (auto& [onRepalyFinished, contextPtr] : self->onQuoteFeedFinisheds) {

            asio::dispatch(*contextPtr, onRepalyFinished);
        }

        self->m_orderBookKey2CallBack.clear();
        self->m_symbol2SubscribeCallBack.clear();

        self->m_quoteTime2Symbol2md.clear();
        std::map<QuoteTime_t, UnorderMarketDepthRawPtrMap> tmp;
        tmp.swap(self->m_quoteTime2Symbol2md);
        self->onQuoteFeedFinisheds.clear();
        self->onQuoteFeedStarts.clear();

        QuoteFeedService::getInstance().removeQuoteFeed(self->m_request.algoOrderId);

        }
    );

}

SubscribeKey_t QuoteFeedReplay::subscribe(std::shared_ptr<SubMarketDepth_t> subPtr) {

    assert(subPtr->dispatchContextPtr != nullptr);
    assert(subPtr->subscribeKey > 0);

    asio::post(m_strand, [self = shared_from_this(), subPtr]() {
        self->_subscribe(subPtr);
        });

    return subPtr->subscribeKey;
}

uint64_t QuoteFeedReplay::_subscribe(std::shared_ptr<SubMarketDepth_t> subPtr) {

    auto [it, isinserted] = m_subscribeKey2SymbolCallBack.insert({ subPtr->subscribeKey,subPtr });
    if (isinserted) {
        for (auto& symbol : subPtr->symbols) {
            m_symbol2SubscribeCallBack[symbol].insert({ subPtr->subscribeKey,subPtr });
        }
        SPDLOG_DEBUG("[aId:{}]subscribe symbol count:{}", subPtr->subscribeKey, subPtr->symbols.size());
        run();
        return subPtr->subscribeKey;
    }
    return 0;
}

void QuoteFeedReplay::unSubscribe(const uint64_t subcribeKey) {

    asio::post(m_strand, [self = shared_from_this(), subcribeKey]() {
        self->_unSubscribe(subcribeKey);
        });
}

void QuoteFeedReplay::_unSubscribe(const uint64_t subcribeKey) {

    SPDLOG_INFO("unSubscribe[subcribeKey:{}]", subcribeKey);
    if (auto subPtrIt = m_subscribeKey2SymbolCallBack.find(subcribeKey); subPtrIt != m_subscribeKey2SymbolCallBack.end()) {
        auto& subPtr = subPtrIt->second;
        for (auto& symbol : subPtr->symbols) {
            if (auto sym_it = m_symbol2SubscribeCallBack.find(symbol); sym_it != m_symbol2SubscribeCallBack.end()) {
                sym_it->second.erase(subcribeKey);
            }
        }
        m_subscribeKey2SymbolCallBack.erase(subcribeKey);
    };
}

void QuoteFeedReplay::run() {
    if (not m_keepRuning) {
        SPDLOG_INFO("[aId:{}]QuoteFeedReplay launching....", m_request.algoOrderId);

        asio::co_spawn(m_strand, co_run(), [self = shared_from_this()](std::exception_ptr ex) {
            self->stop();
            if (!ex) {
                SPDLOG_INFO("[aId:{}]QuoteFeedReplay done", self->m_request.algoOrderId);
            }
            else {
                SPDLOG_ERROR("[aId:{}]QuoteFeedReplay exception", self->m_request.algoOrderId);
                std::rethrow_exception(ex);
            }

            });
    }
}

asio::awaitable<void> QuoteFeedReplay::co_run() {
    if (m_keepRuning)
        co_return;

    m_keepRuning = true;
    int i = 0;
    if (m_quoteTime2Symbol2md.size() > 0) {
        SPDLOG_INFO("aid:{} Replay start,{},{}", m_request.algoOrderId
            , agcommon::getDateTimeInt(m_quoteTime2Symbol2md.begin()->first)
            , agcommon::getDateTimeInt(m_quoteTime2Symbol2md.rbegin()->first));
    }
    else
        SPDLOG_INFO("aid:{} Replay start with 0 data", m_request.algoOrderId);

    for (auto& [onQuoteFeedStart,contextPtr] : onQuoteFeedStarts) {
        if (contextPtr) {
            asio::dispatch(*contextPtr,onQuoteFeedStart);
        }
    }

    for (const auto& [qt, symbol2md] : m_quoteTime2Symbol2md) {

        currentQuoteTime = qt;

        #if ENABLE_DELAY_STATS
            agcommon::TimeCost delay(fmt::format("[DELAY][{}]size:{}",agcommon::getDateTimeStr(qt), symbol2md.size()),"",false);
        #endif

        for (const auto& [symbol, md] : symbol2md) {

            if (m_keepRuning) {
                SPDLOG_DEBUG("MarketDepth:{}", md->to_string());
                md->retainAlive();
                if (m_symbol2md.find(md->symbol) == m_symbol2md.end()) {
                    m_symbol2md[md->symbol] = md;
                }
                else {
                    auto& lastMd = m_symbol2md[md->symbol];
                    md->calDelta(lastMd);
                    lastMd->release();
                    m_symbol2md[md->symbol] = md;
                }

                for (const auto& [orderBookKey, onPair] : m_orderBookKey2CallBack) {
                    auto& [onMarketDepth, onDelayTest] = onPair;
                    if (onMarketDepth) {
                        onMarketDepth(md);
                    }
                }

                if (const auto subCallBackMap_it = m_symbol2SubscribeCallBack.find(md->symbol); subCallBackMap_it != m_symbol2SubscribeCallBack.end()) {

                    for (auto& [subKey, subMarketDepth] : subCallBackMap_it->second) {
                        SPDLOG_DEBUG("[{}]subMarketDepth:{}", subKey, md->to_string());

                        if (subMarketDepth->publish2Client) {

                            auto mdMessage = md->encode2AlgoMessage(subKey);

                            TCPSessionManager::getInstance().sendNotify2C(subMarketDepth->acctKey, AlgoMsg::CMD_NOTIFY_MarketDepth, mdMessage,false);
                        }
                        if (subMarketDepth->onMarketDepth) {

                            subMarketDepth->onMarketDepth(md);
                        }
                        if (subMarketDepth->co_onMarketDepth) {

                            auto status = co_await subMarketDepth->co_onMarketDepth(md);

                            if (agcommon::AlgoStatus::isFinalStatus(status)) {
                                break;
                            }
                        }
                    }
                }
            }
            md->release();
        }
    
        #if ENABLE_DELAY_STATS
                testDelay<QuoteFeedReplay>(delay);
        #endif // ENABLE_DELAY_TEST

    }

    for (const auto& [symbol, md] : m_symbol2md) {
        md->release();
    }

    m_symbol2md.clear();
}

/*call getVWAP should in the same thread of QuoteFeedReplay's running m_strand */
double QuoteFeedReplay::getVWAP(const Symbol_t& symbol, const QuoteTime_t& begTime, const QuoteTime_t& endTime) {

    auto it_low = m_quoteTime2Symbol2md.lower_bound(begTime);
    auto it_up  = m_quoteTime2Symbol2md.upper_bound(endTime);  

    double beg_amt = 0.0;
    uint64_t beg_vol = 0;

    double end_amt = 0.0;
    uint64_t end_vol = 0;

    for (auto it = it_low; it != it_up; ++it) {
        if (auto symbol_it = it->second.find(symbol); symbol_it != it->second.end()) {
            beg_amt = symbol_it->second->amount;
            beg_vol = symbol_it->second->volume;
            break;
        }
    }
    for (auto it = it_up; it != it_low; --it) {
        if (auto symbol_it = it->second.find(symbol); symbol_it != it->second.end()) {
            end_amt = symbol_it->second->amount;
            end_vol = symbol_it->second->volume;
            break;
        }
    }

    double vwap = 0.0;

    SPDLOG_DEBUG("VWAP:{},{},{}:{:.2f},{},{:.2f},{}", symbol, agcommon::getDateTimeInt(begTime), agcommon::getDateTimeInt(endTime)
        ,beg_amt, beg_vol, end_amt, end_vol);

    if (end_amt > beg_amt and end_vol - beg_vol) {
        vwap = (end_amt -  beg_amt)*1.0 / (end_vol - beg_vol);
    }
   
    return vwap;
}