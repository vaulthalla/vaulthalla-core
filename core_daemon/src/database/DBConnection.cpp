#include "database/DBConnection.hpp"
#include "types/config/Config.hpp"
#include "types/config/ConfigRegistry.hpp"

namespace vh::database {

DBConnection::DBConnection() {
    const auto& config = types::config::ConfigRegistry::get();
    const auto db = config.database;
    DB_CONNECTION_STR = "postgresql://" + db.user + ":" + db.password + "@" + db.host + ":" + std::to_string(db.port) + "/" + db.name;
    conn_ = std::make_unique<pqxx::connection>(DB_CONNECTION_STR);
}

DBConnection::~DBConnection() {
    if (conn_ && conn_->is_open()) conn_->close();
}

pqxx::connection& DBConnection::get() {
    return *conn_;
}
} // namespace vh::database
