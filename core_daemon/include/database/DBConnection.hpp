#pragma once
#include <pqxx/connection>

namespace vh::database {
class DBConnection {
  public:
    DBConnection();
    ~DBConnection();

    [[nodiscard]] pqxx::connection& get() const;

  private:
    std::string DB_CONNECTION_STR;
    std::unique_ptr<pqxx::connection> conn_;

    void initPrepared() const;
    void initPreparedUsers() const;
    void initPreparedFiles() const;
    void initPreparedDirectories() const;
    void initPreparedRoles() const;
    void initPreparedPermissions() const;
    void initPreparedUserRoles() const;
    void initPreparedVaultRoles() const;
    void initPreparedPermOverrides() const;
};
} // namespace vh::database
