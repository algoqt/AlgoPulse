#include "StorageService.h"


StorageService::StorageService(): 
    m_runContextPtr(ContextService::getInstance().createContext("StorageService", 1))
    ,m_db(nullptr) {

    if (sqlite3_open("algoStorage.db", &m_db) != SQLITE_OK) {

        SPDLOG_ERROR("Error opening database: {}",sqlite3_errmsg(m_db));

        return;
    }

    createTable(m_db);
}

StorageService::~StorageService() { 

    ContextService::getInstance().stopContext("StorageService");

    if (m_db) {
        sqlite3_close(m_db);
    }
};


void StorageService::createTable(sqlite3* db) {

    std::string createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS MsgShotSignalInfo (
            algo_order_id INTEGER NOT NULL,
            signal_id INTEGER NOT NULL,
            obj_data BLOB,
            last_update_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY (algo_order_id, signal_id)
        );
    )";

    char* errmsg = nullptr;
    if (sqlite3_exec(db, createTableSQL.data(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
        SPDLOG_ERROR("Error creating table:{}", errmsg);
        sqlite3_free(errmsg);
        throw std::runtime_error("Table creation failed");
    }

    createTableSQL.clear();

    createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS MsgAlgoPerformance (
            trade_date INTEGER,
            algo_order_id INTEGER,
            algo_category INTEGER,
            algo_strategy INTEGER,
            obj_data BLOB,
            last_update_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY (trade_date,algo_order_id)
        );
    )";

    if (sqlite3_exec(db, createTableSQL.data(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
        SPDLOG_ERROR("Error creating table:{}", errmsg);
        sqlite3_free(errmsg);
        throw std::runtime_error("Table creation failed");
    }

    createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS MsgOrder (
            algo_order_id   INTEGER,
            order_id        INTEGER,
            obj_data        BLOB,
            last_update_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY (algo_order_id,order_id)
        );
    )";

    if (sqlite3_exec(db, createTableSQL.data(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
        SPDLOG_ERROR("Error creating table:{}", errmsg);
        sqlite3_free(errmsg);
        throw std::runtime_error("Table creation failed");
    }

    createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS MsgTrade (
            order_id        INTEGER,
            trade_id        INTEGER,
            obj_data        BLOB,
            last_update_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY (order_id,trade_id)
        );
    )";

    if (sqlite3_exec(db, createTableSQL.data(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
        SPDLOG_ERROR("Error creating table:{}", errmsg);
        sqlite3_free(errmsg);
        throw std::runtime_error("Table creation failed");
    }
}

void StorageService::storeMessage(const std::shared_ptr<AlgoMsg::MsgShotSignalInfo> msg) {

    const char* insert_sql = R"(
        INSERT OR REPLACE INTO MsgShotSignalInfo (
            algo_order_id,
            signal_id,
            obj_data
        ) VALUES (?, ?, ?);
    )";

    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, insert_sql, -1, &stmt, nullptr) != SQLITE_OK) {

        SPDLOG_ERROR("Failed to prepare statement:{}", sqlite3_errmsg(m_db));
    }

    sqlite3_bind_int64(stmt, 1, msg->algo_order_id());

    sqlite3_bind_int64(stmt, 2, msg->signal_id());

    std::string msgstr;

    if (msg->SerializeToString(&msgstr)) {
        //SPDLOG_INFO("{}", msgstr);
        sqlite3_bind_blob(stmt, 3, msgstr.data(), msgstr.size(), SQLITE_TRANSIENT);
    }
   
    if (sqlite3_step(stmt) != SQLITE_DONE) {

        sqlite3_finalize(stmt);
        SPDLOG_ERROR("Failed to execute statement:{}", sqlite3_errmsg(m_db));
    }

    sqlite3_finalize(stmt);

};

void StorageService::storeMessage(const std::shared_ptr<AlgoMsg::MsgShotPerformance> msg) {

    const char* insertOrReplaceSQL = R"(
        INSERT OR REPLACE INTO MsgAlgoPerformance (
            trade_date,
            algo_order_id,
            algo_category,
            algo_strategy,
            obj_data
        )
        VALUES (?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, insertOrReplaceSQL, -1, &stmt, nullptr) != SQLITE_OK) {
        SPDLOG_ERROR("Failed to prepare statement:{}" , sqlite3_errmsg(m_db));
    }

    sqlite3_bind_int(stmt, 1, msg->trade_date());

    sqlite3_bind_int64(stmt, 2, msg->algo_order_id());

    sqlite3_bind_int(stmt, 3, AlgoMsg::MsgAlgoCategory::Category_SHOT);

    sqlite3_bind_int(stmt, 4, msg->algo_strategy());

    std::string msgstr;

    if (msg->SerializeToString(&msgstr)) {
        //SPDLOG_INFO("{}", msgstr);
        sqlite3_bind_blob(stmt, 5, msgstr.data(), msgstr.size(), SQLITE_TRANSIENT);
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {

        sqlite3_finalize(stmt);
        SPDLOG_ERROR("Failed to execute statement:{}",sqlite3_errmsg(m_db));
    }
    sqlite3_finalize(stmt);

};

void StorageService::storeMessage(const std::shared_ptr<AlgoMsg::MsgAlgoPerformance> msg) {

    const char* insertOrReplaceSQL = R"(
        INSERT OR REPLACE INTO MsgAlgoPerformance (
            trade_date,
            algo_order_id,
            algo_category,
            algo_strategy,
            obj_data
        )
        VALUES (?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, insertOrReplaceSQL, -1, &stmt, nullptr) != SQLITE_OK) {

        SPDLOG_ERROR("Failed to prepare statement:{}", sqlite3_errmsg(m_db));
    }

    sqlite3_bind_int(stmt, 1, msg->start_time() / 1000000);

    sqlite3_bind_int64(stmt, 2, msg->algo_order_id());

    sqlite3_bind_int(stmt, 3, AlgoMsg::MsgAlgoCategory::Category_ALGO);

    sqlite3_bind_int(stmt, 4, msg->algo_strategy());

    std::string msgstr;

    if (msg->SerializeToString(&msgstr)) {

        sqlite3_bind_blob(stmt, 5, msgstr.data(), msgstr.size(), SQLITE_TRANSIENT);
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        SPDLOG_ERROR("Failed to execute statement:{}", sqlite3_errmsg(m_db));
    }
    sqlite3_finalize(stmt);

};