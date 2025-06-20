#pragma once
#include <pqxx/connection>

namespace vh::database {
class DBConnection {
  public:
    DBConnection();
    ~DBConnection();

    pqxx::connection& get();

  private:
    std::unique_ptr<pqxx::connection> conn_;

    const std::string DB_CONNECTION_STR = std::getenv("VAULTHALLA_DB_CONNECTION_STR");
};
} // namespace vh::database
