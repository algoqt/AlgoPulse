#pragma once

#include "typedefs.h"
#include "QuoteFeed.h"

class QuoteFeedService {

public:
    static QuoteFeedService& getInstance() {

		static QuoteFeedService instance{};

		return instance;
	}

    std::shared_ptr<QuoteFeed> createQuoteFeed(const QuoteFeedRequest& req, AlgoErrorMessage_t& algoErrMessage);

    std::shared_ptr<QuoteFeed> queryDataFeed(const AlgoOrderId_t& key);

    void removeQuoteFeed(const AlgoOrderId_t& algoOrderId);

private:

    QuoteFeedService()  = default;

    ~QuoteFeedService() = default;

    std::mutex m_mutex;

    /* backtest only*/
    std::map<AlgoOrderId_t, std::shared_ptr<QuoteFeed>> replayQuoteFeedPtrs{};

};
