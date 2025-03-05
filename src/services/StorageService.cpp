#include "StorageService.h"
#include <chrono>
#include <iomanip>
#include <sstream>

static uint64_t getCurrentTimeAsInteger() {

    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);

    std::tm local_time = *std::localtime(&now_time_t);

    std::ostringstream oss;
    oss << std::put_time(&local_time, "%Y%m%d%H%M%S");

    uint64_t result = std::stoull(oss.str());

    return result;
}

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
        CREATE TABLE IF NOT EXISTS MsgAlgoInstanceCreateRequest (
            create_time INTEGER,
            request_id  INTEGER NOT NULL,
            acct_type INTEGER NOT NULL,
            acct text NOT NULL,
            vendor_id INTEGER NOT NULL,
            algo_strategy INTEGER NOT NULL,
            trade_time_beg INTEGER ,
            trade_time_end INTEGER ,
            obj_data BLOB,

            PRIMARY KEY (create_time,request_id)
        );
    )";

    char* errmsg = nullptr;
    if (sqlite3_exec(db, createTableSQL.data(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
        SPDLOG_ERROR("Error creating table:{}", errmsg);
        sqlite3_free(errmsg);
        //throw std::runtime_error("Table creation failed");
    }

    createTableSQL.clear();
    createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS MsgShotSignalInfo (
            algo_order_id INTEGER NOT NULL,
            signal_id INTEGER NOT NULL,
            obj_data BLOB,
            last_update_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY (algo_order_id, signal_id)
        );
    )";

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
            obj_data      BLOB,
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

void StorageService::storeMessage(const std::shared_ptr<AlgoMsg::MsgShotSignalInfo>& msg) {

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

    auto bytes = msg->ByteSizeLong();
    if (tmpCache.capacity() < bytes) {
        tmpCache.reserve(bytes);
    }

    if (msg->SerializePartialToArray(tmpCache.data(), bytes)) {

        sqlite3_bind_blob(stmt, 3, tmpCache.data(), bytes, SQLITE_STATIC);
    }
   
    if (sqlite3_step(stmt) != SQLITE_DONE) {

        sqlite3_finalize(stmt);
        SPDLOG_ERROR("Failed to execute statement:{}", sqlite3_errmsg(m_db));
    }

    sqlite3_finalize(stmt);

};

void StorageService::storeMessage(const std::shared_ptr<AlgoMsg::MsgShotPerformance>& msg) {

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

    auto bytes = msg->ByteSizeLong();
    if (tmpCache.capacity() < bytes) {
        tmpCache.reserve(bytes);
    }

    if (msg->SerializePartialToArray(tmpCache.data(), bytes)) {
        sqlite3_bind_blob(stmt, 5, tmpCache.data(), bytes, SQLITE_STATIC);
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {

        sqlite3_finalize(stmt);
        SPDLOG_ERROR("Failed to execute statement:{}",sqlite3_errmsg(m_db));
    }
    sqlite3_finalize(stmt);

};

void StorageService::storeMessage(const std::shared_ptr<AlgoMsg::MsgAlgoPerformance>& msg) {

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

    auto bytes = msg->ByteSizeLong();
    if (tmpCache.capacity() < bytes) {
        tmpCache.reserve(bytes);
    }

    if (msg->SerializePartialToArray(tmpCache.data(), bytes)) {

        sqlite3_bind_blob(stmt, 5, tmpCache.data(), bytes, SQLITE_STATIC);
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        SPDLOG_ERROR("Failed to execute statement:{}", sqlite3_errmsg(m_db));
    }
    sqlite3_finalize(stmt);

};


void StorageService::storeMessage(const std::shared_ptr<AlgoMsg::MsgAlgoInstanceCreateRequest>& msg) {

    const char* insertOrReplaceSQL = R"(
        INSERT OR REPLACE INTO MsgAlgoInstanceCreateRequest (
            create_time,
            request_id,
            acct_type,
            acct,
            vendor_id,
            algo_strategy,
            trade_time_beg,
            trade_time_end,
            obj_data
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, insertOrReplaceSQL, -1, &stmt, nullptr) != SQLITE_OK) {

        SPDLOG_ERROR("Failed to prepare statement:{}", sqlite3_errmsg(m_db));
    }

    sqlite3_bind_int64(stmt, 1, getCurrentTimeAsInteger());

    sqlite3_bind_int64(stmt, 2, msg->request_id());

    sqlite3_bind_int(stmt, 3, msg->acct_type());

    sqlite3_bind_text(stmt, 4, msg->acct().data(), msg->acct().size(), SQLITE_STATIC);

    sqlite3_bind_int(stmt, 5, msg->vendor_id());

    sqlite3_bind_int(stmt, 6, msg->algo_strategy());

    sqlite3_bind_int64(stmt, 7, msg->algo_order().start_time());

    sqlite3_bind_int64(stmt, 8, msg->algo_order().end_time());

    auto bytes = msg->ByteSizeLong();
    if (tmpCache.capacity() < bytes) {
        tmpCache.reserve(bytes);
    }
    
    if (msg->SerializePartialToArray(tmpCache.data(), bytes)) {

        sqlite3_bind_blob(stmt, 9, tmpCache.data(), bytes, SQLITE_STATIC);
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        SPDLOG_ERROR("Failed to execute statement:{}", sqlite3_errmsg(m_db));
    }
    sqlite3_finalize(stmt);
};

std::string buildQuery(const std::optional<int64_t>& create_time,
    const std::optional<int>& acct_type,
    const std::optional<std::string>& acct,
    const std::optional<int64_t>& beg_time,
    const std::optional<int64_t>& end_time) {
    std::string sql = "SELECT create_time, request_id, acct_type, acct FROM MsgAlgoInstanceCreateRequest";
    std::vector<std::string> conditions;
    std::vector<const void*> params;
    int paramIndex = 1;

    if (create_time.has_value()) {
        conditions.push_back("create_time = ?");
        params.push_back(&(*create_time));
        ++paramIndex;
    }
    if (acct_type.has_value()) {
        conditions.push_back("acct_type = ?");
        params.push_back(&(*acct_type));
        ++paramIndex;
    }
    if (acct.has_value()) {
        conditions.push_back("acct = ?");
        params.push_back(acct->c_str());
        ++paramIndex;
    }
    if (beg_time.has_value()) {
        conditions.push_back("trade_time_beg >= ?");
        params.push_back(&(*beg_time));
        ++paramIndex;
    }
    if (end_time.has_value()) {
        conditions.push_back("trade_time_end <= ?");
        params.push_back(&(*end_time));
        ++paramIndex;
    }

    if (!conditions.empty()) {
        sql += " WHERE " + conditions[0];
        for (size_t i = 1; i < conditions.size(); ++i) {
            sql += " AND " + conditions[i];
        }
    }

    return sql;
}

void queryMessages(sqlite3* db,
    const std::optional<int64_t>&   create_time,
    const std::optional<int>&       acct_type,
    const std::optional<std::string>& acct,
    const std::optional<int64_t>& beg_time,
    const std::optional<int64_t>& end_time) {

    auto sql = buildQuery(create_time, acct_type, acct, beg_time, end_time);
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    int index = 1;
    if (create_time.has_value()) sqlite3_bind_int64(stmt, index++, *create_time);
    if (acct_type.has_value()) sqlite3_bind_int(stmt, index++, *acct_type);
    if (acct.has_value()) sqlite3_bind_text(stmt, index++, acct->c_str(), -1, SQLITE_STATIC);
    if (beg_time.has_value()) sqlite3_bind_int64(stmt, index++, *beg_time);
    if (end_time.has_value()) sqlite3_bind_int64(stmt, index++, *end_time);

    //while (sqlite3_step(stmt) == SQLITE_ROW) {
    //    QueryResult result;
    //    result.create_time = sqlite3_column_int64(stmt, 0);
    //    result.request_id = sqlite3_column_int64(stmt, 1);
    //    result.acct_type = sqlite3_column_int(stmt, 2);
    //    result.acct.assign(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
    //}

    sqlite3_finalize(stmt);
}