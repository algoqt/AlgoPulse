#include "AlgoPlacer.h"
#include "common.h"
#include "OrderService.h"
#include "Configs.h"

AlgoPlacer::AlgoPlacer(const std::shared_ptr<asio::io_context>& c) :
	contextPtr{ c } {
};

void AlgoPlacer::setOnFusionOrderUpdateCallback(const OnFusionOrderUpdateCallback& on
	, const std::shared_ptr<asio::io_context>& callBackContext) {

	onFusionOrderCallback = on;

	fusionOrderCallbackContext = callBackContext;
}

void AlgoPlacer::placeAlgoOrder(const Order* _order
	, float execDurationSeconds /*= 90*/ 
	, bool isPriceLimit		/*= false*/
	, const OnFusionOrderUpdateCallback&		_callback /*= nullptr*/
	, const std::shared_ptr<asio::io_context>&	_callBackContext /*= nullptr*/ ) {

	auto order = Order::make_intrusive(*_order);

	const auto& callback        =        _callback != nullptr ? _callback : onFusionOrderCallback;
	const auto& callbackContext = _callBackContext != nullptr ? _callBackContext : fusionOrderCallbackContext;

	if (agcommon::isBackTestAcctType(_order->acctType)) {

		auto orderBookKey = order->getOrderBookKey();
		auto quoteFeedKey = order->algoOrderId;

		//auto orderBookPtr = OrderService::getInstance().queryOrderBook(orderBookKey);
		auto quoteFeedPtr = QuoteFeedService::getInstance().queryDataFeed(quoteFeedKey);	//策略本身需创建好QuoteFeed

		if (quoteFeedPtr == nullptr) {

			order->status = agcommon::OrderStatus::REJECTED;
			order->text = "quoteFeed is nullptr.";
		}
		else {
			auto startTime = quoteFeedPtr->getCurrentQuoteTime();		//当前回放的时间点
			auto endTime   = agcommon::AshareMarketTime::addMarketDuration(startTime, execDurationSeconds);

			double vwap = quoteFeedPtr->getVWAP(order->symbol, startTime, endTime);

			order->status = agcommon::OrderStatus::FILLED;
			order->avgPrice  = vwap;
			order->filledQty = order->orderQty;
			order->filledAmt = order->orderQty * vwap;

		}

		if (callback) {
			if (callbackContext)
				asio::dispatch(*callbackContext, [callback, order]() {
					callback(order.get());
				}
			);
			else
				callback(order.get());
		}

		return;
	}

	auto algoOrder = std::make_shared<AlgoOrder>();

	algoOrder->setAlgoOrderId(AlgoOrderIdGenerator::getInstance().NewId());

	algoOrder->setAcct(order->acct);
	algoOrder->setAcctType(order->acctType);
	auto broker = agcommon::Configs::getConfigs().getBrokerIdFromAcct(order->acctType, order->acct);
	algoOrder->setBrokerId(broker);
	algoOrder->setAlgoStrategy(AlgoMsg::VWAP);
	algoOrder->setAlgoCategory(AlgoMsg::Category_ALGO);

	algoOrder->setExecDuration(execDurationSeconds);
	//algoOrder->setStartTime(startTime);
	//algoOrder->setEndTime(endTime);
	algoOrder->setSymbol(order->symbol);
	algoOrder->setTradeSide(order->tradeSide);
	algoOrder->setQtyTarget(order->orderQty);

	algoOrder->setClientAlgoOrderId(std::to_string(order->algoOrderId));

	if (isPriceLimit)
		algoOrder->setPriceLimit(order->orderPrice);

	algoOrder->setNotBuyOnLLOrSellOnHL(true);

	algoOrder->setNotPegOrderAtLimitPrice(true);

	auto traderPtr = std::make_shared<AlgoTrader>(algoOrder, contextPtr);

	auto algoContext = std::make_shared<AlgoContext>(order, callback, callbackContext, traderPtr);

	auto _onAlgoPerformanceUpdate = [algoContext](const AlgoPerformance& algoPerf) {

		AlgoPlacer::onAlgoPerformanceUpdate(algoContext, algoPerf);

		};

	traderPtr->onAlgoPerformanceUpdate = _onAlgoPerformanceUpdate;

	asio::co_spawn(*contextPtr
		, traderPtr->start()
		, [self = shared_from_this(), algoContext] (const std::exception_ptr ex) {
			self->onAlgoInstanceFinished(algoContext, ex);
		}
	);

	std::scoped_lock<std::mutex> lock(m_mutex);

	orderId2algoContext.emplace(order->orderId, algoContext);
}

void AlgoPlacer::cancelAlgoOrder(const OrderId_t& orderId) {

	std::scoped_lock<std::mutex> lock(m_mutex);

	auto it = orderId2algoContext.find(orderId);

	if (it != orderId2algoContext.end()) {
		auto& trader = it->second->traderPtr;
		asio::post(*trader->contextPtr, [trader]() {
			trader->cancel();
			}
		);
	}

}

int AlgoPlacer::cancelAllAlgoOrder() {

	std::scoped_lock<std::mutex> lock(m_mutex);
	int runnings = 0;
	for(auto& [orderId,algoContext]:orderId2algoContext) {
		auto& trader = algoContext->traderPtr;
		asio::post(*trader->contextPtr, [trader]() {
			trader->cancel();
			}
		);
		runnings++;
	}
	return runnings;
}

size_t AlgoPlacer::hasPendingOrders() {

	std::scoped_lock<std::mutex> lock(m_mutex);
	
	return orderId2algoContext.size();
}

void AlgoPlacer::onAlgoPerformanceUpdate(const std::shared_ptr<AlgoContext>& algoContext, const AlgoPerformance& algoPerf) {

	auto& order = algoContext->orderFusion;

	order->filledQty = algoPerf.qtyFilled;
	order->avgPrice = algoPerf.avgPrice;
	order->filledAmt = algoPerf.amtFilled;

	if (algoPerf.algoStatus == agcommon::AlgoStatus::Terminated or algoPerf.algoStatus == agcommon::AlgoStatus::Expired) {

		order->cancelQty = order->orderQty - order->filledQty;

		if (order->filledQty > 0)
			order->status = agcommon::OrderStatus::PARTIALCANCEL;
		else
			order->status = agcommon::OrderStatus::CANCELED;
	}
	else {

		if (algoPerf.algoStatus == agcommon::AlgoStatus::Error) {
			order->status = agcommon::OrderStatus::REJECTED;
			order->text = algoPerf.errMsg;
		}
		else if (algoPerf.algoStatus == agcommon::AlgoStatus::Finished) {
			order->status = agcommon::OrderStatus::FILLED;
		}
		else {
			order->status = agcommon::OrderStatus::NEW;
		}
	}

	if (algoContext->onFusionOrderCallback) {
		
		auto& callbackContext = algoContext->onFusionOrderCallbackRunContext;

		if(callbackContext == nullptr)
			callbackContext =  algoContext->traderPtr->contextPtr;
		
		asio::dispatch(*callbackContext, [algoContext, order]() {algoContext->onFusionOrderCallback(order.get()); });
	}

}