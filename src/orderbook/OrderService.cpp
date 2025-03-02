#include "OrderService.h"
#include "TCPSession.h"


OrderService::OrderService() :m_runContextPtr(ContextService::getInstance().createContext("OrderService", 1)) {};

OrderService::~OrderService() { ContextService::getInstance().stopContext("OrderService"); };

std::shared_ptr<OrderBook> OrderService::queryOrderBook(const OrderBookKey_t& key) {

	std::scoped_lock lock(m_mutex);

	auto& orderBooks = q2RouterKey2OrderBook[agcommon::QuoteMode::REPLAY];

	auto it = orderBooks.find(key);

	if (it == orderBooks.end()) {
		return nullptr;
	}
	return it->second;
}

std::shared_ptr<OrderBook> OrderService::createOrderBookIfNotExist(const OrderBookRequest& req, AlgoErrorMessage_t& algoErrMessage) {

	std::scoped_lock lock(m_mutex);

	auto& orderBookMap = q2RouterKey2OrderBook[req.quoteMode];

	auto it = orderBookMap.find(req.orderBookKey);

	if (it == orderBookMap.end()) {
		
		std::shared_ptr<OrderBook> orderBookPtr = nullptr;

		switch (req.quoteMode)
		{
		case agcommon::QuoteMode::LIVE: {

			auto orderBookReq = req;
			// each broker  one context
			auto brokerContxt = fmt::format("OrderBookLive_broker_{}", req.orderBookKey.broker);

			orderBookReq.runContextPtr = ContextService::getInstance().createContext(brokerContxt);

			orderBookPtr = std::make_shared<OrderBookLive>(req);

			break;
		}
		case agcommon::QuoteMode::SIMLIVE: {

			auto orderBookReq = req;

			orderBookReq.runContextPtr = ContextService::getInstance().createContext("OrderBookSim_",1);

			orderBookPtr = std::make_shared<OrderBookSim>(orderBookReq);

			break;
		}
		case agcommon::QuoteMode::REPLAY: {

			orderBookPtr = std::make_shared<OrderBookSim>(req);

			break;
		}
		case agcommon::QuoteMode::SIMCUST: {

			orderBookPtr = std::make_shared<OrderBookSimCust>(req);
			break;
		}
		default:

			SPDLOG_ERROR("unSupport quoteMode:{}", static_cast<int>(req.quoteMode) );
			break;
		}

		if (orderBookPtr!=nullptr) {

			SPDLOG_INFO("ADD orderRouterKey :{},QuoteMode:{}",req.orderBookKey, agcommon::getQuoteModeName(req.quoteMode));

			orderBookPtr->run();

			orderBookMap.emplace(req.orderBookKey, orderBookPtr);

		}
		else {

			algoErrMessage = fmt::format("OrderService::createOrderRouterIfNotExist failed,\
				orderRouterKey:[{}],invalid QuoteMode:{}", req.orderBookKey	, agcommon::getQuoteModeName(req.quoteMode));
			SPDLOG_ERROR(algoErrMessage);
		}

		return orderBookPtr;

	}

	return it->second;
}

void OrderService::removeOrderBook(const OrderBookRequest& req) {

	std::scoped_lock lock(m_mutex);

	auto& orderBooks = q2RouterKey2OrderBook[req.quoteMode];

	if (auto it = orderBooks.find(req.orderBookKey); it != orderBooks.end()) {
		orderBooks.erase(it);
		SPDLOG_INFO("remove orderRouterKey :{},QuoteMode:{}", req.orderBookKey,static_cast<int>(req.quoteMode));
	}
}

bool OrderService::placeOrder(const Order* order) {

	if (order == nullptr) return false;

	auto quoteMode = agcommon::getQuoteMode(order->acctType);

	auto orderBookKey = order->getOrderBookKey();

	std::scoped_lock lock(m_mutex);

	auto& orderBooks = q2RouterKey2OrderBook[quoteMode];
	
	if (quoteMode == agcommon::QuoteMode::REPLAY && order->algoOrderId==0) {

		SPDLOG_ERROR("OrderService::placeOrder failed,Replay Mode algoOrderId should not 0.");
		return false;
	}
	if (auto it = orderBooks.find(orderBookKey); it != orderBooks.end()) {

		return it->second->placeOrder(order);
	}
	else {
		SPDLOG_ERROR("OrderService::placeOrder failed,OrderRouter NOT FOUND \
			,Call createOrderRouterIfNotExist first,orderRouterKey:{},QuoteMode:{}", orderBookKey, static_cast<int>(quoteMode));
	}

	return false;
}

bool OrderService::cancelOrder(const Order* order) {

	if (order == nullptr) return false;

	auto quoteMode = agcommon::getQuoteMode(order->acctType);

	std::scoped_lock lock(m_mutex);

	auto& orderBooks = q2RouterKey2OrderBook[quoteMode];

	auto orderBookKey = order->getOrderBookKey();

	if (quoteMode == agcommon::QuoteMode::REPLAY && order->algoOrderId==0) {

		SPDLOG_ERROR("OrderService::placeOrder failed,Replay Mode algoOrderIdshould not empty.{}", order->to_string());
		return false;
	}

	if (orderBooks.find(orderBookKey) != orderBooks.end()) {

		return orderBooks[orderBookKey]->cancelOrderByOrderId(order->orderId);
	}

	SPDLOG_ERROR("OrderService::cancelOrder failed,Order Router{} ,Call createOrderRouterIfNotExist first,QuoteMode:{}", orderBookKey, static_cast<int>(quoteMode));
	return false;
}

bool OrderService::cancelOrderByOrderId(const OrderBookKey_t& orderBookKey, const OrderId_t& orderId, const agcommon::QuoteMode& quoteMode) {

	std::scoped_lock lock(m_mutex);

	auto& orderBooks = q2RouterKey2OrderBook[quoteMode];

	if (orderBooks.find(orderBookKey) != orderBooks.end()) {

		return orderBooks[orderBookKey]->cancelOrderByOrderId(orderId);

	}

	SPDLOG_ERROR("OrderService::cancelOrderWithId failed,Order Router{} NOT FOUND,Should Call createOrderRouterIfNotExist first.orderId:{}."
		, orderBookKey,orderId);
	return false;
}


void OrderService::onOrderUpdate(const Order* order) {

	auto msgPtr = std::make_shared<AlgoMsg::MsgOrderInfo>();

	msgPtr->set_order_id(order->orderId);
	msgPtr->set_acct(order->acct);
	msgPtr->set_acct_type(order->acctType);
	msgPtr->set_algo_order_id(order->algoOrderId);
	msgPtr->set_order_time(agcommon::getDateTimeInt(order->orderTime));

	msgPtr->set_symbol(order->symbol);
	msgPtr->set_order_side((uint32_t)order->tradeSide);
	msgPtr->set_order_qty((uint32_t)order->orderQty);
	msgPtr->set_order_price(order->orderPrice);

	msgPtr->set_qty_filled(order->filledQty);
	msgPtr->set_qty_canceled(order->cancelQty);
	msgPtr->set_price_avg_filled(order->avgPrice);
	msgPtr->set_amount_filled(order->filledAmt);

	msgPtr->set_broker(std::to_string(order->brokerId));
	msgPtr->set_broker_order_no(order->brokerOrderNo);
	msgPtr->set_pair_id(order->pairId);

	msgPtr->set_order_status(order->status);
	msgPtr->set_note_text(order->text);

	msgPtr->set_client_algo_order_id("");
	msgPtr->set_update_time(agcommon::getDateTimeInt(order->updTime));


	auto acctKey = AcctKey_t(order->acctType, order->acct, order->brokerId);

	auto shouldCache = order->isFinalStatus();

	TCPSessionManager::getInstance().sendNotify2C(acctKey, AlgoMsg::CMD_NOTIFY_Order, msgPtr, shouldCache);
}

void OrderService::onTrade(const Trade* trade) {

	auto msgPtr = std::make_shared<AlgoMsg::MsgTradeInfo>();

	msgPtr->set_trade_id(trade->tradeId);
	msgPtr->set_order_id(trade->orderId);
	msgPtr->set_trade_time(agcommon::getDateTimeInt(trade->filledTime));

	msgPtr->set_acct(trade->acct);
	msgPtr->set_acct_type(trade->acctType);

	msgPtr->set_symbol(trade->symbol);
	msgPtr->set_order_side(trade->tradeSide);
	msgPtr->set_qty_filled(trade->filledQty);
	msgPtr->set_price_filled(trade->price);
	msgPtr->set_amount_filled(trade->filledAmt);

	msgPtr->set_broker(trade->brokerId);
	msgPtr->set_broker_order_no(trade->brokerOrderNo);

	msgPtr->set_fee(trade->fee);

	SPDLOG_DEBUG("{}", trade->to_string());

	auto acctKey = AcctKey_t(trade->acctType, trade->acct, trade->brokerId);

	TCPSessionManager::getInstance().sendNotify2C(acctKey, AlgoMsg::CMD_NOTIFY_Trade, msgPtr,true);

}

void OrderService::testDelay(agcommon::TimeCost& delay) {

	std::scoped_lock lock(m_mutex);

	for (auto& [quoteMode, orderBooks] : q2RouterKey2OrderBook) {

		for (auto& [orderBookKey, orderBook] : orderBooks) {

			orderBook->onDelayTest(delay);
			delay.logTimeCost();
		}
	}
}