#pragma once

#include "typedefs.h"
#include "common.h"

#include "QuoteFeedService.h"
#include "Order.h"
#include <memory>
#include "OrderBookSim.h"
#include "ContextService.h"
#include "StorageService.h"

/*
* rout different AcctKey_t to its broker OrderSerivce
*/

class OrderService {

public:

	static OrderService& getInstance() {

		static OrderService instance {};
		return instance;
	}

	std::shared_ptr<OrderBook> createOrderBookIfNotExist(const OrderBookRequest& req, AlgoErrorMessage_t& algoErrMessage);

	std::shared_ptr<OrderBook> queryOrderBook(const OrderBookKey_t& key);


	void removeOrderBook(const OrderBookRequest& req);

	bool placeOrder(const Order* order);

	bool cancelOrder(const Order* order);

	bool cancelOrderByOrderId(const OrderBookKey_t& orderBookKey
		, const OrderId_t& orderId
		, const agcommon::QuoteMode& quoteMode);

	void onOrderUpdate(const Order* order);

	void onTrade(const Trade* trade);

	void testDelay(agcommon::TimeCost& delay);

private:

	OrderService();

	~OrderService();

	AsioContextPtr		m_runContextPtr;

	std::mutex			m_mutex;

	std::map<agcommon::QuoteMode,std::map<OrderBookKey_t, std::shared_ptr<OrderBook>>>    q2RouterKey2OrderBook{};


};