#include "Configs.h"
#include "common.h"

static std::string trim(const std::string& str) {
    size_t start = 0;
    size_t end = str.size();

    while (start < end && std::isspace(static_cast<unsigned char>(str[start]))) {
        start++;
    }

    while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        end--;
    }

    return str.substr(start, end - start);
}

static std::unordered_map<std::string, std::unordered_map<std::string, std::string>> parseINI(const std::string& filename) {
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> config;
    std::string line;
    std::string currentSection;

    if (std::filesystem::exists(filename)) {

        std::ifstream file(filename);

        while (std::getline(file, line)) {
            line = trim(line);
            if (line.empty() || line[0] == ';' || line[0] == '#') {
                continue;
            }
            if (line[0] == '[') {
                currentSection = line.substr(1, line.find(']') - 1);  // section
            }
            else {
                size_t delimiterPos = line.find('=');
                if (delimiterPos != std::string::npos) {
                    std::string key   = trim(line.substr(0, delimiterPos));
                    std::string value = trim(line.substr(delimiterPos + 1));
                    config[currentSection][key] = value;
                    SPDLOG_INFO("READ {},[{}] {}={}", filename,currentSection, key, value);
                }
            }
        }
    }
    return config;
}

namespace agcommon {

    Configs::Configs() {

        std::scoped_lock lock(m_mutex);

        std::string configFileName = "../config/config.ini";

        if (std::filesystem::exists(configFileName)) {

            m_config = parseINI(configFileName);
        }
        else {
            SPDLOG_ERROR("config File Dose'not EXISTS:{}", configFileName);

            exit(1);
        }

        const auto dataSectionName = "DATACONFIG";
#if(_WIN32)
        auto fileDir = "H5FileDir";
#else
        auto fileDir = "H5FileDir_WSL";
#endif
        if (auto it = m_config.find(dataSectionName); it != m_config.end()) {

            m_h5FileDir = it->second.contains(fileDir) ? it->second.at(fileDir) : "D:/gmData/";

            if (not m_h5FileDir.empty() and m_h5FileDir[m_h5FileDir.size() - 1] != '/') {
                m_h5FileDir.append("/");
            }

            m_SecurityInfoH5File  = std::filesystem::path(m_h5FileDir + getConfigOrDefault(dataSectionName,"SecurityInfoFileName","StaticInfo.h5"));

            m_TradeCalendarH5File = std::filesystem::path(m_h5FileDir + getConfigOrDefault(dataSectionName,"TradeCalendarFileName", "TradeCalendar.h5"));

            m_DailyBarH5File      = std::filesystem::path(m_h5FileDir + getConfigOrDefault(dataSectionName,"DailyFileName", "DailyBar.h5"));

            m_MinuteBarH5File     = std::filesystem::path(m_h5FileDir + getConfigOrDefault(dataSectionName,"MinuteFileName", "MinuteBar.h5"));

            SPDLOG_INFO("set m_h5FileDir={}", m_h5FileDir);
            SPDLOG_INFO("set m_StockH5File={}", m_SecurityInfoH5File.string());
            SPDLOG_INFO("set m_TradeCalendarH5File={}", m_TradeCalendarH5File.string());
            SPDLOG_INFO("set m_DailyBarH5File={}", m_DailyBarH5File.string());
            SPDLOG_INFO("set m_MinuteBarH5File={}", m_MinuteBarH5File.string());

        }

        if (not std::filesystem::is_directory(m_h5FileDir)) {
            SPDLOG_ERROR("m_tickH5Dir DOESNOT EXISTS:{}", m_h5FileDir);
        }
        if (not std::filesystem::exists(m_SecurityInfoH5File)) {
            SPDLOG_ERROR("m_StockH5File DOESNOT EXISTS:{}", m_SecurityInfoH5File.string());
        }
        if (not std::filesystem::exists(m_TradeCalendarH5File)) {
            SPDLOG_ERROR("m_TradeCalendarH5File DOESNOT EXISTS:{}", m_TradeCalendarH5File.string());
        }
        if (not std::filesystem::exists(m_DailyBarH5File)) {
            SPDLOG_ERROR("m_DailyBarH5File DOESNOT EXISTS:{}", m_DailyBarH5File.string());
        }
        if (not std::filesystem::exists(m_MinuteBarH5File)) {
            SPDLOG_ERROR("m_MinuteBarH5File DOESNOT EXISTS:{}", m_MinuteBarH5File.string());
        }

        for (const auto& [section, map] : m_config) {

            if (section.starts_with("BrokerConfig") and section.find_first_of('_')!=section.npos) {

                auto brokerId = section.substr(section.find_first_of('_')+1);

                if (agcommon::get_int(brokerId, 0) != 0) {
                    for (const auto& [acct, acctTypeName]: map) {
                        AlgoMsg::AcctType acctType;

                        auto isOk = AlgoMsg::AcctType_Parse(acctTypeName, &acctType);
                        if (isOk) {
                            acct2Broker[acctType].try_emplace(acct, agcommon::get_int(brokerId, 0));
                            SPDLOG_INFO("Parse broker:{},acct:{},acctType:{} successful.", brokerId, acct, acctType);
                        }
                        else {
                            SPDLOG_INFO("Parse broker:{},acct:{},acctType:{} failed.", brokerId, acct, acctType);
                        }
                    }
                }
            }
        }

    }

    Broker_t Configs::getBrokerIdFromAcct(const AlgoMsg::AcctType& acctType, const Acct_t& acct) {

        std::scoped_lock lock(m_mutex);

        if (auto it = acct2Broker.find(acctType); it != acct2Broker.end()) {
            if (auto it_it = it->second.find(acct); it_it != it->second.end())
                return it_it->second;
        }
        return 0;
    }

    const std::unordered_map<std::string, std::string>& Configs::getAlgoParamMap(
        const AlgoMsg::MsgAlgoCategory& category){

        std::scoped_lock lock(m_mutex);
        auto& categoryName = AlgoMsg::MsgAlgoCategory_Name(category);

        return m_config[categoryName];
    }

}