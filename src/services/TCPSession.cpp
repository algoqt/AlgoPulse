#include "TCPSession.h"
#include "StorageService.h"
#include "MarketDepth.h"
#include "Order.h"
#include "IdGenerator.h"
#include "ContextService.h"
#include "ConceptQuote.h"

TCPSession::TCPSession(std::shared_ptr<asio::io_context> io_context, const SocketId_t socketId)
          :m_io_context(io_context)
          ,m_socket(*io_context)
          ,m_strand(*io_context)
          ,m_socketId(socketId){

    m_receivingBuffer.reserve(1024);

    m_sendingBuffer.reserve(2048);
}

void TCPSession::start(const AlgoMessagePkgHandle_t& handler)
{
    assert(handler!=nullptr);

    onMessageHandler = handler;

    do_read_header();

    //m_socketHandle = m_socket.native_handle();

    auto ip = m_socket.remote_endpoint().address().to_string();

    auto port = m_socket.remote_endpoint().port();

    //m_socket.set_option(asio::ip::tcp::no_delay(true));

    SPDLOG_INFO("new TCPSession::started,socket:{},isOpen:{},remote_ep:{}:{}", m_socketId,m_socket.is_open(), ip,port);
}

asio::ip::tcp::socket& TCPSession::socket(){
    return m_socket;
}

void TCPSession::do_read_header(){

    asio::async_read(m_socket, asio::buffer(m_receivingBuffer.data(), 4),
        [self = shared_from_this()](boost::system::error_code ec, std::size_t bytes_transferred)
        {
            if (!ec)
            {
                auto body_length = self->decode_header();
                self->do_read_body(body_length);
                SPDLOG_DEBUG("-read body_length:{}", body_length);
            }
            else
            {   
                SPDLOG_INFO("do_read_header,socket {} with error: {}", self->m_socketId, ec.message());
                self->safeDisConnect();
            }
        });
}

void TCPSession::do_read_body(size_t body_length) {

    auto buffer_size = 4 + body_length;

    while (m_receivingBuffer.capacity() < buffer_size) {

        buffer_size = (buffer_size / 1024) * 1024 + 1024;

        SPDLOG_INFO("m_receivingBuffer capacity {},RESIZE TO :{} ", m_receivingBuffer.capacity(), buffer_size);
        m_receivingBuffer.clear();

        m_receivingBuffer.reserve(buffer_size);
    }

    asio::async_read(m_socket, asio::buffer(m_receivingBuffer.data() + 4, body_length),
        [self = shared_from_this(), body_length](boost::system::error_code ec, std::size_t bytes_transferred)
        {
            if (!ec)
            {
                SPDLOG_DEBUG("body_length:{},bytes_transferred:{}", body_length, bytes_transferred);

                auto recvPkgPtr = std::make_unique<AlgoMsg::MessagePkg>();

                recvPkgPtr->ParseFromArray(self->m_receivingBuffer.data() + 4, (int)body_length);

                SPDLOG_INFO("recv msg_cmd:{},msg_seq_id:{},body_length:{}", (uint32_t)recvPkgPtr->head().msg_cmd(), recvPkgPtr->head().msg_seq_id(),body_length);

                self->onMessageHandler(self, std::move(recvPkgPtr));

                self->do_read_header();
            }
            else
            {
                SPDLOG_INFO("do_read_body,socket {} with error:{}", self->m_socketId, ec.message());

                self->safeDisConnect();
            }
        });
}

void TCPSession::queueSendMessage2C(const std::shared_ptr<AlgoMsg::MessagePkg>& pkgPtr) {

    asio::post(m_strand, [self= shared_from_this(), pkgPtr]() {

        self->m_sendingMessageQueue.emplace_back( pkgPtr);

        if (self->m_sendingMessageQueue.size() > 1) {
            return;
        }
        self->_send();
        }
    );
}

void TCPSession::_send() {

    if (m_sendingMessageQueue.empty()) {
        return;
    }

    auto& sendPkgPtr = m_sendingMessageQueue.front();

    auto messageSeqId = sendPkgPtr->head().msg_seq_id();

    size_t pkgBodyByteSize = sendPkgPtr->ByteSizeLong();

    auto buffer_size = 4 + pkgBodyByteSize;

    while (m_sendingBuffer.capacity() < buffer_size) {

        buffer_size = (buffer_size / 1024) * 1024 + 1024;

        SPDLOG_INFO("m_sendingBuffer capacity {},RESIZE TO :{} ", m_sendingBuffer.capacity(), buffer_size);
        m_sendingBuffer.clear();

        m_sendingBuffer.reserve(buffer_size);
    }

    size_t headByteSize = encodeHeader(m_sendingBuffer.data(), pkgBodyByteSize);

    sendPkgPtr->SerializeToArray(m_sendingBuffer.data() + headByteSize, pkgBodyByteSize);
         
    size_t send_size = headByteSize + pkgBodyByteSize;
    if (m_socket.is_open()) {

        SPDLOG_DEBUG("send cmd {},messageid:{},current:{}", (uint32_t)sendPkgPtr->head().msg_cmd(), messageSeqId, sendingMessageQueue.size());

        asio::async_write(m_socket, asio::buffer(m_sendingBuffer.data(), send_size),

            [self = shared_from_this(), sendPkgPtr, messageSeqId](boost::system::error_code ec, std::size_t bytes_transferred) {

                auto cmd = sendPkgPtr->head().msg_cmd();

                if (!ec) {
                    TCPSessionManager::getInstance().sendMessagesOffset(sendPkgPtr);

                    self->m_sendingMessageQueue.pop_front();

                    SPDLOG_DEBUG("NotifyOK.cmd {},messageid:{},remain:{}", (uint32_t)cmd, messageSeqId, self->sendingMessageQueue.size());

                    if (not self->m_sendingMessageQueue.empty()) {

                        asio::post(self->m_strand, [self]() {self->_send(); });
                    }
                }
                else
                {
                    SPDLOG_INFO("send Failed.cmd {},messageid:{}", (uint32_t)cmd, messageSeqId);

                    SPDLOG_INFO("socket {} with error:{}", self->m_socketId, ec.message());

                    self->safeDisConnect();
                }
            });
    }
}
 
void TCPSession::safeDisConnect() {

    if (m_socket.is_open()) {

        boost::system::error_code ec;
        m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);

        if (ec) {

            SPDLOG_ERROR("socket {} Shutdown failed:{} ", m_socketId, ec.message());
        }

        m_socket.close(ec);

        if (ec) {

            SPDLOG_ERROR("socket{} Close failed:{} ", m_socketId, ec.message());
        }
    }

    TCPSessionManager::getInstance().removeTCPSession(shared_from_this());
}

size_t TCPSession::encodeHeader(unsigned char* buf, size_t body_length) {

    unsigned char* buf_pos = buf;

    *buf_pos++ = (char)((body_length & 0xFF000000) >> 24);
    *buf_pos++ = (char)((body_length & 0x00FF0000) >> 16);
    *buf_pos++ = (char)((body_length & 0x0000FF00) >> 8);
    *buf_pos++ = (char)(body_length & 0x000000FF);

    return 4;
}


///////////////////////////////////////////////////////////////////////////////////////


TCPSessionManager::TCPSessionManager():

    m_contextPtr(ContextService::getInstance().createContext("TCPSessionManager"))
    ,m_strand(*m_contextPtr)
    ,m_timer(*m_contextPtr)
{
	SPDLOG_INFO("TCPSessionManager::TCPSessionManager()");

    m_timer.expires_after(std::chrono::seconds(3));

    m_timer.async_wait([this](boost::system::error_code ec) {

        asio::post(m_strand, [this]() {

            timerJob();
		});
    });
}
TCPSessionManager::~TCPSessionManager() {

    SPDLOG_INFO("TCPSessionManager::~TCPSessionManager()");
}

std::shared_ptr<TCPSession> TCPSessionManager::createTCPSession(std::shared_ptr<asio::io_context>& io_context) {

    m_autoId.fetch_add(1);
    auto session = std::make_shared<TCPSession>(io_context, m_autoId.load());

    return session;
}

void TCPSessionManager::closeAllSessions() {

    ContextService::getInstance().stopContext("TCPSessionManager");
}

void TCPSessionManager::timerJob() {
    
    pushStockConceptQuotes();

    printStatInfo();

    m_timer.expires_after(std::chrono::seconds(3));

    m_timer.async_wait([this](boost::system::error_code ec) {

        asio::post(m_strand, [this]() {
            timerJob();
            });
        });
}

void TCPSessionManager::printStatInfo() {
    
    static auto statTime = agcommon::now();

    if (agcommon::getSecondsDiff(statTime, agcommon::now()) < 15) {
        return;
    }

    std::scoped_lock<std::mutex> lock(m_mutex);

    auto sessionCnt = m_tcpSessions.size();
    auto acctCnt = m_acctKey2TCPSessions.size();
    auto remainMessageCnt = m_messageId2Pkg.size();

    SPDLOG_INFO("[Stat]sessions:{},accts:{},remain messages:{},sent messages:{}"
        , sessionCnt
        , acctCnt
        , remainMessageCnt
        , m_sentMessageCnt.load());

    SPDLOG_INFO("MarketDepth alives:{},Order alives:{},Trade alives:{}"
        , MarketDepth::totalAliveObjectCount.load()
        , Order::totalAliveObjectCount.load()
        , Trade::totalAliveObjectCount.load());

    statTime = agcommon::now();
}

bool TCPSessionManager::isLogin(const AcctKey_t& acctKey) {

    std::scoped_lock lock(m_mutex);

    return m_acctKey2TCPSessions.find(acctKey) != m_acctKey2TCPSessions.end();
}

bool TCPSessionManager::isLogin(const std::shared_ptr<TCPSession>& session) {

    std::scoped_lock lock(m_mutex);

    return not session->m_loginAcctKeys.empty();
}

bool TCPSessionManager::login(const AcctKey_t& acctKey,const AlgoMsg::MsgLoginRequest& request, std::shared_ptr<TCPSession> session) {

    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        /*check user pw*/
    }

    if (addLoginTCPSession(acctKey, session)) {
        SPDLOG_INFO("acctKey:{} login in session:{},resendmessage:{}", acctKey, session->m_socketId, request.resendmessage());
    }

    return true;
}

bool TCPSessionManager::addLoginTCPSession(const AcctKey_t& acctKey, std::shared_ptr<TCPSession>& session) {

    std::scoped_lock<std::mutex> lock(m_mutex);


    auto it = m_tcpSessions.insert({ session->m_socketId, session });

    session->m_loginAcctKeys.insert(acctKey);

    auto [socketMap_it, inserted2] = m_acctKey2TCPSessions.try_emplace(acctKey,SessionMap{ {session->m_socketId, session} });

    if (not inserted2) {
        socketMap_it->second.insert({ session->m_socketId, session });
    }

    return true;
}

bool TCPSessionManager::removeTCPSession(const std::shared_ptr<TCPSession>& session) {

    std::scoped_lock<std::mutex> lock(m_mutex);

    auto& loginAcctKeys = session->m_loginAcctKeys;

    for (const auto& loginAcctKey : loginAcctKeys) {

        if (auto it = m_acctKey2TCPSessions.find(loginAcctKey); it != m_acctKey2TCPSessions.end()) {

			auto& socketMap = it->second;
            socketMap.erase(session->m_socketId);

            if (socketMap.empty()) {
				SPDLOG_INFO("acctKey:{} remove socket session:{},no session remain.", loginAcctKey, session->m_socketId);
                m_acctKey2TCPSessions.erase(loginAcctKey);
			}
		}
    }
    if (auto it = m_tcpSessions.find(session->m_socketId); it != m_tcpSessions.end()) {

        SPDLOG_INFO("remove socket session:{}", session->m_socketId);

        m_tcpSessions.erase(it);
    }

    return true;
}


void TCPSessionManager::sendResp2C(const std::shared_ptr<TCPSession>& session
    , const AlgoMsg::MsgAlgoCMD cmd
    , std::shared_ptr<google::protobuf::Message> messageBody) {

    if (messageBody) {

        send(session, AcctKey_t(), AlgoMsg::MsgDirection::CS_RSP, cmd, messageBody,true);
    }
}

void TCPSessionManager::sendNotify2C(const AcctKey_t& acctKey, const AlgoMsg::MsgAlgoCMD cmd
    , std::shared_ptr<google::protobuf::Message> messageBody,bool shouldCache) {

    if (messageBody) {

       send(nullptr, acctKey, AlgoMsg::MsgDirection::CS_NTY, cmd, messageBody, shouldCache);
    }
}

void TCPSessionManager::send(const std::shared_ptr<TCPSession>& session
                ,const AcctKey_t& acctKey
                ,const AlgoMsg::MsgDirection direction
                ,const AlgoMsg::MsgAlgoCMD cmd
                ,std::shared_ptr<google::protobuf::Message> messageBody
                ,bool shouldCache) {

    StorageService::getInstance().storeMessage(cmd, messageBody);

    m_strand.post([this, session, acctKey, direction, cmd, messageBody, shouldCache]() {

        auto sendPkgPtr   = std::make_shared<AlgoMsg::MessagePkg>();

        auto messageSeqId = MessageIdGenerator::getInstance().NewId();

        sendPkgPtr->mutable_head()->set_msg_cmd(cmd);
        sendPkgPtr->mutable_head()->set_msg_direction(direction);
        sendPkgPtr->mutable_head()->set_msg_seq_id(messageSeqId);
        sendPkgPtr->mutable_clientkey()->set_acct_type(acctKey.acctType);
        sendPkgPtr->mutable_clientkey()->set_acct(acctKey.acct);
        sendPkgPtr->mutable_clientkey()->set_broker(acctKey.broker);
        messageBody->SerializeToString(sendPkgPtr->mutable_body());

        SPDLOG_DEBUG("sendNotify2C,cmd:{},messageSeqId:{}", (uint32_t)cmd, messageSeqId);

        if (shouldCache) {
            auto containerIt = m_messageId2Pkg.emplace_hint(m_messageId2Pkg.end(), messageSeqId, sendPkgPtr);

            auto& m_it = m_acct2MessageIter[acctKey];

            m_it.emplace_hint(m_it.end(), messageSeqId, containerIt);
        }

        if (session != nullptr) {

            session->queueSendMessage2C(sendPkgPtr);
        }
        else {

            std::scoped_lock<std::mutex> lock(m_mutex);

            if (auto sessions = m_acctKey2TCPSessions.find(acctKey); sessions != m_acctKey2TCPSessions.end()) {

                for (auto& [socketHandle,session] : sessions->second) {

                    session->queueSendMessage2C(sendPkgPtr);
                }
            }
            else {
                SPDLOG_ERROR("sendNotify2C,cmd:{},messageSeqId:{},acctKey:{},no session found", (uint32_t)cmd, messageSeqId, acctKey);
            }
		}

	});
}

void TCPSessionManager::sendMessagesOffset(const std::shared_ptr<AlgoMsg::MessagePkg>& pkgPtr) {


    m_strand.post([this, pkgPtr]() {

        auto  messageSeqId  = pkgPtr->head().msg_seq_id();
        auto& clientKey     = pkgPtr->clientkey();
        auto  acctKey       = AcctKey_t(clientKey.acct_type(), clientKey.acct(), clientKey.broker());

        m_acctSentAcknMaxMessageId[acctKey] = messageSeqId;     // 存储最近发送成功的 NOTIFY  重登录从 acct2MessageIter 发送 messageSeqId 之后的消息

    });
}


void TCPSessionManager::reSend(const std::shared_ptr<TCPSession>& session
    , const AcctKey_t& acctKey
    , const AlgoMsg::MsgLoginRequest& request) {

    m_strand.post([this, session, acctKey, request]() {

        auto& messages_it = m_acct2MessageIter[acctKey];

        auto maxId = m_acctSentAcknMaxMessageId[acctKey];

        auto begMessageId_order = request.order_msg_seq_id() > 0 ? request.order_msg_seq_id() : maxId;

        auto begMessageId_trade = request.trade_msg_seq_id() > 0 ? request.trade_msg_seq_id() : maxId;

        auto begMessageId_algo  = request.algo_msg_seq_id() > 0  ? request.algo_msg_seq_id()  : maxId;

        for (auto& [messageSeqId, containerIt] : messages_it) {

            auto& sendPkgPtr = containerIt->second;

            if (sendPkgPtr->head().msg_cmd() == AlgoMsg::MsgAlgoCMD::CMD_NOTIFY_Order and messageSeqId >= begMessageId_order) {

                session->queueSendMessage2C(sendPkgPtr);

                continue;
            }

            if (sendPkgPtr->head().msg_cmd() == AlgoMsg::MsgAlgoCMD::CMD_NOTIFY_Trade and messageSeqId >= begMessageId_trade) {

                session->queueSendMessage2C(sendPkgPtr);

                continue;
            }

            if (sendPkgPtr->head().msg_cmd() >= AlgoMsg::MsgAlgoCMD::CMD_NOTIFY_AlgoExecutionInfo and messageSeqId >= begMessageId_algo) {

                session->queueSendMessage2C(sendPkgPtr);
            }
        }
    });
}

std::string TCPSessionManager::addMarketDepthSubscribe(const std::shared_ptr<TCPSession>& session
    ,const AlgoMsg::MsgMarketDepthSubcribeRequest* req) {

    if (not isLogin(session)) {
        return "session not login!";
    }

    std::scoped_lock<std::mutex> lock(m_mutex);

    if (req->all_symbols()) {
        if (req->is_unsubscribe()) {
            m_subsribeAllMd.erase(session->m_socketId);
        }
        else {
            m_subsribeAllMd.insert(session->m_socketId);
        }
    }
    else {
        const auto& symbols = req->symbols();
        for (const auto& symbol : req->symbols()) {
            if (req->is_unsubscribe()) {
                if (auto it = m_subscribeSymbolMd.find(symbol); it != m_subscribeSymbolMd.end()) {
                    it->second.erase(session->m_socketId);
                }
            }
            else {
                auto [it,inserted] = m_subscribeSymbolMd.try_emplace(symbol);
                it->second.insert(session->m_socketId);
            }
        }
    }
    return "";
}

std::string TCPSessionManager::addStockConceptQuoteSubscribe(const std::shared_ptr<TCPSession>& session
    , const AlgoMsg::MsgSubscribeStockConceptQuoteRequest* req) {

    if (not isLogin(session)) {
        return "session not login!";
    }

    std::scoped_lock<std::mutex> lock(m_mutex);

    if (req->is_unsubscribe()) {
        m_subsribeConceptQuote.erase(session->m_socketId);
    }
    else {
        m_subsribeConceptQuote.insert(session->m_socketId);
    }
    return "";
}

void TCPSessionManager::pushMarketDepth(const MarketDepth* md) {

    auto cmd = AlgoMsg::MsgAlgoCMD::CMD_PUSH_MarketDepth;

    asio::post(m_strand, [this,cmd,mdPtr = boost::intrusive_ptr(md)]() {

        auto mdPkg = mdPtr->encode2AlgoMessage();

        std::scoped_lock<std::mutex> lock(m_mutex);

        if (m_subscribeSymbolMd.empty() and m_subsribeAllMd.empty()) {
            return;
        }

        auto sendPkgPtr = std::make_shared<AlgoMsg::MessagePkg>();
        sendPkgPtr->mutable_head()->set_msg_cmd(cmd);
        sendPkgPtr->mutable_head()->set_msg_direction(AlgoMsg::MsgDirection::CS_NTY);
        sendPkgPtr->mutable_head()->set_msg_seq_id(MessageIdGenerator::getInstance().NewId());
        mdPkg->SerializeToString(sendPkgPtr->mutable_body());

        for (auto const& socketId : m_subsribeAllMd) {
            if (auto it = m_tcpSessions.find(socketId); it != m_tcpSessions.end()) {
                it->second->queueSendMessage2C(sendPkgPtr); 
            }
        }
        if(auto it = m_subscribeSymbolMd.find(mdPtr->symbol()); it != m_subscribeSymbolMd.end() ) {
            for (auto const& socketId : it->second) {
                if (auto it = m_tcpSessions.find(socketId); it != m_tcpSessions.end()) {
                    it->second->queueSendMessage2C(sendPkgPtr);
                }
            }
        }
    });
}

void TCPSessionManager::pushStockConceptQuotes() {

    if(not agcommon::AshareMarketTime::isInMarketTime()) {
        return;
    }
    auto cmd = AlgoMsg::MsgAlgoCMD::CMD_PUSH_StockConceptQuotes;

    asio::post(m_strand, [this, cmd]() {

        std::scoped_lock<std::mutex> lock(m_mutex);

        if (m_subsribeConceptQuote.empty()) {
            return;
        }

        auto mdPkg = ConceptQuote::getInstance().encodeConceptQuotes();
        if (mdPkg) {
            auto sendPkgPtr = std::make_shared<AlgoMsg::MessagePkg>();
            sendPkgPtr->mutable_head()->set_msg_cmd(cmd);
            sendPkgPtr->mutable_head()->set_msg_direction(AlgoMsg::MsgDirection::CS_NTY);
            sendPkgPtr->mutable_head()->set_msg_seq_id(MessageIdGenerator::getInstance().NewId());
            mdPkg->SerializeToString(sendPkgPtr->mutable_body());

            for (auto const& socketId : m_subsribeConceptQuote) {
                if (auto it = m_tcpSessions.find(socketId); it != m_tcpSessions.end()) {
                    it->second->queueSendMessage2C(sendPkgPtr);
                }
            }
        }
    }
    );
}

void TCPSessionManager::pushStockConceptInfos(const std::shared_ptr<TCPSession>& session) {

    auto cmd = AlgoMsg::MsgAlgoCMD::CMD_PUSH_StockConceptInfos;

    asio::post(m_strand, [this, session, cmd]() {

        std::scoped_lock<std::mutex> lock(m_mutex);

        auto infoPkg = ConceptQuote::getInstance().encodeConceptInfos();

        auto sendPkgPtr = std::make_shared<AlgoMsg::MessagePkg>();
        sendPkgPtr->mutable_head()->set_msg_cmd(cmd);
        sendPkgPtr->mutable_head()->set_msg_direction(AlgoMsg::MsgDirection::CS_NTY);
        sendPkgPtr->mutable_head()->set_msg_seq_id(MessageIdGenerator::getInstance().NewId());
        infoPkg->SerializeToString(sendPkgPtr->mutable_body());

        session->queueSendMessage2C(sendPkgPtr);
    }
    );
}