#pragma once

#include "QuoteFeed.h"
#include <../third_party/requests/Exception.hpp>
#include <../third_party/requests/Request.hpp>
#include <../third_party/requests/AsyncRequest.hpp>
#include <../third_party/requests/Url.hpp>
#include "MemoryContainer.h"

/*
* fecth marketDepth from tx :http://qt.gtimg.cn
*/
class QuoteFeedInternet : public QuoteFeed {

public:

    QuoteFeedInternet(const QuoteFeedRequest& req);

    ~QuoteFeedInternet() = default;  

    void run() override;

    void stop() override;

    bool isRuning() override { return m_keepRuning.load(); };

    SubscribeKey_t subscribe(std::shared_ptr<SubMarketDepth_t> subPtr) override;

    void unSubscribe(const uint64_t subcribeKey) override;

private:

    asio::awaitable<void>   co_run();
    
private:

    QuoteFeedRequest                       m_request{};

    std::atomic<bool>                      m_keepRuning{ false };

    VectorPool<MarketDepth*>               m_marketDepthVecPool;

    VectorPool<std::string>                m_respDataVecPool;

    UnorderMarketDepthRawPtrMap            m_symbol2md;

    uint64_t                               m_fetchCount{ 0 };

    std::string                            m_url       { "http://qt.gtimg.cn" };

    std::unordered_map<Symbol_t, size_t>         m_symbolSubCounts{};

    std::vector<std::pair<size_t, std::string>>  m_batchSymbolString2Fetch{};

    void genSymbolBatchs2Fetch();

    void requestResponseHandler(int batchSize,requests::Response& resp);

    void onMarketDepth(std::shared_ptr<std::vector<MarketDepth*>> newMds);

public:

    static MarketDepth* parseMarketDepth_tx(const std::string& text);

    std::shared_ptr<QuoteFeedInternet> shared_from_this() {

        return std::static_pointer_cast<QuoteFeedInternet>(QuoteFeed::shared_from_this());
    }
};

