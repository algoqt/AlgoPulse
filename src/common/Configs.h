#pragma once

#include <unordered_map>
#include <map>
#include "typedefs.h"
#include "common.h"

namespace agcommon {

    class Configs {
    public:
        static Configs& getConfigs() {

            static Configs instance;

            return instance;
        }

        ~Configs() = default;

        std::string getTickH5Dir() { return m_h5FileDir; };

        std::filesystem::path getSecurityInfoFile() { return m_SecurityInfoH5File; };

        std::filesystem::path getTradeCalendarFile() { return m_TradeCalendarH5File; }

        std::filesystem::path getDailyBarFile() { return m_DailyBarH5File; }

        std::filesystem::path getMinuteBarFile() { return m_MinuteBarH5File; }

        const std::unordered_map<std::string, std::string>& getAlgoParamMap (
            const AlgoMsg::MsgAlgoCategory& category);

        Broker_t getBrokerIdFromAcct(const AlgoMsg::AcctType& acctType, const Acct_t& acct);

        inline std::string getConfigOrDefault(
            const std::string& section,
            const std::string& key,
            const std::string& defaultValue) const {

            if (auto it = m_config.find(section); it != m_config.end()) {
                auto& dict = it->second;
                return dict.contains(section) ? dict.at(key) : defaultValue;
            }
            return defaultValue;
        }

        inline double getConfigOrDefault(
            const std::string& section,
            const std::string& key,
            const double defaultValue) const {

            if (auto it = m_config.find(section); it != m_config.end()) {
                auto& dict = it->second;
                return dict.contains(section) ? agcommon::get_double(dict.at(key), defaultValue) : defaultValue;
            }
            return defaultValue;
        }
        inline int32_t getConfigOrDefault(
            const std::string& section,
            const std::string& key,
            const int32_t defaultValue) const {

            if (auto it = m_config.find(section); it != m_config.end()) {
                auto& dict = it->second;
                return dict.contains(section) ? agcommon::get_int(dict.at(key), defaultValue) : defaultValue;
            }
            return defaultValue;
        }

    private:

        Configs();

        std::unordered_map<std::string, std::unordered_map<std::string, std::string>>  m_config{};

        std::string m_h5FileDir;
        std::filesystem::path m_SecurityInfoH5File;
        std::filesystem::path m_TradeCalendarH5File;
        std::filesystem::path m_DailyBarH5File;
        std::filesystem::path m_MinuteBarH5File;

        std::mutex   m_mutex;

    private:

        std::map<AlgoMsg::AcctType, std::unordered_map<Acct_t, Broker_t>>  acct2Broker;
    };
}