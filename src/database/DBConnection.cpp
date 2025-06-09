#include "database/DBConnection.hpp"

namespace vh::database {
    DBConnection::DBConnection() {
        auto config = DBConfig();
        conn_ = std::make_unique<pqxx::connection>(config.connectionString());
    }

    DBConnection::~DBConnection() {
        if (conn_ && conn_->is_open()) conn_->close();
    }

    pqxx::connection& DBConnection::get() {
        return *conn_;
    }
}
