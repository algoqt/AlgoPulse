#include "AlgoOrder.h"
#include "AlgoTrader.h"
#include "ShotTrader.h"
#include "AlgoService.h"
#include "IdGenerator.h"
#include "ContextService.h"
#include <google/protobuf/text_format.h>
#include "TCPSession.h"
#include "Configs.h"

AlgoService::AlgoService() :m_isRunning{false} {
	start();
}

AlgoService::~AlgoService() {

	stop();
}

void AlgoService::start() {
	const std::string hostConfig{ "HOSTCONFIG" };
	if(m_isRunning) {
		SPDLOG_ERROR("AlgoService is already running");
		return;
	}

	m_ipstr = agcommon::Configs::getConfigs().getConfigOrDefault(hostConfig,"ip", "127.0.0.1");
	m_port  = agcommon::Configs::getConfigs().getConfigOrDefault(hostConfig,"port", 8081);

	SPDLOG_INFO("-----AlgoService::start:{},{}------",m_ipstr, m_port);

	m_isRunning = true;
	int numNetworkThreads = 3;
	int numAcceptThreads   = 1;   // 单线程/ 无锁
	int numDispatchThreads = 1;   // 单线程/ 无锁
	int numQuoteFeedThreads = 1;

	uint32_t numCores = std::thread::hardware_concurrency();
	m_numAlgoWorkers = numCores - numNetworkThreads - numAcceptThreads - numDispatchThreads - numQuoteFeedThreads;

	SPDLOG_INFO("total cores:{},numNetThreads:{},numWorkers:{}", numCores, numNetworkThreads, m_numAlgoWorkers);

	m_netAcceptcontextPtr = ContextService::getInstance().createContext("netAccept_Context", numAcceptThreads);
	m_contextKeys.push_back("netAccept_Context");
	
	for (int i = 0; i < numNetworkThreads; i++)
	{
		auto contextKey = "netProcess_Context_" + std::to_string(i+1);
		auto contextPtr = ContextService::getInstance().createContext(contextKey, 1);
		m_netContexts.push_back(contextPtr);
		m_contextKeys.push_back(contextKey);
	}

	auto dispatchKey =   "algoDispatch_Context";
	m_dispatcherContextPtr = ContextService::getInstance().createContext(dispatchKey, numDispatchThreads);
	m_contextKeys.push_back(dispatchKey);

	for (uint16_t i = 0; i < m_numAlgoWorkers; i++) {
		auto contextKey = "worker_Context_" + std::to_string(i + 1);
		auto contextPtr = ContextService::getInstance().createContext(contextKey, 1);
		m_workerContexts.push_back(contextPtr);
		m_contextKeys.push_back(contextKey);
		m_workercontextKeys.push_back(contextKey);
	}

	accept();
}

void AlgoService::accept() {
	bool reuseAddr = false;
	m_acceptor = std::make_shared<asio::ip::tcp::acceptor>(*m_netAcceptcontextPtr
		, asio::ip::tcp::endpoint(asio::ip::address::from_string(m_ipstr), m_port), reuseAddr);

	listen();
}

void AlgoService::stop() {
	SPDLOG_INFO("-------AlgoService::stop----------");
	m_isRunning = false;
	if (m_acceptor) {
		m_acceptor->close();
	}
	for (auto& contextKey:m_contextKeys) {
		ContextService::getInstance().stopContext(contextKey);
	}
	TCPSessionManager::getInstance().closeAllSessions();
}

void AlgoService::listen()
{
	if (not m_isRunning)
		return;

	SPDLOG_INFO("AlgoService listening ...");
	if (m_netContexts.size() > 0) {
		auto idx = m_connectSeq % m_netContexts.size() ;
		auto new_session = TCPSessionManager::getInstance().createTCPSession(m_netContexts[idx]);
		m_acceptor->async_accept(new_session->socket(),
			[this, new_session](boost::system::error_code ec) { handleAccept(new_session, ec); }
		);
	}
}

void AlgoService::handleAccept(std::shared_ptr<TCPSession> new_session,
	const boost::system::error_code& error)
{
	if (not m_isRunning)
		return;

	if (!error){
		new_session->start(std::bind(&AlgoService::dispatchAlgoRequest, this, std::placeholders::_1, std::placeholders::_2));
		m_connectSeq++;
	} else{
		SPDLOG_ERROR("AlgoService accept error:{}", error.message());
	}

	listen();

}

void AlgoService::dispatchAlgoRequest(const std::shared_ptr<TCPSession> session,const AlgoMsg::MessagePkg& recvPkgPtr) {

	m_totalAlgoRequests++;

	auto cmd = recvPkgPtr.head().msg_cmd();
	SPDLOG_INFO("dispatchAlgoRequest:{},{},total:{}", recvPkgPtr.head().msg_seq_id(), (uint32_t)cmd,m_totalAlgoRequests.load());

	switch (cmd)
	{
	case AlgoMsg::MsgAlgoCMD::CMD_LoginRequest: {

		asio::post(*m_dispatcherContextPtr,[this, session, pkg=recvPkgPtr]() { onLoginRequest(session, pkg); });
		break;
	}
	case AlgoMsg::MsgAlgoCMD::CMD_AlgoInstanceCreateRequest: {

		asio::post(*m_dispatcherContextPtr, [this, session, pkg = recvPkgPtr]() { onStartAlgoInstanceRequest(session, pkg); });
		break;
	}
	case AlgoMsg::MsgAlgoCMD::CMD_AlgoInstanceUpdateRequest: {

		asio::post(*m_dispatcherContextPtr, [this, session, pkg = recvPkgPtr]() { onUpdateAlgoInstanceRequest(session, pkg); });
		break;
	}
	default:
		SPDLOG_INFO("not match AlgoMsgCMD:{}.", (uint32_t)recvPkgPtr.head().msg_cmd());
		break;
	}
}

void AlgoService::onLoginRequest(const std::shared_ptr<TCPSession> session, const AlgoMsg::MessagePkg& recvPkgPtr) {

	AlgoMsg::MsgLoginRequest req;

	auto parseOK = req.ParseFromArray(recvPkgPtr.body().data(), recvPkgPtr.body().length());

	if(not parseOK) {
		recvPkgPtr.PrintDebugString();
		std::string json;
		google::protobuf::TextFormat::PrintToString(recvPkgPtr, &json);
		SPDLOG_ERROR("parse AlgoMsg::MsgLoginRequest failed,{}", json);
		return;
	}

	auto resp = std::make_shared<AlgoMsg::MsgLoginResponse>();
	resp->set_request_id(req.request_id());
	resp->set_acct_type(req.acct_type());
	resp->set_acct(req.acct());
	resp->set_desc(req.user_name());

	auto brokerId = agcommon::Configs::getConfigs().getBrokerIdFromAcct(req.acct_type(), req.acct());
	auto acctKey = AcctKey_t(req.acct_type(), req.acct(), brokerId);

	if (TCPSessionManager::getInstance().login(acctKey,req, session)) {
		resp->set_error_code((int32_t)AlgoErrorCode::ALGO_OK);
		resp->set_error_msg("login success");
	}
	else {
		AlgoMsg::MsgLoginResponse resp;
		resp.set_error_code((int32_t)AlgoErrorCode::ALGO_ACCT_ALREADY_LOGIN);
		resp.set_error_msg(fmt::format("{},{} login failed", req.acct_type(), req.acct()));
	}

	TCPSessionManager::getInstance().sendResp2C(session, AlgoMsg::MsgAlgoCMD::CMD_LoginResponse, resp);
}

void AlgoService::onStartAlgoInstanceRequest(const std::shared_ptr<TCPSession> session, const AlgoMsg::MessagePkg& recvPkgPtr) {

	AlgoMsg::MsgAlgoInstanceCreateRequest req;

	auto parseOK = req.ParseFromArray(recvPkgPtr.body().data(), recvPkgPtr.body().length());

	auto resp = std::make_shared< AlgoMsg::MsgAlgoInstanceCreateResponse>();

	if (not parseOK) {
		SPDLOG_ERROR("parse AlgoMsg::MsgAlgoInstanceCreateRequest failed");
		resp->set_error_code((int32_t)AlgoErrorCode::ALGO_ERROR);
		resp->set_error_msg("parse AlgoMsg::MsgAlgoInstanceCreateRequest failed");
		TCPSessionManager::getInstance().sendResp2C(session, AlgoMsg::MsgAlgoCMD::CMD_AlgoInstanceCreateResponse, resp);
		return;
	}

	resp->set_request_id(req.request_id());
	resp->set_acct_type(req.acct_type());
	resp->set_acct(req.acct());

	auto requestId = req.request_id();

	std::string json;

	google::protobuf::TextFormat::PrintToString(req, &json);

	std::replace(json.begin(), json.end(), '\n', ' ');

	SPDLOG_INFO("recv AlgoOrder:{}", json);

	auto brokerId = agcommon::Configs::getConfigs().getBrokerIdFromAcct(req.acct_type(), req.acct());
	auto acctKey = AcctKey_t(req.acct_type(), req.acct(), brokerId);

	if (TCPSessionManager::getInstance().isLogin(acctKey) == false) {

		SPDLOG_ERROR("[{}][{}]acct not login,{}", requestId,req.algo_order().client_algo_order_id(), acctKey);

		resp->set_error_code((int32_t)AlgoErrorCode::ALGO_ACCT_NOT_LOGIN);

		resp->set_error_msg(fmt::format("acct not login,{}",acctKey.acct));
	}
	else {
		AlgoErrorMessage_t algoErrMessage_all;

		auto algoOrders = createAlgoOrder(req, algoErrMessage_all);

		resp->set_error_code((int32_t)AlgoErrorCode::ALGO_ERROR);

		if (algoErrMessage_all.empty()) {

			for (auto& algoOrder : algoOrders) {

				SPDLOG_INFO("[{}]create algoOrder:{}", requestId, algoOrder->to_string());

				AlgoErrorMessage_t algoErrMessage;
				int32_t errorCode = (int32_t)AlgoErrorCode::ALGO_ERROR;

				errorCode = (int32_t)dispatchAlgoOrder(algoOrder, algoErrMessage);

				if (errorCode == (int32_t)AlgoErrorCode::ALGO_OK) {
					resp->set_error_code(errorCode);
					resp->add_success_algo_order_id(algoOrder->algoOrderId);
				}
				else {
					resp->add_failed_algo_order_id(algoOrder->algoOrderId);
					algoErrMessage_all = algoErrMessage_all + "|" + std::format("{}:{}", algoOrder->algoOrderId,algoErrMessage);
				}
			}

			if (resp->failed_algo_order_id().size() > 0) {

				resp->set_error_msg(fmt::format("[{}]success:{},failed:{}.{}", requestId, resp->success_algo_order_id().size(), resp->failed_algo_order_id().size(), algoErrMessage_all));
			}
			else {
				resp->set_error_msg("");
			}
		}
		else {
			resp->set_error_msg(algoErrMessage_all);
		}

		SPDLOG_INFO("[{}]create algoOrders SIZE:{}.{}", requestId, algoOrders.size(), algoErrMessage_all);
	}

	TCPSessionManager::getInstance().sendResp2C(session, AlgoMsg::MsgAlgoCMD::CMD_AlgoInstanceCreateResponse, resp);

}

std::vector<std::shared_ptr<AlgoOrder>> AlgoService::createAlgoOrder(const AlgoMsg::MsgAlgoInstanceCreateRequest& req
	, AlgoErrorMessage_t& algoErrorMessage) {

	static std::set<std::string> indexCodes{"hs300","zz500","zz1000","other"};

	std::vector<std::shared_ptr<AlgoOrder>> algoOrders;

	std::unordered_set<Symbol_t> symbols;

	auto startTime = agcommon::parseDateTimeInteger(req.algo_order().start_time());
	auto endTime   = agcommon::parseDateTimeInteger(req.algo_order().end_time());

	if (not startTime or not endTime) {
		algoErrorMessage = fmt::format("invalid date format:{},{}", req.algo_order().start_time(), req.algo_order().end_time());
		return algoOrders;
	}

	uint32_t startDate = req.algo_order().start_time()/1000000;
	uint32_t endDate   = req.algo_order().end_time() / 1000000;

	auto tradeDates = StockDataManager::getInstance().getTradeDateInts(startDate, endDate);

	if (tradeDates.empty()) {
		tradeDates.insert(endDate);
		//algoErrorMessage = fmt::format("invalid trading dates:{},{}", startDate, endDate);
		//return algoOrders;
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

	for (auto& tradeDate : tradeDates) {

		uint64_t startTime = (uint64_t)tradeDate * 1000000 + req.algo_order().start_time() % 1000000;
		uint64_t endTime =   (uint64_t)tradeDate * 1000000 + req.algo_order().end_time() % 1000000;

		SPDLOG_DEBUG(" {},{},{}", tradeDate, startTime, endTime);

		auto& symbol_req = req.algo_order().symbol();
		if (indexCodes.contains(symbol_req) and algoCategory == AlgoMsg::MsgAlgoCategory::Category_ALGO) {

			symbols = StockDataManager::getInstance().getIndexConstituents(startDate, symbol_req);
		}

		else {
			symbols.insert(symbol_req);
		}

		for (auto& symbol : symbols) {

			auto algoOrder = std::make_shared<AlgoOrder>();

			algoOrder->setAlgoOrderId(AlgoOrderIdGenerator::getInstance().NewId()); // IdGenerator<IdType::AlgoOrderID>
			algoOrder->setAcct(req.acct());
			algoOrder->setAcctType(req.acct_type());
			auto broker = agcommon::Configs::getConfigs().getBrokerIdFromAcct(req.acct_type(), req.acct());
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

			if (auto it = algoOrder->paramKeyconfig.find("notBuyOnLLOrSellOnHL");it != algoOrder->paramKeyconfig.end()) {
				auto v = agcommon::toLower(it->second) == "true" ? true : false;
				algoOrder->setNotBuyOnLLOrSellOnHL(v);
			}

			if (auto it = algoOrder->paramKeyconfig.find("notPegOrderAtLimitPrice");it != algoOrder->paramKeyconfig.end()) {
				auto v = agcommon::toLower(it->second) == "true" ? true : false;
				algoOrder->setNotPegOrderAtLimitPrice(v);
			}
			algoOrders.push_back(algoOrder);
		}
	}

	return algoOrders;
}

AlgoErrorCode AlgoService::dispatchAlgoOrder(const std::shared_ptr<AlgoOrder>& algoOrder
				,AlgoErrorMessage_t& algoErrMessage) {

	AlgoErrorCode algoErrorCode= AlgoErrorCode::ALGO_OK;

	auto clientAlgoOrderIdKey = std::make_pair(algoOrder->getAcctKey(),algoOrder->clientAlgoOrderId);
	
	//if (algoOrder->clientAlgoOrderId.empty() || clientAlgoOrderId2AlgoOrder.find(clientAlgoOrderIdKey) != clientAlgoOrderId2AlgoOrder.end()) {
	//	algoErrMessage = "clientAlgoOrderId {} is empty or already exists." + algoOrder->clientAlgoOrderId;
	//	SPDLOG_ERROR(algoErrMessage);
	//	return AlgoErrorCode::ALGO_DUPLICATE_ERROR;
	//}

	if (algoOrderId2AlgoOrder.find(algoOrder->algoOrderId) != algoOrderId2AlgoOrder.end()) { // unlikely happend..

		algoErrMessage = "algoOrderId already exists," + algoOrder->algoOrderId;
		SPDLOG_ERROR(algoErrMessage);
		algoErrorCode =  AlgoErrorCode::ALGO_DUPLICATE_ERROR;
	}

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
		algoErrMessage = fmt::format("{},unsupport algo_category:{}.", algoOrder->clientAlgoOrderId,AlgoMsg::MsgAlgoCategory_Name(algoOrder->algoCategory));
		SPDLOG_ERROR(algoErrMessage);
		return AlgoErrorCode::ALGO_ERROR;
	}

	if (algoErrorCode == AlgoErrorCode::ALGO_OK and algoErrMessage =="") {

		m_totalTraders.fetch_add(1);

		if (algoOrder->isBackTestOrder())
		{
			auto shouldQueue = m_runningTraders_bt_costMutiContext.load() !=0 or m_runningTraders_bt.load() >= m_numAlgoWorkers;

			if (shouldQueue) {

				m_algoTraderQueue_bt.push_back(traderPtr);

				traderPtr->publishAlgoperformance();

				SPDLOG_INFO("[{}]algoTraderQueue SIZE:{}", AlgoMsg::MsgAlgoCategory_Name(algoOrder->algoCategory), m_algoTraderQueue_bt.size());
			}
			else {

				m_runningTraders.fetch_add(1);

				m_runningTraders_bt.fetch_add(1);

				if (traderPtr->getAlgoCategory() != AlgoMsg::Category_ALGO) {
					m_runningTraders_bt_costMutiContext.fetch_add(1);
				}
				
				asio::co_spawn(*traderPtr->contextPtr, traderPtr->start(), std::bind(&AlgoService::onAlgoInstanceFinished, this, traderPtr, std::placeholders::_1));
			}
		}
		else 
		{
			m_runningTraders.fetch_add(1);

			asio::co_spawn(*traderPtr->contextPtr, traderPtr->start(), std::bind(&AlgoService::onAlgoInstanceFinished, this, traderPtr, std::placeholders::_1));
		}

		algoErrorCode = AlgoErrorCode::ALGO_OK;

	}
	else {

		asio::post(*traderPtr->contextPtr, [traderPtr, algoErrMessage]() { traderPtr->stop(algoErrMessage); });

		SPDLOG_ERROR("[aId:{}]AlgoTrader::preStartCheck() failed:{}", algoOrder->algoOrderId,algoErrMessage);

		algoErrorCode = AlgoErrorCode::ALGO_ORDERCHECK_FAILED;
	}

	if (algoErrorCode != AlgoErrorCode::ALGO_DUPLICATE_ERROR) {

		algoOrderId2AlgoOrder.emplace_hint(algoOrderId2AlgoOrder.end(),algoOrder->algoOrderId, algoOrder);

		algoOrderId2AlgoTrader.emplace_hint(algoOrderId2AlgoTrader.end(), algoOrder->algoOrderId, traderPtr);

	}

	return algoErrorCode;
}

void AlgoService::onAlgoInstanceFinished(const std::shared_ptr<Trader> traderPtr,const std::exception_ptr ex) {

	auto algoOrderId  = traderPtr->getAlgoOrderId();

	auto algoOrderPtr = traderPtr->getAlgoOrder();

	auto algoCategory = traderPtr->getAlgoCategory();

	m_runningTraders.fetch_sub(1);

	if (!ex) {
		SPDLOG_INFO( "[aId:{}]co_spawn done  ,totoal:{},running:{}", algoOrderId,m_totalTraders.load(), m_runningTraders.load());
	}
	else {

		SPDLOG_ERROR("[aId:{}]co_spawn failed,totoal:{},running:{}", algoOrderId, m_totalTraders.load(), m_runningTraders.load());
		try {

			std::rethrow_exception(ex);
		} catch (const std::exception& e) {
			SPDLOG_ERROR("[aId:{}]exception:{}", algoOrderId, e.what());
		}
	}

	if (algoOrderPtr->isBackTestOrder()) {

		m_runningTraders_bt.fetch_sub(1);

		if (traderPtr->getAlgoCategory() != AlgoMsg::Category_ALGO) {
			m_runningTraders_bt_costMutiContext.fetch_sub(1);
		}

		asio::post(*m_dispatcherContextPtr, [this, traderPtr, algoOrderId, algoOrderPtr]() {

			if (not m_algoTraderQueue_bt.empty()) {

				auto& new_trader = m_algoTraderQueue_bt.front();

				asio::co_spawn(*new_trader->contextPtr, new_trader->start(), std::bind(&AlgoService::onAlgoInstanceFinished, this, new_trader, std::placeholders::_1));
				
				m_algoTraderQueue_bt.pop_front();

				m_runningTraders_bt.fetch_add(1);

				m_runningTraders.fetch_add(1);

				if (traderPtr->getAlgoCategory() != AlgoMsg::Category_ALGO) {
					m_runningTraders_bt_costMutiContext.fetch_add(1);
				}

				SPDLOG_INFO("[REPLAY]running size:{},queue size:{}", m_runningTraders_bt.load(), m_algoTraderQueue_bt.size());
			}
		});
	}

}

void AlgoService::onUpdateAlgoInstanceRequest(const std::shared_ptr<TCPSession> session, const AlgoMsg::MessagePkg& recvPkgPtr) {

	AlgoMsg::MsgAlgoInstanceUpdateRequest req;

	auto parseOK = req.ParseFromArray(recvPkgPtr.body().data(), recvPkgPtr.body().length());

	if (not parseOK) {

		SPDLOG_ERROR("parse AlgoMsg::MsgAlgoInstanceCreateRequest failed");

		return;
	}

	auto resp = std::make_shared< AlgoMsg::MsgAlgoInstanceUpdateResponse>();

	resp->set_request_id(req.request_id());
	resp->set_acct_type(req.acct_type());
	resp->set_acct(req.acct());
	resp->set_algo_order_id(req.algo_order_id());

	auto brokerId = agcommon::Configs::getConfigs().getBrokerIdFromAcct(req.acct_type(), req.acct());
	auto acctKey = AcctKey_t(req.acct_type(), req.acct(), brokerId);

	AlgoErrorMessage_t algoErrMessage;

	if (TCPSessionManager::getInstance().isLogin(acctKey) == false) {

		SPDLOG_ERROR("[{}][{}]acct not login,{}", req.request_id(), req.algo_order_id(), acctKey);

		resp->set_error_code((int32_t)AlgoErrorCode::ALGO_ACCT_NOT_LOGIN);

		resp->set_error_msg(fmt::format("{} acct not login.", acctKey));
	}

	else {
		auto algoInstanceId = req.algo_order_id();

		auto action = req.action();

		if (action == AlgoMsg::MsgAlgoAction::ACTION_STOP) {

			auto it = algoOrderId2AlgoTrader.find(algoInstanceId);

			if (it == algoOrderId2AlgoTrader.end()) {

				resp->set_error_code((int32_t)AlgoErrorCode::ALGO_NOT_EXISTS);
				algoErrMessage = "algoOrderId not exists:" + algoInstanceId;
				resp->set_error_msg(algoErrMessage);
				SPDLOG_ERROR(algoErrMessage);
			}
			else {

				auto& trader = it->second;

				auto algoOrder = trader->getAlgoOrder();

				if (algoOrder->acct != req.acct() or algoOrder->acctType != req.acct_type()) {

					resp->set_error_code((int32_t)AlgoErrorCode::ALGO_ERROR);
					algoErrMessage = fmt::format("algoOrderId:{} acct/acctType NOT Match:{},{}", algoInstanceId, req.acct(), req.acct_type());
					resp->set_error_msg(algoErrMessage);
					SPDLOG_ERROR(algoErrMessage);
				}
				else {
					if (trader->isStopped()) {

						resp->set_error_code((int32_t)AlgoErrorCode::ALGO_ERROR);
						algoErrMessage = fmt::format("algoOrderId:{} already stopped", algoInstanceId);
						resp->set_error_msg(algoErrMessage);
						SPDLOG_ERROR(algoErrMessage);
					}

					else {

						resp->set_error_code((int32_t)AlgoErrorCode::ALGO_OK);
						asio::post(*trader->contextPtr, [trader]() {trader->cancel(); });
					}
				}
			}
		}
		else {

			resp->set_error_code((int32_t)AlgoErrorCode::ALGO_ERROR);
			algoErrMessage = fmt::format("algoOrderId{} not support Update Action:{}", algoInstanceId, AlgoMsg::MsgAlgoAction_Name(action));
			resp->set_error_msg(algoErrMessage);
			SPDLOG_ERROR(algoErrMessage);
		}
		//resp.PrintDebugString();
	}

	TCPSessionManager::getInstance().sendResp2C(session, AlgoMsg::MsgAlgoCMD::CMD_AlgoInstanceUpdateResponse, resp);
}

AlgoErrorCode AlgoService::cancelAlgoOrder(const AlgoOrderId_t algoOrderId, AlgoErrorMessage_t& algoErrorMessage) {

		auto algoInstanceId = algoOrderId;

		auto it = algoOrderId2AlgoTrader.find(algoInstanceId);

		if (it == algoOrderId2AlgoTrader.end()) {

			algoErrorMessage = fmt::format("algoOrderId:{} algoOrderId not exists", algoInstanceId);
			SPDLOG_INFO(algoErrorMessage);
			return AlgoErrorCode::ALGO_NOT_EXISTS;
		}
		else {

			auto& trader = it->second;

			auto algoOrder = trader->getAlgoOrder();

			if (trader->isStopped()) {

				algoErrorMessage = fmt::format("algoOrderId:{} already stopped", algoInstanceId);
				SPDLOG_INFO(algoErrorMessage);
				return AlgoErrorCode::ALGO_ERROR;
			}

			else {

				asio::post(*trader->contextPtr, [trader]() {trader->cancel(); });
				return AlgoErrorCode::ALGO_OK;
			}
		}
}
