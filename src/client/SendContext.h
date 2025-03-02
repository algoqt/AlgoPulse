#pragma once

#include <boost/asio.hpp>
#include "AlgoMessages.pb.h"
#include <queue>
#include "ClientApi.h"
#include "IdGenerator.h"
#include <vector>
#include <spdlog/spdlog.h>

namespace asio = boost::asio;

typedef std::multimap<uint64_t, AlgoMsg::MessagePkg*>       SendingTimeoutMap;

typedef std::unordered_map<uint64_t, std::pair<AlgoMsg::MessagePkg*, SendingTimeoutMap::iterator>>  SendingMap;

class SendContext:public std::enable_shared_from_this<SendContext> {

public:
    SendContext();

    void setClientApi(ClientSpi* clientSpi);

    void connetSetting(const std::string& ipStr, const int port,bool autoReConnet,int reConnectIntervalSecs);

    bool connect();

    inline bool isConnected() const { return m_isConneted; }

    void start();

    void stop();

    int send(const AlgoMsg::MsgAlgoCMD cmd, const google::protobuf::Message* message, const uint64_t request_id = 0);

private:

    std::shared_ptr<asio::io_context> io_context;

    std::shared_ptr<asio::io_context> process_context;

    ClientSpi*                  m_clientSpi;

    std::vector<std::thread>    m_threads{};

    asio::ip::tcp::socket       m_socket;

    std::string                 m_ipStr{};

    int                         m_port{ 0 };

    bool                        m_autoReConnet{ false };

    int                         m_reConnectIntervalSecs{ 10 };

    bool                        m_isConneted{ false };

    std::atomic<bool>           m_keepRunning{ false };

    const int HEADER_LENGTH_BODY = 4;

    std::vector<unsigned char> m_sendingBuffer ;

    std::vector<unsigned char> m_receivingBuffer;

    void onMessageHandle(const AlgoMsg::MessagePkg* recvPkg);

    void printForDebug(const google::protobuf::Message*);

    static size_t encodePkg(unsigned char* sendingBuf
        , const AlgoMsg::MsgAlgoCMD cmd
        , const AlgoMsg::MsgDirection direction
        , const google::protobuf::Message* message
        , AlgoMsg::MessagePkg* sendPkg);

    static size_t encodeHeader(unsigned char* buf, uint32_t body_length);
    void          _send();

    inline size_t decode_header() {

        return ntohl(*(uint32_t*)m_receivingBuffer.data());

    };
    void do_read_pkgHeader();

    void do_read_pkgBody(std::size_t body_length);

    void asyncReConnect();

    std::queue<AlgoMsg::MessagePkg*> sendingMessageQueue{};

    std::unordered_map<uint64_t, std::pair<AlgoMsg::MessagePkg*, SendingTimeoutMap::iterator>>    sendingMessageMap{};

    SendingTimeoutMap                timeoutMessageMap{};

    std::queue<AlgoMsg::MessagePkg*> receivedMessageQ{};
};