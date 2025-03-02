#include "QuoteFeed.h"

// OrderBook susbribe QuoteFeed
void QuoteFeed::regesterOrderBookCallback(const OrderBookKey_t& orderBookKey
    , const OnMarketDepthCallback& onMarketDepth
    , const OnDelayTestCallback& onDelay /*= nullptr*/) {

    assert(onMarketDepth !=nullptr);

    auto self = shared_from_this();

    asio::dispatch(m_strand, [self, orderBookKey, onMarketDepth, onDelay]() {

        self->m_orderBookKey2CallBack[orderBookKey] = { onMarketDepth,onDelay };

        SPDLOG_INFO("orderBookKey regester callback:{}", orderBookKey);

    });
}

void QuoteFeed::unRegesterOrderBookCallback(const OrderBookKey_t& orderBookKey) {

    auto self = shared_from_this();
    asio::dispatch(m_strand, [self, orderBookKey]() {
        if (self->m_orderBookKey2CallBack.find(orderBookKey) != self->m_orderBookKey2CallBack.end()) {
            self->m_orderBookKey2CallBack.erase(orderBookKey);
        }
        }
    );
}

void QuoteFeed::regesterOnQuoteFeedFinish(const std::function<void()>& f,const AsioContextPtr& contextPtr) {

    auto self = shared_from_this();
    asio::dispatch(m_strand, [self, contextPtr,f]() {
        self->onQuoteFeedFinisheds.push_back(std::make_pair(f, contextPtr));
        });
};







