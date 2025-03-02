#include "ClientApiImpl.h"

int ClientApiImpl::init(const std::string& ipStr, const int port, bool autoReConnet, int reConnectIntervalSecs) {

	if (not isInited) {
		SPDLOG_INFO("ClientApiImpl::init...");

		m_sendContext = std::make_shared<SendContext>();

		m_sendContext->connetSetting(ipStr, port, autoReConnet, reConnectIntervalSecs);

		isInited = true;
	}

	return m_sendContext->connect()? 0:-1;
}

void ClientApiImpl::release() {

	if(m_sendContext)
		m_sendContext->stop();
}
int ClientApiImpl::checkStatus() const {
	if (not isInited) {
		SPDLOG_ERROR("Client not inited yet.");
		return -1;
	}
	if (not m_sendContext->isConnected()) {
		SPDLOG_ERROR("not connected yet.check server is on serives...");
		return -1;
	}
	return 0;
}
void  ClientApiImpl::registerSpi(ClientSpi* clientSpi) {

	m_clientSpi = clientSpi;

	if (m_sendContext)
		m_sendContext->setClientApi(clientSpi);
	else
		throw std::runtime_error("api not init yet!");
}

int ClientApiImpl::login(const AlgoMsg::MsgLoginRequest& request){

	if (auto ret = checkStatus();ret != 0) {
		return ret;
	}
	return m_sendContext->send(AlgoMsg::CMD_LoginRequest, &request, request.request_id());
}
int ClientApiImpl::createAlgoInstance(const AlgoMsg::MsgAlgoInstanceCreateRequest& request)  {
	if (auto ret = checkStatus(); ret != 0) {
		return ret;
	}
	return m_sendContext->send(AlgoMsg::CMD_AlgoInstanceCreateRequest, &request, request.request_id());
}

int ClientApiImpl::updateAlgoInstance(const AlgoMsg::MsgAlgoInstanceUpdateRequest& request)  {
	if (auto ret = checkStatus(); ret != 0) {
		return ret;
	}
	return m_sendContext->send(AlgoMsg::CMD_AlgoInstanceUpdateRequest, &request, request.request_id());
}

int ClientApiImpl::queryAlgoInstance(const AlgoMsg::MsgAlgoInstanceQueryRequest& request) {
	if (auto ret = checkStatus(); ret != 0) {
		return ret;
	}
	return m_sendContext->send(AlgoMsg::CMD_AlgoInstanceQueryRequest, &request, request.request_id());
}

int ClientApiImpl::queryOrder(const AlgoMsg::MsgOrderQueryRequest& request) {
	if (auto ret = checkStatus(); ret != 0) {
		return ret;
	}
	return m_sendContext->send(AlgoMsg::CMD_OrderQueryRequest, &request, request.request_id());
	return 0;
}

int ClientApiImpl::queryTrade(const AlgoMsg::MsgTradeQueryRequest& request) {
	if (auto ret = checkStatus(); ret != 0) {
		return ret;
	}
	return m_sendContext->send(AlgoMsg::CMD_TradeQueryRequest, &request, request.request_id());
}