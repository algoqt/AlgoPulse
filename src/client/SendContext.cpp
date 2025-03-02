#include "SendContext.h"
 

SendContext::SendContext():io_context(std::make_shared<asio::io_context>())
    , process_context(std::make_shared<boost::asio::io_context>())

    , m_socket{*io_context }

    , m_clientSpi(nullptr)

    , m_ipStr{"127.0.0.1"}

    , m_port{8081}

    , m_autoReConnet(false)

    , m_reConnectIntervalSecs(10)
{
    m_sendingBuffer.reserve(2048);
    m_receivingBuffer.reserve(2048);


    m_threads.push_back(std::thread([this]() {
        asio::executor_work_guard<asio::io_context::executor_type> work = asio::make_work_guard(io_context->get_executor());
        SPDLOG_INFO("SendContext::init network context");
        io_context->run();
        }));

    m_threads.push_back(std::thread([this]() {
        asio::executor_work_guard<asio::io_context::executor_type> work = asio::make_work_guard(process_context->get_executor());
        SPDLOG_INFO("SendContext::init process context");
        process_context->run();
        }));

    m_keepRunning = true;

}

void SendContext::setClientApi(ClientSpi* clientSpi) {

    m_clientSpi = clientSpi;

}

void SendContext::connetSetting(const std::string& ipStr, const int port, bool autoReConnet, int reConnectIntervalSecs)
{
	m_autoReConnet          = autoReConnet;

	m_reConnectIntervalSecs = reConnectIntervalSecs;

	m_ipStr = ipStr;

	m_port = port;
}

bool SendContext::connect() {

    try {
        if (m_isConneted) {

            boost::system::error_code close_ec;

            m_socket.close(close_ec);

            m_isConneted = false;
        }

        if (m_keepRunning) {

            SPDLOG_INFO("try connet....");

            m_socket.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string(m_ipStr), m_port));

            m_socket.set_option(boost::asio::ip::tcp::no_delay(true));

            SPDLOG_INFO("connet successfully.");

            m_isConneted = true;

            start();

            return true;
        } 

    }
    catch (const boost::system::system_error& e) {

        SPDLOG_ERROR("connet to {},{} failed,reconnect {} seconds later:{}", m_ipStr, m_port, m_reConnectIntervalSecs, e.code().message());

        if (m_autoReConnet && m_reConnectIntervalSecs > 0) {

            asyncReConnect();
        }
        else {
            SPDLOG_ERROR("connet to {},{} failed,won't reconnect :{}", m_ipStr, m_port, e.what());
        }
    }
    catch (std::exception& e) {

		SPDLOG_ERROR("connet to {},{} failed,won't reconnect :{}", m_ipStr, m_port, e.what());
        abort();
        return false;
	}
    return false;
}

void SendContext::start() {

    do_read_pkgHeader();
}

void SendContext::stop() {

    m_keepRunning = false;
    m_autoReConnet = false;

	m_socket.close();

    io_context->stop();
    process_context->stop();

    for (auto& t : m_threads) {
		t.join();
	}
}
void SendContext::asyncReConnect() {

    if (m_isConneted) {

        boost::system::error_code close_ec;

        m_socket.close(close_ec);

        SPDLOG_INFO("socket close to reconnet:{}", close_ec.message());

        m_isConneted = false;
    }

    if (m_keepRunning) {

        m_socket.async_connect(asio::ip::tcp::endpoint(asio::ip::address::from_string(m_ipStr), m_port), [this](boost::system::error_code ec) {

            if (!ec) {

                SPDLOG_INFO("reconnet to {},{} sucessful.", m_ipStr, m_port);

                m_isConneted = true;

                start();
            }
            else {

                SPDLOG_ERROR("reconnet to {},{} failed:{}", m_ipStr, m_port, ec.message());

                if (m_autoReConnet && m_reConnectIntervalSecs > 0) {

                    std::this_thread::sleep_for(std::chrono::seconds(m_reConnectIntervalSecs));

                    asyncReConnect();
                }
            }
            });
    }
}

int SendContext::send(const AlgoMsg::MsgAlgoCMD cmd, const google::protobuf::Message* message, const uint64_t request_id) {
    //message->DebugString();

    auto sendPkgPtr = new AlgoMsg::MessagePkg;

    size_t bodySize = message->ByteSizeLong();

    sendPkgPtr->mutable_head()->set_msg_cmd(cmd);

    sendPkgPtr->mutable_head()->set_msg_direction(AlgoMsg::MsgDirection::CS_REQ);

    sendPkgPtr->mutable_head()->set_msg_seq_id(MessageIdGenerator::getInstance().NewId());

    message->SerializeToString(sendPkgPtr->mutable_body());

    asio::post(*io_context, [self = shared_from_this(), cmd, sendPkgPtr, request_id]() {

        self->sendingMessageQueue.push(sendPkgPtr);

        auto it = self->timeoutMessageMap.insert({ std::time(nullptr), sendPkgPtr});

        self->sendingMessageMap.emplace_hint(self->sendingMessageMap.end(), request_id, std::pair(sendPkgPtr, it));

        if (self->sendingMessageQueue.size() > 1) {
            return;
        }

        self->_send();
    });

    return 0;
}

void SendContext::_send() {


    auto sendPkgPtr = sendingMessageQueue.front();

    size_t pkgBodyByteSize = sendPkgPtr->ByteSizeLong();

    auto buffer_size = 4 + pkgBodyByteSize;

    while (m_sendingBuffer.capacity() < buffer_size) {
        buffer_size = (buffer_size / 1024) * 1024 + 1024;
        SPDLOG_WARN("m_sendingBuffer capacity {},RESIZE TO :{} ", m_sendingBuffer.capacity(), buffer_size);
        m_sendingBuffer.clear();
        m_sendingBuffer.reserve(buffer_size);
    }

    size_t headByteSize = encodeHeader(m_sendingBuffer.data(), pkgBodyByteSize);

    auto messageSeqId = sendPkgPtr->head().msg_seq_id();

    sendPkgPtr->SerializeToArray(m_sendingBuffer.data() + headByteSize, pkgBodyByteSize);

    SPDLOG_INFO("{}:{},{}", messageSeqId,headByteSize, pkgBodyByteSize);

    asio::async_write(m_socket, asio::buffer(m_sendingBuffer.data(), headByteSize + pkgBodyByteSize),
        [self = shared_from_this(), sendPkgPtr](boost::system::error_code ec, std::size_t bytes_transferred) {
            
            auto cmd = sendPkgPtr->head().msg_cmd();

            auto messageSeqId = sendPkgPtr->head().msg_seq_id();

            if (!ec) {

                self->sendingMessageQueue.pop();

                if (not self->sendingMessageQueue.empty()) {

                    asio::dispatch(*self->io_context, [self]() {  self->_send(); });
                }
            }
            else
            {
                SPDLOG_ERROR("send Failed.cmd {},messageid:{}", (uint32_t)cmd, messageSeqId);

                SPDLOG_ERROR("socket {} with error:{}",(int)self->m_socket.native_handle(), ec.message());
            }
        });
    }

void SendContext::do_read_pkgHeader()
{

    boost::asio::async_read(m_socket, boost::asio::buffer(m_receivingBuffer.data(), HEADER_LENGTH_BODY),
        [self = shared_from_this()](boost::system::error_code ec, std::size_t bytes_transferred)
        {
            if (!ec)
            {
                auto body_length = self->decode_header();
                SPDLOG_DEBUG("read_header [{}],body string length will be:{}", bytes_transferred, body_length);
                self->do_read_pkgBody(body_length);
            }
            else
            {
                if ((ec == asio::error::connection_aborted) ||
                    (ec == asio::error::connection_refused) ||
                    (ec == asio::error::connection_reset) ||
                    (ec == asio::error::eof) ||
                    (ec == asio::error::operation_aborted))
                    return;

                SPDLOG_INFO("read_header error:{},{}",ec.value(), ec.message());
                self->connect();
            }
        });
}

void SendContext::do_read_pkgBody(std::size_t body_length)
{
    auto buffer_size = 4 + body_length;

    while (m_receivingBuffer.capacity() < buffer_size) {
        buffer_size = (buffer_size / 1024) * 1024 + 1024;
        SPDLOG_WARN("m_receivingBuffer capacity {},RESIZE TO :{} ", m_receivingBuffer.capacity(), buffer_size);
        m_receivingBuffer.clear();
        m_receivingBuffer.reserve(buffer_size);
    }

    boost::asio::async_read(m_socket,
        boost::asio::buffer(m_receivingBuffer.data() + 4, body_length),
        [self = shared_from_this(), body_length](boost::system::error_code ec, std::size_t bytes_transferred)
        {

            if (!ec)
            {
                auto recvPkgPtr = new AlgoMsg::MessagePkg;

                bool ok = recvPkgPtr->ParseFromArray(self->m_receivingBuffer.data() + 4, body_length);

                SPDLOG_DEBUG("recv msg_cmd:{},msg_seq_id:{},body_length:{}", (uint32_t)recvPkgPtr->head().msg_cmd(), recvPkgPtr->head().msg_seq_id(), body_length);


                if (ok) {

                    asio::post(*self->process_context, [self, recvPkgPtr]() {
                        self->onMessageHandle(recvPkgPtr);
                        delete recvPkgPtr;
                        }
                    );
                }
                self->do_read_pkgHeader();
            }
            else
            {
                if ((ec == asio::error::connection_aborted) ||
                    (ec == asio::error::connection_refused) ||
                    (ec == asio::error::connection_reset) ||
                    (ec == asio::error::eof) ||
                    (ec == asio::error::operation_aborted))
                    return;

                SPDLOG_INFO("do_read_body error:{}, close soceket:{}", ec.message(),(int)self->m_socket.native_handle());
                self->connect();
            }
        });
}


size_t SendContext::encodePkg(unsigned char* sendingBuf

    , const AlgoMsg::MsgAlgoCMD cmd

    , const AlgoMsg::MsgDirection direction

    , const google::protobuf::Message* message, AlgoMsg::MessagePkg* sendPkgPtr) {

    size_t bodySize = message->ByteSizeLong();
    sendPkgPtr->mutable_head()->set_msg_cmd(cmd);
    sendPkgPtr->mutable_head()->set_msg_direction(direction);
    sendPkgPtr->mutable_head()->set_msg_seq_id(MessageIdGenerator::getInstance().NewId());

    message->SerializeToString(sendPkgPtr->mutable_body());

    size_t pkgBodyByteSize = sendPkgPtr->ByteSizeLong();

    SPDLOG_DEBUG("message bodySIZE:{},pkgSize:{}", bodySize, pkgBodyByteSize);

    size_t headByteSize = encodeHeader(sendingBuf, pkgBodyByteSize);

    sendPkgPtr->SerializeToArray(sendingBuf + headByteSize, pkgBodyByteSize);

    return headByteSize + pkgBodyByteSize;
}

size_t SendContext::encodeHeader(unsigned char* buf, uint32_t body_length) {

    unsigned char* buf_pos = buf;

    *buf_pos++ = (char)((body_length & 0xFF000000) >> 24);
    *buf_pos++ = (char)((body_length & 0x00FF0000) >> 16);
    *buf_pos++ = (char)((body_length & 0x0000FF00) >> 8);
    *buf_pos++ = (char)(body_length & 0x000000FF);

    return 4;
}

void SendContext::onMessageHandle(const AlgoMsg::MessagePkg* recvPkg) {

    auto cmd = recvPkg->head().msg_cmd();

    bool print4Debug = false;
    //SPDLOG_INFO("receive messageid:{},cmd:{}", recvPkg->head().msg_seq_id(),recvPkg->head().msg_cmd());
    google::protobuf::Message* message = nullptr;

    bool parseOK = false;

    std::string messageStr = "";

    uint64_t request_id = 0;

    switch (cmd)
    {
        case AlgoMsg::CMD_LoginResponse: {

            AlgoMsg::MsgLoginResponse resp;
            parseOK = resp.ParseFromArray(recvPkg->body().data(), recvPkg->body().length());
            if (parseOK) {
                request_id = resp.request_id();
                //messageStr = resp.DebugString();
                if (m_clientSpi)
                    m_clientSpi->onLogin(resp);
            }
            break;
        }
        case AlgoMsg::CMD_AlgoInstanceCreateResponse: {

            AlgoMsg::MsgAlgoInstanceCreateResponse resp;

            parseOK = resp.ParseFromArray(recvPkg->body().data(), recvPkg->body().length());

            if (parseOK) {
                request_id = resp.request_id();
               // messageStr = resp.DebugString();
                if (m_clientSpi)
                    m_clientSpi->onCreateAlgoInstance(resp);
            }
            break;
        }
        case AlgoMsg::CMD_AlgoInstanceQueryResponse: {

            AlgoMsg::MsgAlgoInstanceQueryResponse resp;

            parseOK = resp.ParseFromArray(recvPkg->body().data(), recvPkg->body().length());

            if (parseOK) {
                request_id = resp.request_id();
                //messageStr = resp.DebugString();
                if (m_clientSpi)
                    m_clientSpi->onQueryAlgoInstance(resp);
            }
            break;
        }
        case AlgoMsg::CMD_AlgoInstanceUpdateResponse: {

            AlgoMsg::MsgAlgoInstanceUpdateResponse resp;

            parseOK = resp.ParseFromArray(recvPkg->body().data(), recvPkg->body().length());

            if (parseOK) {

                request_id = resp.request_id();
                //messageStr = resp.DebugString();
                if (m_clientSpi)
                    m_clientSpi->onUpdateAlgoInstance(resp);
            }
            break;
        }
        case AlgoMsg::CMD_NOTIFY_AlgoExecutionInfo: {

            AlgoMsg::MsgAlgoPerformance notify;

            parseOK = notify.ParseFromArray(recvPkg->body().data(), recvPkg->body().length());

            if (parseOK) {
                //messageStr = notify.DebugString();
                if (m_clientSpi)

                    m_clientSpi->onNotifyAlgoInstanceExecutionInfo(notify);
            }
            break;
        }
        case AlgoMsg::CMD_NOTIFY_Order: {

            AlgoMsg::MsgOrderInfo notify;

            parseOK = notify.ParseFromArray(recvPkg->body().data(), recvPkg->body().length());

            if (parseOK) {
                //messageStr = notify.DebugString();
                if (m_clientSpi)
                    m_clientSpi->OnNotifyOrderInfo(notify);
            }
            break;
        }
        case AlgoMsg::CMD_NOTIFY_Trade: {

            AlgoMsg::MsgTradeInfo notify;

            parseOK = notify.ParseFromArray(recvPkg->body().data(), recvPkg->body().length());

            if (parseOK)
            {
                //messageStr = notify.DebugString();
                if (m_clientSpi)
                    m_clientSpi->OnNotifyTradeInfo(notify);
            }
            break;
        }
        default: {
            //SPDLOG_WARN("AlgoMessage cmd:{},messageid:{} hasnot handled!", (uint32_t)recvPkg->head().msg_cmd(), recvPkg->head().msg_seq_id());
            break;
        }
    }

    // 从发送及超时队列里移除 
    if (request_id > 0) {

        asio::post(*io_context, [self = shared_from_this(), request_id]() {

            auto it = self->sendingMessageMap.find(request_id);

            if (it != self->sendingMessageMap.end()) {

                auto& pkg_ptr = std::get<0>(it->second);  // it->second == std::tuple(pkg_ptr,time_it)

                auto& time_it = std::get<1>(it->second);

                delete pkg_ptr;

                self->timeoutMessageMap.erase(time_it);

                self->sendingMessageMap.erase(it);
            }
        });
    }
}



void SendContext::printForDebug(const google::protobuf::Message* message) {

    message->PrintDebugString();
}