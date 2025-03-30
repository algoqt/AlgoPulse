#include "Task_onStartAlgoInstance.h"
#include "StockDataManager.h"
#include "IdGenerator.h"


AlgoErrorCode Task_onStartAlgoInstance::dispatchAlgoOrder(const std::shared_ptr<AlgoOrder>& algoOrder
	, AlgoErrorMessage_t& algoErrMessage) {

	auto& algoSerive = AlgoService::getInstance();
	AlgoErrorCode algoErrorCode = AlgoErrorCode::ALGO_OK;

	auto clientAlgoOrderIdKey = std::make_pair(algoOrder->getAcctKey(), algoOrder->clientAlgoOrderId);

	std::shared_ptr<Trader> traderPtr;

	switch (algoOrder->algoCategory)
	{
	case AlgoMsg::MsgAlgoCategory::Category_ALGO: {
		traderPtr = std::make_shared<AlgoTrader>(algoOrder);
		break;
	}
	case AlgoMsg::MsgAlgoCategory::Category_SHOT: {
		traderPtr = std::make_shared<ShotTrader>(algoOrder);
		break;
	}
	//case AlgoMsg::MsgAlgoCategory::Category_HIT: {
	//	traderPtr = std::make_shared<ShotTrader>(algoOrder);
	//	break;
	//}
	//case AlgoMsg::MsgAlgoCategory::Category_SPREAD: {
	//	traderPtr = std::make_shared<PairTrader>(algoOrder);
	//	break;
	//}
	//case AlgoMsg::MsgAlgoCategory::Category_T0: {
	//	traderPtr = std::make_shared<PairTrader>(algoOrder);
	//	break;
	//}

	default:
		algoErrMessage = fmt::format("{},unsupport algo_category:{}.", algoOrder->clientAlgoOrderId, AlgoMsg::MsgAlgoCategory_Name(algoOrder->algoCategory));
		SPDLOG_ERROR(algoErrMessage);
		return AlgoErrorCode::ALGO_ERROR;
	}

	if (algoErrorCode == AlgoErrorCode::ALGO_OK and algoErrMessage == "") {

		algoSerive.m_totalTraders.fetch_add(1);

		if (algoOrder->isBackTestOrder())
		{
			auto shouldQueue = algoSerive.m_runningTraders_bt_costMutiContext.load() != 0 or algoSerive.m_runningTraders_bt.load() >= algoSerive.m_numAlgoWorkers;

			if (shouldQueue) {

				algoSerive.m_algoTraderQueue_bt.push_back(traderPtr);

				traderPtr->publishAlgoperformance();

				SPDLOG_INFO("[{}]algoTraderQueue SIZE:{}", AlgoMsg::MsgAlgoCategory_Name(algoOrder->algoCategory), algoSerive.m_algoTraderQueue_bt.size());
			}
			else {

				algoSerive.m_runningTraders.fetch_add(1);

				algoSerive.m_runningTraders_bt.fetch_add(1);

				if (traderPtr->getAlgoCategory() != AlgoMsg::Category_ALGO) {
					algoSerive.m_runningTraders_bt_costMutiContext.fetch_add(1);
				}

				asio::co_spawn(*traderPtr->contextPtr, traderPtr->start(), std::bind(&Task_onStartAlgoInstance::onAlgoInstanceFinished, this, traderPtr, std::placeholders::_1));
			}
		}
		else
		{
			algoSerive.m_runningTraders.fetch_add(1);

			asio::co_spawn(*traderPtr->contextPtr, traderPtr->start(), std::bind(&Task_onStartAlgoInstance::onAlgoInstanceFinished, this, traderPtr, std::placeholders::_1));
		}

		algoErrorCode = AlgoErrorCode::ALGO_OK;

	}
	else {

		asio::post(*traderPtr->contextPtr, [traderPtr, algoErrMessage]() { traderPtr->stop(algoErrMessage); });

		SPDLOG_ERROR("[aId:{}]AlgoTrader::preStartCheck() failed:{}", algoOrder->algoOrderId, algoErrMessage);

		algoErrorCode = AlgoErrorCode::ALGO_ORDERCHECK_FAILED;
	}

	if (algoErrorCode != AlgoErrorCode::ALGO_DUPLICATE_ERROR) {

		algoSerive.algoOrderId2AlgoTrader.emplace_hint(algoSerive.algoOrderId2AlgoTrader.end(), algoOrder->algoOrderId, traderPtr);

	}

	return algoErrorCode;
}


std::vector<std::shared_ptr<AlgoOrder>> Task_onStartAlgoInstance::createAlgoOrder(const AlgoMsg::MsgAlgoInstanceCreateRequest& req
	, AlgoErrorMessage_t& algoErrorMessage) {

	static std::set<std::string> indexCodes{ "hs300","zz500","zz1000","other" };

	std::vector<std::shared_ptr<AlgoOrder>> algoOrders;

	std::unordered_set<Symbol_t> symbols;

	auto startTime = agcommon::parseDateTimeInteger(req.algo_order().start_time());
	auto endTime = agcommon::parseDateTimeInteger(req.algo_order().end_time());

	if (not startTime or not endTime) {
		algoErrorMessage = fmt::format("invalid date format:{},{}", req.algo_order().start_time(), req.algo_order().end_time());
		return algoOrders;
	}

	uint32_t startDate = req.algo_order().start_time() / 1000000;
	uint32_t endDate = req.algo_order().end_time() / 1000000;

	auto tradeDates = StockDataManager::getInstance().getTradeDateInts(startDate, endDate);

	if (tradeDates.empty()) {
		//tradeDates.insert(endDate);
		algoErrorMessage = fmt::format("no trading dates between:{},{}", startDate, endDate);
		return algoOrders;
	}

	AlgoMsg::MsgAlgoCategory algoCategory = AlgoMsg::MsgAlgoCategory::Category_ALGO;

	switch (req.algo_strategy())
	{
	case AlgoMsg::MsgAlgoStrategy::T0: {
		algoCategory = AlgoMsg::MsgAlgoCategory::Category_T0;
		break;
	}
	case AlgoMsg::MsgAlgoStrategy::SPREAD: {
		algoCategory = AlgoMsg::MsgAlgoCategory::Category_SPREAD;
		break;
	}
	case AlgoMsg::MsgAlgoStrategy::SHOT: {
		algoCategory = AlgoMsg::MsgAlgoCategory::Category_SHOT;
		break;
	}
	case AlgoMsg::MsgAlgoStrategy::HIT: {
		algoCategory = AlgoMsg::MsgAlgoCategory::Category_HIT;
		break;
	}
	default:
		algoCategory = AlgoMsg::MsgAlgoCategory::Category_ALGO;
		break;
	}
	auto broker = agcommon::Configs::getConfigs().getBrokerIdFromAcct(req.acct_type(), req.acct());

	for (auto& tradeDate : tradeDates) {

		uint64_t startTime = (uint64_t)tradeDate * 1000000 + req.algo_order().start_time() % 1000000;
		uint64_t endTime = (uint64_t)tradeDate * 1000000 + req.algo_order().end_time() % 1000000;

		SPDLOG_DEBUG(" {},{},{}", tradeDate, startTime, endTime);

		auto& symbol_req = req.algo_order().symbol();
		if (indexCodes.contains(symbol_req) and algoCategory == AlgoMsg::MsgAlgoCategory::Category_ALGO) {

			symbols = StockDataManager::getInstance().getIndexConstituents(tradeDate, symbol_req);
		}

		else {
			symbols.insert(symbol_req);
		}

		for (auto& symbol : symbols) {

			auto algoOrder = std::make_shared<AlgoOrder>();

			algoOrder->setAlgoOrderId(AlgoOrderIdGenerator::getInstance().NewId());
			algoOrder->setAcct(req.acct());
			algoOrder->setAcctType(req.acct_type());
			algoOrder->setBrokerId(broker);
			algoOrder->setAlgoStrategy(req.algo_strategy());
			algoOrder->setAlgoCategory(algoCategory);
			algoOrder->setStartTime(*agcommon::parseDateTimeInteger(startTime));
			algoOrder->setEndTime(*agcommon::parseDateTimeInteger(endTime));
			algoOrder->setExecDuration(req.algo_order().exec_duration());
			algoOrder->setSymbol(symbol);
			algoOrder->setTradeSide(req.algo_order().order_side());
			algoOrder->setQtyTarget(req.algo_order().order_qty());
			algoOrder->setAmtTarget(req.algo_order().order_amount());
			algoOrder->setClientAlgoOrderId(req.algo_order().client_algo_order_id());
			algoOrder->setMinAmountPerOrder(req.algo_order().min_suborder_amount());
			algoOrder->setParticipateRate(req.algo_order().max_pov());
			algoOrder->setPriceLimit(req.algo_order().order_price());
			SPDLOG_DEBUG("....{}", req.algo_order().algo_params());

			algoOrder->paramKeyconfig = agcommon::parseStringToMap(req.algo_order().algo_params());

			const auto& originalParamMap = agcommon::Configs::getConfigs().getAlgoParamMap(algoOrder->algoCategory);

			for (const auto& [k, v] : originalParamMap) {

				algoOrder->paramKeyconfig.try_emplace(k, v);
			}

			if (auto it = algoOrder->paramKeyconfig.find("notBuyOnLLOrSellOnHL"); it != algoOrder->paramKeyconfig.end()) {
				auto v = agcommon::toLower(it->second) == "true" ? true : false;
				algoOrder->setNotBuyOnLLOrSellOnHL(v);
			}

			if (auto it = algoOrder->paramKeyconfig.find("notPegOrderAtLimitPrice"); it != algoOrder->paramKeyconfig.end()) {
				auto v = agcommon::toLower(it->second) == "true" ? true : false;
				algoOrder->setNotPegOrderAtLimitPrice(v);
			}
			algoOrders.push_back(algoOrder);
		}
	}

	return algoOrders;
}

void Task_onStartAlgoInstance::onAlgoInstanceFinished(const std::shared_ptr<Trader>& traderPtr, const std::exception_ptr ex) {

	auto& algoSerive = AlgoService::getInstance();

	auto algoOrderId = traderPtr->getAlgoOrderId();

	auto algoOrderPtr = traderPtr->getAlgoOrder();

	auto algoCategory = traderPtr->getAlgoCategory();

	algoSerive.m_runningTraders.fetch_sub(1);

	if (!ex) {
		SPDLOG_INFO("[aId:{}]co_spawn done  ,totoal:{},running:{}", algoOrderId, algoSerive.m_totalTraders.load(), algoSerive.m_runningTraders.load());
	}
	else {

		SPDLOG_ERROR("[aId:{}]co_spawn failed,totoal:{},running:{}", algoOrderId, algoSerive.m_totalTraders.load(), algoSerive.m_runningTraders.load());
		try {

			std::rethrow_exception(ex);
		}
		catch (const std::exception& e) {
			SPDLOG_ERROR("[aId:{}]exception:{}", algoOrderId, e.what());
		}
	}

	if (algoOrderPtr->isBackTestOrder()) {

		algoSerive.m_runningTraders_bt.fetch_sub(1);

		if (traderPtr->getAlgoCategory() != AlgoMsg::Category_ALGO) {
			algoSerive.m_runningTraders_bt_costMutiContext.fetch_sub(1);
		}

		asio::post(*algoSerive.m_dispatcherContextPtr, [this, traderPtr, algoOrderId, algoOrderPtr]() {

			auto& algoSerive = AlgoService::getInstance();

			if (not algoSerive.m_algoTraderQueue_bt.empty()) {

				auto& new_trader = algoSerive.m_algoTraderQueue_bt.front();

				asio::co_spawn(*new_trader->contextPtr, new_trader->start(), std::bind(&Task_onStartAlgoInstance::onAlgoInstanceFinished, this, new_trader, std::placeholders::_1));

				algoSerive.m_algoTraderQueue_bt.pop_front();

				algoSerive.m_runningTraders_bt.fetch_add(1);

				algoSerive.m_runningTraders.fetch_add(1);

				if (traderPtr->getAlgoCategory() != AlgoMsg::Category_ALGO) {
					algoSerive.m_runningTraders_bt_costMutiContext.fetch_add(1);
				}

				SPDLOG_INFO("[REPLAY]running size:{},queue size:{}", algoSerive.m_runningTraders_bt.load(), algoSerive.m_algoTraderQueue_bt.size());
			}
			});
	}

}

