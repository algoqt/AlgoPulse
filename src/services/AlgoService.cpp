#include "AlgoOrder.h"
#include "AlgoTrader.h"
#include "ShotTrader.h"
#include "AlgoService.h"
#include "IdGenerator.h"
#include "ContextService.h"

#include "TCPSession.h"
#include "Configs.h"
#include "StorageService.h"
#include "Task.h"

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
		new_session->start(std::bind(&AlgoService::dispatchAlgoMessage, this, std::placeholders::_1, std::placeholders::_2));
		m_connectSeq++;
	} else{
		SPDLOG_ERROR("AlgoService accept error:{}", error.message());
	}

	listen();

}

void AlgoService::dispatchAlgoMessage(const std::shared_ptr<TCPSession>& session,std::unique_ptr<AlgoMsg::MessagePkg>&& recvPkgPtr) {

	m_totalAlgoRequests++;

	auto cmd = recvPkgPtr->head().msg_cmd();
	SPDLOG_INFO("dispatchAlgoRequest msg_seq:{},cmd:{},totalRequests:{}", recvPkgPtr->head().msg_seq_id(), (uint32_t)cmd,m_totalAlgoRequests.load());

	switch (cmd)
	{

	default:

		asio::post(*m_dispatcherContextPtr, [this, cmd, session, pkg = std::move(recvPkgPtr)]() mutable {

			auto task = Task::getTask(cmd);

			if (task) {
				task->handleMessage(session, cmd, std::move(pkg));
			}
			else {
				SPDLOG_INFO("not match AlgoMsgCMD:{}.", (uint32_t)cmd);
			}
		});

		break;
	}
}


