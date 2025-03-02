#pragma once

#include "typedefs.h"
#include "common.h"
#include <map>

namespace asio = boost::asio;

class Trader;
class AlgoOrder;
class TCPSession;

class AlgoService {

public:

	static AlgoService& getInstance() {
		static AlgoService instance{};
		return instance;
	}

	AlgoErrorCode dispatchAlgoOrder(const std::shared_ptr<AlgoOrder>& algoOrder
		, AlgoErrorMessage_t& algoErrMessage);

	std::unordered_map<AlgoOrderId_t, std::shared_ptr<Trader>>   algoOrderId2AlgoTrader{};

public:
	
	~AlgoService();

	void start();

	void stop();
	
private:

	AlgoService();

	// net
	void accept();

	void listen();

	void handleAccept(std::shared_ptr<TCPSession> new_connection,const boost::system::error_code& error);

	// request handle
	void dispatchAlgoRequest(const std::shared_ptr<TCPSession> connection,const AlgoMsg::MessagePkg& recvPkgPtr);

	void onLoginRequest(const std::shared_ptr<TCPSession> connection, const AlgoMsg::MessagePkg& recvPkgPtr);

	void onStartAlgoInstanceRequest(const std::shared_ptr<TCPSession> connection, const AlgoMsg::MessagePkg& recvPkgPtr);

	void onUpdateAlgoInstanceRequest(const std::shared_ptr<TCPSession> connection, const AlgoMsg::MessagePkg& recvPkgPtr);

	void onAlgoInstanceFinished(const std::shared_ptr<Trader> trader_ptr,const std::exception_ptr ex);

public:

	std::atomic<bool>	  m_isRunning	  { false };

	std::atomic<uint32_t>  m_connectSeq{ 0 };

	std::atomic<uint64_t>  m_totalAlgoRequests{ 0 };

	std::atomic<uint64_t>  m_totalTraders{ 0 };

	std::atomic<uint64_t>  m_runningTraders{ 0 };

	std::string   m_ipstr;

	std::uint32_t m_port;

	// net
	std::shared_ptr<asio::io_context>				m_netAcceptcontextPtr{};
	std::vector<std::shared_ptr<asio::io_context>>  m_netContexts{};
	std::shared_ptr<asio::ip::tcp::acceptor>		m_acceptor{nullptr};

	// worker
	std::shared_ptr<asio::io_context>				m_dispatcherContextPtr{};
	std::vector<std::shared_ptr<asio::io_context>>  m_workerContexts{};
	uint32_t										m_numAlgoWorkers{ 0 };

	// all context
	std::vector<std::string>						m_contextKeys{};
	std::vector<std::string>						m_workercontextKeys{};

	// algo data
	std::unordered_map<AlgoOrderId_t, std::shared_ptr<AlgoOrder>>	algoOrderId2AlgoOrder{};
	//std::map<ClientAlgoOrderKey_t, AlgoOrder>						clientAlgoOrderId2AlgoOrder{};

	std::vector<std::shared_ptr<AlgoOrder>> createAlgoOrder(const AlgoMsg::MsgAlgoInstanceCreateRequest& req
													, AlgoErrorMessage_t& algoErrorMessage);

	AlgoErrorCode cancelAlgoOrder(const AlgoOrderId_t algoOrderId, AlgoErrorMessage_t& algoErrorMessage);


	// only for bt request
	std::deque<std::shared_ptr<Trader>>	m_algoTraderQueue_bt{}; // 回测请求队列
	std::atomic<uint32_t>				m_runningTraders_bt;
	std::atomic<uint32_t>				m_runningTraders_bt_costMutiContext;  // 
};

