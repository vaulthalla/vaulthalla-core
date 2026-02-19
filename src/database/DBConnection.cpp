#include "database/DBConnection.hpp"
#include "config/Config.hpp"
#include "config/ConfigRegistry.hpp"
#include "crypto/TPMKeyProvider.hpp"
#include "crypto/PasswordUtils.hpp"
#include "logging/LogRegistry.hpp"

#include <fstream>
#include <paths.h>

using namespace vh::crypto;
using namespace vh::logging;

namespace vh::database {

static std::optional<std::string> getFirstInitDBPass() {
    const std::filesystem::path f{"/run/vaulthalla/db_password"};

    if (!std::filesystem::exists(f)) {
        LogRegistry::vaulthalla()->debug("[seed] No pending db_password file at {}", f.string());
        return std::nullopt;
    }

    std::ifstream in(f);
    if (!in.is_open()) {
        LogRegistry::vaulthalla()->warn("[seed] Failed to open db_password file: {}", f.string());
        return std::nullopt;
    }

    std::string pass{};
    if (!(in >> pass)) {
        LogRegistry::vaulthalla()->warn("[seed] Invalid contents in {}", f.string());
        return std::nullopt;
    }

    try {
        std::filesystem::remove(f);
        LogRegistry::vaulthalla()->debug("[seed] Consumed and removed pending db_password file {}", f.string());
    } catch (const std::exception& e) {
        LogRegistry::vaulthalla()->warn("[seed] Failed to remove {}: {}", f.string(), e.what());
    }

    return pass;
}

DBConnection::DBConnection() : tpmKeyProvider_(
    std::make_unique<TPMKeyProvider>(paths::testMode ? "test_psql" : "psql")) {
    if (paths::testMode) {
        const auto user = std::getenv("VH_TEST_DB_USER");
        const auto pass = std::getenv("VH_TEST_DB_PASS");
        const auto host = std::getenv("VH_TEST_DB_HOST");
        const auto port = std::getenv("VH_TEST_DB_PORT");
        const auto name = std::getenv("VH_TEST_DB_NAME");
        if (user && pass && host && port && name) {
            DB_CONNECTION_STR = "postgresql://" + std::string(user) + ":" + std::string(pass) + "@" + std::string(host)
                                + ":" + std::string(port) + "/" + std::string(name);
            conn_ = std::make_unique<pqxx::connection>(DB_CONNECTION_STR);
            LogRegistry::vaulthalla()->info(
                "[DBConnection] Test mode: using connection string from environment variables");
            return;
        }
    }

    if (tpmKeyProvider_->sealedExists()) tpmKeyProvider_->init();
    else {
        const auto initPass = getFirstInitDBPass();
        if (!initPass) {
            if (std::filesystem::exists("/run/vaulthalla/db_password")) {
                std::filesystem::remove_all("/run/vaulthalla/db_password");
                if (std::filesystem::exists("/run/vaulthalla/db_password"))
                    throw std::runtime_error(
                        "Failed to remove stale /run/vaulthalla/db_password file");
            }
            throw std::runtime_error("Database password failed to initialize. See logs for details.");
        }
        tpmKeyProvider_->init(*initPass);
    }

    const auto& key = tpmKeyProvider_->getMasterKey();
    const auto password = auth::PasswordUtils::escape_uri_component({key.begin(), key.end()});

    const auto& config = config::ConfigRegistry::get();
    const auto db = config.database;
    DB_CONNECTION_STR = "postgresql://" + db.user + ":" + password + "@" + db.host + ":" + std::to_string(db.port) + "/"
                        + db.name;
    conn_ = std::make_unique<pqxx::connection>(DB_CONNECTION_STR);
}

DBConnection::~DBConnection() { if (conn_ && conn_->is_open()) conn_->close(); }

pqxx::connection& DBConnection::get() const { return *conn_; }

void DBConnection::initPrepared() const {
    if (!conn_ || !conn_->is_open()) throw std::runtime_error("Database connection is not open");

    // Auth
    initPreparedUsers();
    initPreparedGroups();

    // RBAC
    initPreparedRoles();
    initPreparedUserRoles();
    initPreparedVaultRoles();
    initPreparedPermOverrides();
    initPreparedPermissions();

    // Vaults
    initPreparedVaults();
    initPreparedVaultKeys();
    initPreparedAPIKeys();

    // Sync
    initPreparedSync();
    initPreparedSyncEvents();
    initPreparedSyncThroughput();
    initPreparedSyncConflicts();
    initPreparedSyncConflictArtifacts();

    // Filesystem
    initPreparedFsEntries();
    initPreparedFiles();
    initPreparedDirectories();
    initPreparedOperations();
    initPreparedCache();

    // Admin
    initPreparedSecrets();
    initPreparedWaivers();
}

}
