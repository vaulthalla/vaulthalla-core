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
    void initPreparedVaults() const;
    void initPreparedFsEntries() const;
    void initPreparedFiles() const;
    void initPreparedDirectories() const;
    void initPreparedOperations() const;
    void initPreparedRoles() const;
    void initPreparedPermissions() const;
    void initPreparedUserRoles() const;
    void initPreparedVaultRoles() const;
    void initPreparedPermOverrides() const;
    void initPreparedSync() const;
    void initPreparedCache() const;
};

struct PathPatterns {
    std::string like;
    std::string not_like;
};

inline PathPatterns computePatterns(const std::string& absPath, const bool recursive) {
    const std::string base = (absPath == "/") ? "" : absPath;
    if (recursive) return {base + "/%", ""};
    return {base + "/%", base + "/%/%"};
}

} // namespace vh::database
