#pragma once

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <map>
#include "typedefs.h"
#include "AlgoMessages.pb.h"

namespace asio = boost::asio;

class MarketDepth;

using SocketId_t = uint32_t;

typedef std::map<uint64_t, std::shared_ptr<AlgoMsg::MessagePkg>>	MessageContainer;

class TCPSessionManager;

class TCPSession: public std::enable_shared_from_this<TCPSession>
{
    friend class TCPSessionManager;

    using AlgoMessagePkgHandle_t = std::function<void(const std::shared_ptr<TCPSession>, std::unique_ptr<AlgoMsg::MessagePkg>&&)>;

public:

    void start(const AlgoMessagePkgHandle_t& handler);

    void do_read_header();

    void do_read_body(size_t body_length);

    inline size_t decode_header() const {   return ntohl(*(uint32_t*)m_receivingBuffer.data());    }

    static size_t encodeHeader(unsigned char* buf, size_t body_length);

    void queueSendMessage2C(const std::shared_ptr<AlgoMsg::MessagePkg>&);

    asio::ip::tcp::socket& socket();

public:
    explicit TCPSession(std::shared_ptr<asio::io_context> io_context,const SocketId_t socketId);

    ~TCPSession() = default;

private:

    std::shared_ptr<asio::io_context> m_io_context;

    asio::io_context::strand          m_strand;

    asio::ip::tcp::socket             m_socket;

    SocketId_t                        m_socketId = 0;

    AlgoMessagePkgHandle_t            onMessageHandler = nullptr;

    std::vector<unsigned char>        m_receivingBuffer;

    std::vector<unsigned char>        m_sendingBuffer;

    std::deque<std::shared_ptr<AlgoMsg::MessagePkg>>  m_sendingMessageQueue{};

    std::set<AcctKey_t>               m_loginAcctKeys{};

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

    std::shared_ptr<TCPSession> createTCPSession(std::shared_ptr<asio::io_context>& io_context);

    void closeAllSessions();

    bool login(const AcctKey_t& acctKey, const AlgoMsg::MsgLoginRequest& request, std::shared_ptr<TCPSession> session);

    void reSend(const std::shared_ptr<TCPSession>& session, const AcctKey_t& acctKey,const AlgoMsg::MsgLoginRequest& request);

    bool addLoginTCPSession(const AcctKey_t& acctKey, std::shared_ptr<TCPSession>& session);

    bool removeTCPSession(const std::shared_ptr<TCPSession>& session);

    bool isLogin(const AcctKey_t& acctKey);

    bool isLogin(const std::shared_ptr<TCPSession>& session);

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

    void timerJob();

    void printStatInfo();

    std::string addMarketDepthSubscribe(const std::shared_ptr<TCPSession>& session,const AlgoMsg::MsgMarketDepthSubcribeRequest* req);

    std::string addStockConceptQuoteSubscribe(const std::shared_ptr<TCPSession>& session,const AlgoMsg::MsgSubscribeStockConceptQuoteRequest* req);

    void pushMarketDepth(MarketDepth* md);

    void pushStockConceptQuotes();

    void pushStockConceptInfos(const std::shared_ptr<TCPSession>& session);

private:

    TCPSessionManager();

    ~TCPSessionManager();

    std::shared_ptr<asio::io_context>   m_contextPtr;
    asio::io_context::strand            m_strand;
    asio::steady_timer                  m_timer;
    std::mutex                          m_mutex;
    std::atomic<uint64_t>               m_sentMessageCnt{0};
    std::atomic<SocketId_t>             m_autoId{ 0 };

    using SessionMap = std::unordered_map<SocketId_t, std::shared_ptr<TCPSession>>;

    SessionMap                          m_tcpSessions{};

    std::unordered_map<AcctKey_t
        , SessionMap
        , AcctKey_t::Hash>              m_acctKey2TCPSessions{};

    // message cache. retransmission from begin or last ackn message
    MessageContainer                    m_messageId2Pkg{};

    std::unordered_map<AcctKey_t
        , std::map<uint64_t,MessageContainer::iterator>
        , AcctKey_t::Hash>                                          m_acct2MessageIter{};

    std::unordered_map<AcctKey_t, uint64_t,AcctKey_t::Hash>         m_acctSentAcknMaxMessageId{};

    // session subscribe info
    //std::unordered_map<SocketId_t, std::unordered_set<Symbol_t>>    m_session2MdSubsribe{};

    std::unordered_map<Symbol_t, std::unordered_set<SocketId_t>>    m_subscribeSymbolMd{};

    std::unordered_set<SocketId_t>                                  m_subsribeAllMd{};

    std::unordered_set<SocketId_t>                                  m_subsribeConceptQuote{};


private:

    void send(const std::shared_ptr<TCPSession>& session
        , const AcctKey_t& acctKey
        , const AlgoMsg::MsgDirection direction
        , const AlgoMsg::MsgAlgoCMD cmd
        , std::shared_ptr<google::protobuf::Message> messageBody
        , bool shouldCache = true
    );
};