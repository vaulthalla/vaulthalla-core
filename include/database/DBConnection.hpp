#pragma once
#include <pqxx/connection>

namespace vh::database {
    class DBConnection {
    public:
        DBConnection() : conn_(std::make_unique<pqxx::connection>(DB_CONNECTION_STR)) {
            if (!conn_->is_open()) throw std::runtime_error("Failed to connect to database: " + DB_CONNECTION_STR);
        }
        ~DBConnection() { if (conn_ && conn_->is_open()) conn_->close(); }

        pqxx::connection& get() {
            if (!conn_ || !conn_->is_open()) throw std::runtime_error("Database connection is not open");
            return *conn_;
        }

    private:
        std::unique_ptr<pqxx::connection> conn_;

        const std::string DB_CONNECTION_STR = std::getenv("VAULTHALLA_DB_CONNECTION_STR");
    };
}
