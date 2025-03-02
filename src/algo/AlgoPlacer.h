#pragma once

#include "AlgoTrader.h"
#include "Order.h"

typedef std::function<void(const Order*)> OnFusionOrderUpdateCallback;

struct AlgoContext {

	boost::intrusive_ptr<Order>			orderFusion;

	OnFusionOrderUpdateCallback			onFusionOrderCallback;

	std::shared_ptr<asio::io_context>	onFusionOrderCallbackRunContext;

	std::shared_ptr<AlgoTrader>			traderPtr;


};

/*
 将普通订单转为使用算法执行，并将算法执行结果模拟为订单回报，回调调用方的onOrderUpdateCallback
*/

class AlgoPlacer: public std::enable_shared_from_this<AlgoPlacer> {

public:

	explicit AlgoPlacer(const std::shared_ptr<asio::io_context>& c);

	void setOnFusionOrderUpdateCallback(const OnFusionOrderUpdateCallback& on
		, const std::shared_ptr<asio::io_context>& callBackContext = nullptr);

	void placeAlgoOrder(const Order* _order
		, float execDurationSeconds = 90
		, bool isPriceLimit = false
		, const OnFusionOrderUpdateCallback & on = nullptr
		, const std::shared_ptr<asio::io_context>& callBackContext = nullptr
	
	);

	void cancelAlgoOrder(const OrderId_t& orderId); 

	int cancelAllAlgoOrder();

	size_t hasPendingOrders();

private:

	std::shared_ptr<asio::io_context>	contextPtr;

	OnFusionOrderUpdateCallback			onFusionOrderCallback{ nullptr };

	std::shared_ptr<asio::io_context>   fusionOrderCallbackContext{nullptr};

	std::mutex							m_mutex{};

	std::unordered_map<OrderId_t, std::shared_ptr<AlgoContext>>  orderId2algoContext{};

	inline void onAlgoInstanceFinished(const std::shared_ptr<AlgoContext>& algoContext, const std::exception_ptr& ex) {

		std::scoped_lock  lock(m_mutex);

		orderId2algoContext.erase(algoContext->orderFusion->orderId);
		
	}

	static void onAlgoPerformanceUpdate(const std::shared_ptr<AlgoContext>& algoContext, const AlgoPerformance& algoPerf);

};