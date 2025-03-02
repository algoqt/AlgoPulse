#pragma once

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <map>
#include "typedefs.h"
#include "AlgoMessages.pb.h"

namespace asio = boost::asio;

typedef asio::ip::tcp::socket::native_handle_type sockect_handel_t;

typedef std::map<uint64_t, std::shared_ptr<AlgoMsg::MessagePkg>>	MessageContainer;

class TCPSessionManager;

class TCPSession: public std::enable_shared_from_this<TCPSession>
{
    friend class TCPSessionManager;

    using AlgoMessagePkgHandle_t = std::function<void(const std::shared_ptr<TCPSession>, const AlgoMsg::MessagePkg&)>;

public:

    void start(const AlgoMessagePkgHandle_t& handler);

    void do_read_header();

    void do_read_body(size_t body_length);

    inline size_t decode_header() const {   return ntohl(*(uint32_t*)m_receivingBuffer.data());    }

    static size_t encodeHeader(unsigned char* buf, size_t body_length);

    void queueSendMessage2C(const std::shared_ptr<AlgoMsg::MessagePkg>&);

    asio::ip::tcp::socket& socket();


public:
    explicit TCPSession(std::shared_ptr<asio::io_context> io_context);

    ~TCPSession() = default;

private:

    std::shared_ptr<asio::io_context> m_io_context;

    asio::io_context::strand          m_strand;

    asio::ip::tcp::socket             m_socket;

    sockect_handel_t                  m_socketHandle = 0;

    AlgoMessagePkgHandle_t            onMessageHandler = nullptr;

    std::vector<unsigned char>        m_receivingBuffer;

    std::vector<unsigned char>        m_sendingBuffer;

    std::deque<std::shared_ptr<AlgoMsg::MessagePkg>>  sendingMessageQueue{};

    std::set<AcctKey_t>               loginAcctKeys{};

    void _send();

    void safeDisConnect();

};

////////////////////////////////////////////////////////////////////

class TCPSessionManager {

public:
    static TCPSessionManager& getInstance() {

        static TCPSessionManager instance{};
        return instance;
    }

    std::shared_ptr<TCPSession> createTCPSession(std::shared_ptr<asio::io_context> io_context);

    void closeAllSessions();

    bool login(const AcctKey_t& acctKey, const AlgoMsg::MsgLoginRequest& request, std::shared_ptr<TCPSession> session);

    void reSend(const std::shared_ptr<TCPSession>& session, const AcctKey_t& acctKey,const AlgoMsg::MsgLoginRequest& request);

    bool addTCPSession(std::shared_ptr<TCPSession>& session);

    bool removeTCPSession(const std::shared_ptr<TCPSession>& session);

    bool isLogin(const AcctKey_t& acctKey);

    void sendResp2C(const std::shared_ptr<TCPSession>& session
        , const AlgoMsg::MsgAlgoCMD cmd
        , std::shared_ptr<google::protobuf::Message> messageBody
    );

    void sendNotify2C(const AcctKey_t& acctKey
        , const AlgoMsg::MsgAlgoCMD cmd
        , std::shared_ptr<google::protobuf::Message> messageBody
        , bool shouldCache
    );

    void sendMessagesOffset(const std::shared_ptr<AlgoMsg::MessagePkg>&);

    void printStatInfo();

private:

    TCPSessionManager();

    ~TCPSessionManager();

    std::shared_ptr<asio::io_context>   m_contextPtr;
    asio::io_context::strand            m_strand;
    asio::steady_timer                  m_timer;
    std::mutex                          m_mutex;
    std::atomic<uint64_t>               m_sentMessageCnt{0};
    bool                                m_cacheAllMessage{ false };

    std::map<sockect_handel_t, std::shared_ptr<TCPSession>>       TCPSessions{};

    std::unordered_map<AcctKey_t
        , std::map<sockect_handel_t,std::shared_ptr<TCPSession>>
        , AcctKey_t::Hash>                                         acctKey2TCPSessions{};

    // message cache. retransmission from begin or last ackn message
    MessageContainer                    messageId2Pkg{};

    std::unordered_map<AcctKey_t, std::map<uint64_t,MessageContainer::iterator>
        , AcctKey_t::Hash>                                          acct2MessageIter{};

    std::unordered_map<AcctKey_t, uint64_t,AcctKey_t::Hash>         acctSentAcknMaxMessageId{};

    void send(const std::shared_ptr<TCPSession>& session
        , const AcctKey_t& acctKey
        , const AlgoMsg::MsgDirection direction
        , const AlgoMsg::MsgAlgoCMD cmd
        , std::shared_ptr<google::protobuf::Message> messageBody
        , bool shouldCache = true
    );
};