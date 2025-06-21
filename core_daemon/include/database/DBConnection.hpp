#pragma once
#include <pqxx/connection>

namespace vh::database {
class DBConnection {
  public:
    DBConnection();
    ~DBConnection();

    pqxx::connection& get();

  private:
    std::string DB_CONNECTION_STR;
    std::unique_ptr<pqxx::connection> conn_;
};
} // namespace vh::database
