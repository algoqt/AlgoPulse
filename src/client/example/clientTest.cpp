#include "AlgoMessages.pb.h"
#include <string>
#include "ClientApi.h"
#include "IdGenerator.h"
#include <chrono>
#include <spdlog/spdlog.h>


using namespace std::chrono;
using AlgoOrderId_t = uint64_t;
using Symbol_t = std::string;

enum class AlgoErrorCode {

    ALGO_OK = 0,
    ALGO_QUOTE_ERROR = 101,
    ALGO_DUPLICATE_ERROR = 103,
    ALGO_ORDERSERVICE_ANORMAL = 105,
    ALGO_ORDERCHECK_FAILED = 107,
    ALGO_ACCT_NOT_LOGIN = 109,
    ALGO_ACCT_ALREADY_LOGIN = 110,
    ALGO_ORDERROUTER_ERROR = 111,
    ALGO_NOT_EXISTS = 113,
    ALGO_ERROR = 199,
};

std::unordered_map<uint64_t, std::pair<system_clock::time_point, system_clock::time_point>> pingpong;

std::set<AlgoOrderId_t> testCancelOrders;

std::mutex g_mutex;

auto firstRecvTime = system_clock::now();

auto lastStatTime = system_clock::now();

class MyClientSpi : public ClientSpi {

public:

    MyClientSpi() :cnt(0), loginOk{false} { 

        algoInstanceInfoMap.reserve(5000);
    }

    void onLogin(const AlgoMsg::MsgLoginResponse& loginResponse) override {

        if (loginResponse.error_code() == 0) {
            loginOk = true;
        }

        SPDLOG_INFO("onLoginResponse:\n{}", loginResponse.DebugString());
    }

    void onCreateAlgoInstance(const AlgoMsg::MsgAlgoInstanceCreateResponse& msgAlgoInstanceCreateResponse) override {

        std::scoped_lock lock(g_mutex);

        auto request_id = msgAlgoInstanceCreateResponse.request_id();

        pingpong[request_id].second = system_clock::now();

        if (msgAlgoInstanceCreateResponse.error_code() != (int32_t)AlgoErrorCode::ALGO_OK) {
            SPDLOG_INFO("onCreateAlgoInstance:\n{}", msgAlgoInstanceCreateResponse.DebugString());
        }
        if (cnt == 0) {
            firstRecvTime = system_clock::now();
            lastStatTime = system_clock::now();
        }
        cnt++;
        int everyCnt = 1000;
        if (cnt % everyCnt == 0) {

            auto dur = duration_cast<std::chrono::milliseconds>(system_clock::now() - lastStatTime);

            auto total_dur = duration_cast<std::chrono::milliseconds>(system_clock::now() - firstRecvTime);

            SPDLOG_INFO("total recv {} algo response,total time:{}ms,with last {} algos cost time:{}ms", cnt, total_dur.count(), everyCnt, dur.count());

            lastStatTime = system_clock::now();
        }
        //SPDLOG_INFO("onCreateAlgoInstance:\n{}", msgAlgoInstanceCreateResponse.DebugString());
    }

    void onUpdateAlgoInstance(const AlgoMsg::MsgAlgoInstanceUpdateResponse& msgAlgoInstanceUpdateResponse) override {

        SPDLOG_INFO("onUpdateAlgoInstance:\n{}", msgAlgoInstanceUpdateResponse.DebugString());
    }

    void onQueryAlgoInstance(const AlgoMsg::MsgAlgoInstanceQueryResponse& msgAlgoInstanceQueryResponse) override {

        SPDLOG_INFO("onQueryAlgoInstance:\n{}", msgAlgoInstanceQueryResponse.DebugString());
    };

    void onQueryOrder(const AlgoMsg::MsgOrderQueryResponse& msgOrderQueryResponse) override {

        //SPDLOG_INFO("onQueryOrder:\n{}", msgOrderQueryResponse.DebugString());
    };

    void onQueryTrade(const AlgoMsg::MsgTradeQueryResponse& MsgTradeQueryResponse) override {

        //SPDLOG_INFO("onQueryOrderTrade:\n{}", MsgTradeQueryResponse.DebugString());
    };

    // 推送
    void onNotifyAlgoInstanceExecutionInfo(const AlgoMsg::MsgAlgoPerformance& msgAlgoPerformance) override {

        algoInstanceInfoMap[msgAlgoPerformance.algo_order_id()] = msgAlgoPerformance;

        if (msgAlgoPerformance.symbol() == "601318.SH" or msgAlgoPerformance.symbol() == "002594.SZ") {

            if (testCancelOrders.find(msgAlgoPerformance.algo_order_id()) == testCancelOrders.end()) {

                testCancelOrders.insert(msgAlgoPerformance.algo_order_id());
            }
        }
        if(msgAlgoPerformance.algo_status() >= 6)
            SPDLOG_INFO("onNotifyAlgoInstanceExecutionInfo:\n{}", msgAlgoPerformance.DebugString());
    };

    void OnNotifyOrderInfo(const AlgoMsg::MsgOrderInfo& msgOrderInfo) override {

        //SPDLOG_INFO("OnNotifyOrderInfo:\n{}", msgOrderInfo.DebugString());
    };

    void OnNotifyTradeInfo(const AlgoMsg::MsgTradeInfo& msgOrderTradeInfo) override {

        //SPDLOG_INFO("OnNotifyOrderTradeInfo:\n{}", msgOrderTradeInfo.DebugString());
    }

public:

    uint64_t cnt    { 0 };
    bool     loginOk{ false };
    std::unordered_map<AlgoOrderId_t, AlgoMsg::MsgAlgoPerformance> algoInstanceInfoMap;
};


int main(int argc, char** argv) {

    GOOGLE_PROTOBUF_VERIFY_VERSION;

    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e][%P:%5t][%^%l%$][%s:%#]%v");
    spdlog::flush_on(spdlog::level::warn);
    spdlog::flush_every(std::chrono::seconds(3));

    //if (argc < 4) {
    //    std::cout << "use " << argv[0] << " acct amt duration " << std::endl;
    //    std::system("pause");
    //    return 0;
    //}
    //std::string acct = argv[1];
    //float amt = std::stof(argv[2]);
    //float duration = std::stof(argv[3]);

    std::string acct = "btTest01";

    float amt = 10;

    float duration = 200;

    auto tradeSide = 1;

    SPDLOG_INFO("Input AcctType:B for BT_STOCK S for SIMLIVE_STOCK");

    char acctType_in;
    std::cin >> acctType_in;

    auto acctType = AlgoMsg::AcctType::SIMLIVE_STOCK;
    if (acctType_in == 'B') {
        acctType = AlgoMsg::AcctType::BT_STOCK;
    }

    auto clientApi = ClientApi::createClientApi();

    if (clientApi->init(std::string("127.0.0.1"), 8081, true, 5)) {
        SPDLOG_INFO("clientApi init failed");
        //return 0;
    }

    MyClientSpi* myClientSpi = new MyClientSpi();

    clientApi->registerSpi(myClientSpi);

    SPDLOG_INFO("TEST FOR {},{}", AlgoMsg::AcctType_Name(acctType), acct);

    AlgoMsg::MsgLoginRequest loginReq;

    loginReq.set_request_id(RequestIdGenerator::getInstance().NewId());

    loginReq.set_user_name("user1");
    loginReq.set_password("123456");
    loginReq.set_acct_type(acctType);
    loginReq.set_acct(acct);
    loginReq.set_resendmessage(false);
    loginReq.set_algo_msg_seq_id(0);
    loginReq.set_order_msg_seq_id(0);
    loginReq.set_trade_msg_seq_id(0);

    SPDLOG_INFO("MsgLoginRequest:\n{}", loginReq.DebugString());


    int ret = clientApi->login(loginReq);
    if (ret != 0) {
        SPDLOG_INFO("login failed");
        return 0;
    }

    SPDLOG_INFO("wait for login....");

    do {

        std::this_thread::sleep_for(std::chrono::seconds(1));

    } while (not myClientSpi->loginOk);

    SPDLOG_INFO("login successful.");



    auto symbols = std::vector<Symbol_t>{ "600519.SH", "000651.SZ", "600030.SH" };

    for (auto i = 0; i < 1; i++) {

        for (auto& symbol : symbols) {

            auto now = std::chrono::system_clock::now();
            std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
            std::tm now_tm = *std::localtime(&now_time_t);
            int year = now_tm.tm_year + 1900; // tm_year 是从 1900 年开始的
            int month = now_tm.tm_mon + 1;    // tm_mon 是从 0 开始的
            int day = now_tm.tm_mday;
            uint64_t dt = year * 10000 + month * 100 + day;

            if (acctType == AlgoMsg::AcctType::BT_STOCK) {
                dt = 20250226;
            }
            for (auto& ts : { 1,2 }) {
                    AlgoMsg::MsgAlgoInstanceCreateRequest orderReq;

                    orderReq.set_request_id(RequestIdGenerator::getInstance().NewId());
                    orderReq.set_acct_type(acctType);
                    orderReq.set_acct(acct);
                    orderReq.set_vendor_id(AlgoMsg::MsgAlgoVendorId::VendorId_DEFAULT);
                    orderReq.set_algo_strategy(AlgoMsg::MsgAlgoStrategy::TWAP);

                    auto qty = 1500;

                    AlgoMsg::MsgAlgoOrder* msgAlgoOrder = orderReq.mutable_algo_order();

                    msgAlgoOrder->set_client_algo_order_id(std::string(symbol));

                    msgAlgoOrder->set_symbol(symbol);
                    msgAlgoOrder->set_order_qty(qty);
                    msgAlgoOrder->set_order_side(ts);

                    msgAlgoOrder->set_start_time(dt * 1000000 + 93500);

                    msgAlgoOrder->set_end_time(dt * 1000000 + 100000);

                    msgAlgoOrder->set_exec_duration(duration);

                    //orderReq.PrintDebugString();
                    clientApi->createAlgoInstance(orderReq);

                    std::scoped_lock lock(g_mutex);
                    auto t = system_clock::now();

                    pingpong.insert({ orderReq.request_id(),std::pair(t,t) });

                }
            }
        
        std::this_thread::sleep_for(std::chrono::seconds(5*i));
    }

    std::this_thread::sleep_for(std::chrono::seconds(40));

    for (auto& algoOrderId : testCancelOrders) {

        AlgoMsg::MsgAlgoInstanceUpdateRequest updateReq;
        updateReq.set_request_id(RequestIdGenerator::getInstance().NewId());
        updateReq.set_acct(acct);
        updateReq.set_acct_type(acctType);
        updateReq.set_algo_order_id(algoOrderId);
        updateReq.set_action(AlgoMsg::ACTION_STOP);
        clientApi->updateAlgoInstance(updateReq);
        updateReq.DebugString();
    }

    SPDLOG_WARN(" type any key to continue...");

    char input;
    std::cin >> input;

    clientApi->release();

    std::this_thread::sleep_for(std::chrono::seconds(1));


    return 0;
}