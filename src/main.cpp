
#include <spdlog/spdlog.h>
#include "AlgoService.h"
#include <spdlog/async.h>
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include <csignal>
#include "AlgoService.h"
//#include <crtdbg.h> //_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

#if OUTPUT_LOG_TO_STD_OUT
auto logPattern = "[%Y-%m-%d %H:%M:%S.%e][%P:%5t][%^%l%$][%s:%3#]%v";
#else 
auto logPattern = "[%Y-%m-%d %H:%M:%S.%e][%P:%5t][%^%l%$]%v";
#endif 

bool running = true;

static void signalHandler(int signum) {

    SPDLOG_WARN("Received signal: {} .Exiting...",signum);

    running = false;
}

int main() {
        
    signal(SIGINT, signalHandler); 

    signal(SIGTERM, signalHandler);

    spdlog::init_thread_pool(500 * 1024, 1);

    auto logFile = fmt::format("../logs/log_{}.txt", agcommon::getDateTimeStr(agcommon::now()));

    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_st>(logFile, true);

#if OUTPUT_LOG_TO_STD_OUT
    auto consoleSink = std::make_shared<spdlog::sinks::stderr_color_sink_st>();
    spdlog::sinks_init_list sinks{ fileSink, consoleSink };
#else
    spdlog::sinks_init_list sinks{ fileSink };
#endif

    auto algo_logger = std::make_shared<spdlog::async_logger>("multi_sink_logger", sinks.begin(), sinks.end(), spdlog::thread_pool());

    algo_logger->set_level(spdlog::level::debug);

    algo_logger->set_pattern(logPattern);

    spdlog::set_default_logger(algo_logger);
    spdlog::flush_on(spdlog::level::err);
    spdlog::flush_every(std::chrono::seconds(3));

    try {
        #if ENABLE_DELAY_STATS
               SPDLOG_INFO("ENABLE_DELAY_STATS IS ON");
        #else
               SPDLOG_INFO("ENABLE_DELAY_STATS IS OFF");
        #endif
        #if OUTPUT_LOG_TO_STD_OUT
               SPDLOG_INFO("OUTPUT_LOG_TO_STD_OUT IS ON");
        #else
               SPDLOG_INFO("OUTPUT_LOG_TO_STD_OUT IS OFF");
        #endif

        SPDLOG_INFO("SPDLOG_ACTIVE_LEVEL IS {}", SPDLOG_ACTIVE_LEVEL);

        AlgoService& asr = AlgoService::getInstance();

        while (running) {

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        }
    }
    catch (std::exception& e) {
        SPDLOG_ERROR("START ALGO SERVICE Failed:{}", e.what());
    }

    SPDLOG_INFO("service exit");

    return 0;
}