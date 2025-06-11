#include "database/DBConnection.hpp"
#include <iostream>

namespace vh::database {

    DBConnection::DBConnection() { conn_ = std::make_unique<pqxx::connection>(DB_CONNECTION_STR); }

    DBConnection::~DBConnection() { if (conn_ && conn_->is_open()) conn_->close(); }

    pqxx::connection& DBConnection::get() { return *conn_; }
}
