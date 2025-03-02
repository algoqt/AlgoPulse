#include "TCPSession.h"
#include "StorageService.h"
#include "MarketDepth.h"
#include "Order.h"
#include "IdGenerator.h"
#include "ContextService.h"

TCPSession::TCPSession(std::shared_ptr<asio::io_context> io_context)
          :m_io_context(io_context)
          ,m_socket(*io_context)
          ,m_strand(*io_context){

    m_receivingBuffer.reserve(1024);

    m_sendingBuffer.reserve(2048);
}

void TCPSession::start(const AlgoMessagePkgHandle_t& handler)
{
    assert(handler!=nullptr);

    onMessageHandler = handler;

    do_read_header();

    m_socketHandle = m_socket.native_handle();

    auto ip = m_socket.remote_endpoint().address().to_string();

    auto port = m_socket.remote_endpoint().port();

    //m_socket.set_option(asio::ip::tcp::no_delay(true));

    SPDLOG_INFO("new TCPSession::started,socket:{},isOpen:{},remote_ep:{}:{}", (int)m_socketHandle,m_socket.is_open(), ip,port);
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
                SPDLOG_INFO("do_read_header,socket {} with error: {}", (int)self->m_socketHandle, ec.message());
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

                AlgoMsg::MessagePkg recvPkgPtr;

                recvPkgPtr.ParseFromArray(self->m_receivingBuffer.data() + 4, (int)body_length);

                SPDLOG_INFO("recv msg_cmd:{},msg_seq_id:{},body_length:{}", (uint32_t)recvPkgPtr.head().msg_cmd(), recvPkgPtr.head().msg_seq_id(),body_length);

                self->onMessageHandler(self, std::move(recvPkgPtr));

                self->do_read_header();
            }
            else
            {
                SPDLOG_INFO("do_read_body,socket {} with error:{}", (int)self->m_socketHandle, ec.message());

                self->safeDisConnect();
            }
        });
}

void TCPSession::queueSendMessage2C(const std::shared_ptr<AlgoMsg::MessagePkg>& pkgPtr) {

    asio::post(m_strand, [self= shared_from_this(), pkgPtr]() {

        self->sendingMessageQueue.emplace_back( pkgPtr);

        if (self->sendingMessageQueue.size() > 1) {
            return;
        }
        self->_send();
        }
    );
}

void TCPSession::_send() {

    if (sendingMessageQueue.empty()) {
        return;
    }

    auto& sendPkgPtr = sendingMessageQueue.front();

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

                    self->sendingMessageQueue.pop_front();

                    SPDLOG_DEBUG("NotifyOK.cmd {},messageid:{},remain:{}", (uint32_t)cmd, messageSeqId, self->sendingMessageQueue.size());

                    if (not self->sendingMessageQueue.empty()) {

                        asio::post(self->m_strand, [self]() {self->_send(); });
                    }
                }
                else
                {
                    SPDLOG_INFO("send Failed.cmd {},messageid:{}", (uint32_t)cmd, messageSeqId);

                    SPDLOG_INFO("socket {} with error:{}", (int)self->m_socketHandle, ec.message());

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

            SPDLOG_ERROR("{} Shutdown failed:{} ", (int)m_socketHandle, ec.message());
        }

        m_socket.close(ec);

        if (ec) {

            SPDLOG_ERROR("{} Close failed:{} ", (int)m_socketHandle, ec.message());
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

    m_timer.expires_after(std::chrono::seconds(15));

    m_timer.async_wait([this](boost::system::error_code ec) {

        asio::post(m_strand, [this]() {

			printStatInfo();
		});
    });
}
TCPSessionManager::~TCPSessionManager() {

    SPDLOG_INFO("TCPSessionManager::~TCPSessionManager()");
}

std::shared_ptr<TCPSession> TCPSessionManager::createTCPSession(std::shared_ptr<asio::io_context> io_context) {

    auto session = std::make_shared<TCPSession>(io_context);

    return session;
}

void TCPSessionManager::closeAllSessions() {

    ContextService::getInstance().stopContext("TCPSessionManager");
}

void TCPSessionManager::printStatInfo() {
    {
        std::scoped_lock<std::mutex> lock(m_mutex);

        auto sessionCnt         = TCPSessions.size();
        auto acctCnt            = acctKey2TCPSessions.size();
        auto remainMessageCnt   = messageId2Pkg.size();

        SPDLOG_INFO("[Stat]sessions:{},accts:{},remain messages:{},sent messages:{}"
            ,sessionCnt
            , acctCnt
            , remainMessageCnt
            , m_sentMessageCnt.load());
        
        SPDLOG_INFO("MarketDepth alives:{},Order alives:{},Trade alives:{}"
            , MarketDepth::totalAliveObjectCount.load()
            , Order::totalAliveObjectCount.load()
            , Trade::totalAliveObjectCount.load());
    }

    m_timer.expires_after(std::chrono::seconds(15));

    m_timer.async_wait([this](boost::system::error_code ec) {

        asio::post(m_strand, [this]() {
            printStatInfo();
            });
        });
}

bool TCPSessionManager::isLogin(const AcctKey_t& acctKey) {

    std::scoped_lock lock(m_mutex);

    return acctKey2TCPSessions.find(acctKey) != acctKey2TCPSessions.end();

}
bool TCPSessionManager::login(const AcctKey_t& acctKey,const AlgoMsg::MsgLoginRequest& request, std::shared_ptr<TCPSession> session) {

    std::scoped_lock lock(m_mutex);

    sockect_handel_t sockect_handel = session->m_socketHandle;

    auto [it, inserted] = TCPSessions.try_emplace(sockect_handel, session);

    if (not inserted) {
        SPDLOG_INFO("addConnection already have:{}. login:{},resendmessage:{}", (int)sockect_handel, acctKey, request.resendmessage());
    }
    session->loginAcctKeys.insert(acctKey);

    auto [socketMap_it,inserted2] = acctKey2TCPSessions.try_emplace(acctKey,
        std::map<sockect_handel_t, std::shared_ptr<TCPSession>>{ {sockect_handel, session} });

    if (not inserted2) {
        socketMap_it->second.insert({ sockect_handel, session });
    }

    if (request.resendmessage()) {
        reSend(session, acctKey, request);
    }

    return true;
}

bool TCPSessionManager::addTCPSession(std::shared_ptr<TCPSession>& session) {

    std::scoped_lock<std::mutex> lock(m_mutex);

    auto it = TCPSessions.insert({ session->m_socketHandle, session });

    SPDLOG_INFO("addConnection:{}", (int)session->m_socketHandle);

    return it.second;
}

bool TCPSessionManager::removeTCPSession(const std::shared_ptr<TCPSession>& session) {

    std::scoped_lock<std::mutex> lock(m_mutex);

    auto& loginAcctKeys = session->loginAcctKeys;

    for (auto& loginAcctKey : loginAcctKeys) {

        if (auto it = acctKey2TCPSessions.find(loginAcctKey); it != acctKey2TCPSessions.end()) {

			auto& socketMap = it->second;
            socketMap.erase(session->m_socketHandle);

            if (socketMap.empty()) {
				SPDLOG_INFO("acctKey:{} remove socket session:{},no session remain.", loginAcctKey, (int)session->m_socketHandle);
                acctKey2TCPSessions.erase(loginAcctKey);
			}
		}
    }
    if (auto it = TCPSessions.find(session->m_socketHandle); it != TCPSessions.end()) {

        SPDLOG_INFO("remove socket session:{}", (int)session->m_socketHandle);

        TCPSessions.erase(it);
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

        size_t bodySize   = messageBody->ByteSizeLong();

        sendPkgPtr->mutable_head()->set_msg_cmd(cmd);
        sendPkgPtr->mutable_head()->set_msg_direction(direction);
        sendPkgPtr->mutable_head()->set_msg_seq_id(messageSeqId);
        sendPkgPtr->mutable_clientkey()->set_acct_type(acctKey.acctType);
        sendPkgPtr->mutable_clientkey()->set_acct(acctKey.acct);
        sendPkgPtr->mutable_clientkey()->set_broker(acctKey.broker);

        messageBody->SerializeToString(sendPkgPtr->mutable_body());

        SPDLOG_DEBUG("sendNotify2C,cmd:{},messageSeqId:{}", (uint32_t)cmd, messageSeqId);

        if (shouldCache) {
            auto containerIt = messageId2Pkg.emplace_hint(messageId2Pkg.end(), messageSeqId, sendPkgPtr);

            auto& m_it = acct2MessageIter[acctKey];

            m_it.emplace_hint(m_it.end(), messageSeqId, containerIt);
        }

        if (session != nullptr) {

            session->queueSendMessage2C(sendPkgPtr);
        }
        else {

            std::scoped_lock<std::mutex> lock(m_mutex);

            if (auto sessions = acctKey2TCPSessions.find(acctKey); sessions != acctKey2TCPSessions.end()) {

                for (auto& [socketHandle,session] : sessions->second) {

                    session->queueSendMessage2C(sendPkgPtr);
                }
            }
            //else {
            //    SPDLOG_DEBUG("sendNotify2C,cmd:{},messageSeqId:{},no session found", cmd, messageSeqId);
            //}
		}

	});
}

void TCPSessionManager::sendMessagesOffset(const std::shared_ptr<AlgoMsg::MessagePkg>& pkgPtr) {


    m_strand.post([this, pkgPtr]() {

        auto  messageSeqId  = pkgPtr->head().msg_seq_id();
        auto& clientKey     = pkgPtr->clientkey();
        auto  acctKey       = AcctKey_t(clientKey.acct_type(), clientKey.acct(), clientKey.broker());

        acctSentAcknMaxMessageId[acctKey] = messageSeqId;     // 存储最近发送成功的 NOTIFY  重登录从 acct2MessageIter 发送 messageSeqId 之后的消息

    });
}


void TCPSessionManager::reSend(const std::shared_ptr<TCPSession>& session
    , const AcctKey_t& acctKey
    , const AlgoMsg::MsgLoginRequest& request) {

    m_strand.post([this, session, acctKey, request]() {

        auto& messages_it = acct2MessageIter[acctKey];

        auto maxId = acctSentAcknMaxMessageId[acctKey];

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