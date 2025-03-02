#include "QuoteFeedService.h"
#include "QuoteFeedLive.h"
#include "QuoteFeedInternet.h"
#include "QuoteFeedReplay.h"
#include "StockDataManager.h"
#include "ContextService.h"

std::shared_ptr<QuoteFeed> QuoteFeedService::queryDataFeed(const AlgoOrderId_t& key) {

    std::scoped_lock lock(m_mutex);

    auto it = replayQuoteFeedPtrs.find(key);

    if (it == replayQuoteFeedPtrs.end()) {
        return nullptr;
    }
    return it->second;
}

std::shared_ptr<QuoteFeed> QuoteFeedService::createQuoteFeed(const QuoteFeedRequest& req, AlgoErrorMessage_t& algoErrMessage) {

    static QuoteFeedRequest liveRequest{ agcommon::QuoteMode::LIVE
        , 1
        , {}
        , agcommon::now()
        , agcommon::now()
        , ContextService::getInstance().createContext("QuoteLive")};

    /*real quote*/
    static std::shared_ptr<QuoteFeed> liveQuoteFeedPtr{std::make_shared<QuoteFeedLive>(liveRequest)};

    if (req.quoteMode == agcommon::QuoteMode::LIVE or req.quoteMode == agcommon::QuoteMode::SIMLIVE) {

        if (not liveQuoteFeedPtr->isRuning()) {

            liveQuoteFeedPtr->run();
        }

        return liveQuoteFeedPtr;
    }

    /*customized generated quote: unimplemented, for special quote pattern trainning*/
    if (req.quoteMode == agcommon::QuoteMode::SIMCUST) {
        algoErrMessage = "user defind quote simulator not implemented yet!";
        return nullptr;
    }

    /*replay history quote*/
    if (req.quoteMode == agcommon::QuoteMode::REPLAY) {

        assert(req.dispatchContextPtr != nullptr);

        {
            std::scoped_lock lock(m_mutex);

            auto it = replayQuoteFeedPtrs.find(req.algoOrderId);
            if (it != replayQuoteFeedPtrs.end()) {
                return it->second;
            }
        }

        if (req.algoOrderId == 0 || 
            req.startTime == boost::posix_time::min_date_time || 
            req.endTime   == boost::posix_time::min_date_time || 
            req.startTime >= req.endTime) {

            algoErrMessage = "REPLAY Mode should not (algoOrderId/startTime/endTime) empty or startTime>=endTime";

            return nullptr;
        }

        auto quoteFeedPtr = std::make_shared<QuoteFeedReplay>(req);

        auto st_str =  agcommon::getDateTimeStr(req.startTime);
        auto end_str = agcommon::getDateTimeStr(req.endTime);

        agcommon::TimeCost tc(std::format("[{}]read Tick {} symbols:{}-{}",req.algoOrderId,req.symbolSet.size(), st_str, end_str));
        
        auto& dataSource = StockDataManager::getInstance();

        auto lines = dataSource.cacheFromH5Tick(req.symbolSet, req.startTime, req.endTime, quoteFeedPtr->m_quoteTime2Symbol2md);
        
        tc.timeAt(std::format(",readlines:{}",lines));

        if (quoteFeedPtr->m_quoteTime2Symbol2md.size() == 0) {

            algoErrMessage = std::format("symbols Data Range [{},{}] NOT FOUND.", st_str, end_str);

            return nullptr;
        }

        std::scoped_lock lock(m_mutex);

        const auto& [it,inserted] = replayQuoteFeedPtrs.emplace(req.algoOrderId, quoteFeedPtr);

        if (not inserted) {
            SPDLOG_WARN("Mulit create replayQuote:{}", req.algoOrderId);
        }

        return quoteFeedPtr;
    }

    return nullptr;
}

  void QuoteFeedService::removeQuoteFeed(const AlgoOrderId_t& algoOrderId) {

      std::scoped_lock lock(m_mutex);

      if (auto it = replayQuoteFeedPtrs.find(algoOrderId);it != replayQuoteFeedPtrs.end()) {

          replayQuoteFeedPtrs.erase(it);

          SPDLOG_DEBUG("remove QuoteFeed algoOrderId:{}", algoOrderId);
      }
  }