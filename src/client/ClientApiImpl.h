#pragma once

#include "AlgoMessages.pb.h"
#include "ClientApi.h"
#include "SendContext.h"

class ClientApiImpl :public ClientApi {
public:

    int init(const std::string& ipStr, const int port, bool autoReConnet, int reConnectIntervalSecs) override;
    
    void registerSpi(ClientSpi* clientSpi) override;

    int login(const AlgoMsg::MsgLoginRequest& request) override;

    int createAlgoInstance(const AlgoMsg::MsgAlgoInstanceCreateRequest& request) override;

    int updateAlgoInstance(const AlgoMsg::MsgAlgoInstanceUpdateRequest& request) override;

    int queryAlgoInstance(const AlgoMsg::MsgAlgoInstanceQueryRequest& request) override;

    int queryOrder(const AlgoMsg::MsgOrderQueryRequest& request) override;

    int queryTrade(const AlgoMsg::MsgTradeQueryRequest& request) override;

    void release() override;

private:

	ClientSpi* m_clientSpi = nullptr;

    std::shared_ptr<SendContext> m_sendContext = nullptr;

    bool isInited = false;

    int checkStatus() const;
};