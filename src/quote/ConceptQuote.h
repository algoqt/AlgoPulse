#pragma once

#include "MarketDepth.h"
#include <set>
#include "Configs.h"

struct StockConcept {
    std::string conceptCode;
    std::string conceptName;
    std::string stockCode;
    std::string stockName;
};

class ConceptQuote{

public:

    static ConceptQuote& getInstance() {

        static ConceptQuote instance{};

        return instance;
    }


    void onMarketDepth(const MarketDepth* md) {

        std::scoped_lock  lock(m_mutex);

        m_symbol2MdPtr[md->symbol()] = MarketDepthKeepAlivePtr(md);

    };

    void calConceptReturn() {

        std::scoped_lock  lock(m_mutex);

        if (m_symbol2MdPtr.size() == 0)
            return;

        for (const auto& [cpKey,cpName_symbolSet]: m_conceptInfo) {

            double totalRet = 0.0;
            uint32_t cnt = 0;
            for (const auto& symbol : cpName_symbolSet.second) {

                if (auto it = m_symbol2MdPtr.find(symbol); it != m_symbol2MdPtr.end()) {
                    auto& md = it->second;
                    if (md->quoteTime > lastQuoteTime) {
                        lastQuoteTime = md->quoteTime;
                    }
                    totalRet += md->changeP;
                    cnt++;
                }
            }

            m_conceptReturn[cpKey] = cnt > 0 ? totalRet / cnt : 0.0;
        }
    };

    std::shared_ptr<AlgoMsg::MsgStockConceptQuotes> encodeConceptQuotes() {

        calConceptReturn();

        auto msgPtr = std::make_shared<AlgoMsg::MsgStockConceptQuotes>();

        std::scoped_lock  lock(m_mutex);

        for (const auto& pair : m_conceptReturn) {
            auto quote = msgPtr->add_quote_array();
            quote->set_quote_time(agcommon::getDateTimeInt(lastQuoteTime));
            quote->set_concept_id(pair.first);
            quote->set_return_rate(pair.second);
        }

        return msgPtr;
    }

    std::shared_ptr<AlgoMsg::MsgStockConceptInfos> encodeConceptInfos() {

        auto msgPtr = std::make_shared<AlgoMsg::MsgStockConceptInfos>();

        std::scoped_lock  lock(m_mutex);

        for (const auto& [cpKey,pair] : m_conceptInfo) {
            auto item = msgPtr->add_concept_array();
            item->set_concept_id(cpKey);
            item->set_concept_name(pair.first);
            for (const auto& symbol : pair.second) {
                item->add_symbols(symbol);
            }
        }

        return msgPtr;
    }


private:

    ConceptQuote() :lastQuoteTime{agcommon::now()} {

        std::scoped_lock  lock(m_mutex);

        auto filename = agcommon::Configs::getConfigs().getConfigOrDefault("DATACONFIG", "ConceptInfoFileName", "concept");
        if (!std::filesystem::exists(filename)) {
            SPDLOG_WARN("Error: File {} does not exist!" , filename);
            return;
        }

        std::ifstream file(filename);
        if (!file.is_open()) {
            SPDLOG_ERROR("Error: Failed to open file {} !", filename);
            return;
        }

        std::string line;
        bool is_first_line = true;

        while (std::getline(file, line)) {
            if (is_first_line) {
                is_first_line = false;
                continue;
            }

            std::stringstream ss(line);
            StockConcept record;

            std::getline(ss, record.conceptCode, ',');
            std::getline(ss, record.conceptName, ',');
            std::getline(ss, record.stockCode, ',');
            std::getline(ss, record.stockName, ',');

            if (not record.conceptCode.empty()) {
                auto& conceptInfo = m_conceptInfo[record.conceptCode];
                conceptInfo.first = record.conceptName;
                conceptInfo.second.emplace(record.stockCode);
            }
        }

        file.close();
        SPDLOG_INFO("read total {} concept info", m_conceptInfo.size());
    }
    ~ConceptQuote() = default;

    std::mutex m_mutex;

    using ConceptKey = std::string;

    std::unordered_map<Symbol_t, MarketDepthKeepAlivePtr>               m_symbol2MdPtr{};

    std::unordered_map<ConceptKey, std::pair<std::string, std::unordered_set<Symbol_t>>>  m_conceptInfo{};

    //std::unordered_map<Symbol_t, std::unordered_set<ConceptKey>>       m_symbol2Concepts;

    std::unordered_map<ConceptKey, double>       m_conceptReturn{};

    QuoteTime_t  lastQuoteTime;

};
