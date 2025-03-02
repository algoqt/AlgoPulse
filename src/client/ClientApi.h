#pragma once

#include "AlgoMessages.pb.h"

#ifdef _WIN32
#ifdef ALGOCLIENT_EXPORTS
#define ALGOCLIENT_API __declspec(dllexport)
#else
#define ALGOCLIENT_API __declspec(dllimport)
#endif
#else
#define ALGOCLIENT_API
#endif

class ClientSpi;

class ClientApi {
public:

    static ClientApi* createClientApi();

    virtual int init(const std::string& ipStr, const int port, bool autoReConnet, int reConnectIntervalSecs) = 0;

    virtual void registerSpi(ClientSpi* clientSpi) = 0;

    virtual int login(const AlgoMsg::MsgLoginRequest& request) = 0;

    virtual int createAlgoInstance(const AlgoMsg::MsgAlgoInstanceCreateRequest& request) = 0;

    virtual int updateAlgoInstance(const AlgoMsg::MsgAlgoInstanceUpdateRequest& request) = 0;

    virtual int queryAlgoInstance(const AlgoMsg::MsgAlgoInstanceQueryRequest& request) = 0;

    virtual int queryOrder(const AlgoMsg::MsgOrderQueryRequest& request) = 0;

    virtual int queryTrade(const AlgoMsg::MsgTradeQueryRequest& request) = 0;

    virtual void release() = 0;

};

class ClientSpi {
public:
    virtual ~ClientSpi() {};

    virtual void onDisconnected() {};
    virtual void onReConnected() {};
    // 响应
    virtual void onLogin(const AlgoMsg::MsgLoginResponse& login_rsp) {};

    virtual void onCreateAlgoInstance(const AlgoMsg::MsgAlgoInstanceCreateResponse& msgAlgoInstanceCreateResponse) {};

    virtual void onUpdateAlgoInstance(const AlgoMsg::MsgAlgoInstanceUpdateResponse& msgAlgoInstanceUpdateResponse) {};

    // 执行信息
    virtual void onQueryAlgoInstance(const AlgoMsg::MsgAlgoInstanceQueryResponse& msgAlgoInstanceQueryResponse) {};

    virtual void onQueryOrder(const AlgoMsg::MsgOrderQueryResponse& msgOrderQueryResponse) {};

    virtual void onQueryTrade(const AlgoMsg::MsgTradeQueryResponse& MsgOrderTradeQueryResponse) {};

    // 推送
    virtual void onNotifyAlgoInstanceExecutionInfo(const AlgoMsg::MsgAlgoPerformance& msgAlgoPerformance) {};

    virtual void OnNotifyOrderInfo(const AlgoMsg::MsgOrderInfo& msgOrderInfo) {};

    virtual void OnNotifyTradeInfo(const AlgoMsg::MsgTradeInfo& msgTradeInfo) {};

private:
    //std::map<AlgoMsg::MsgAlgoCMD, std::function<void(const google::protobuf::Message* message)>> messageHandleMap;
};
