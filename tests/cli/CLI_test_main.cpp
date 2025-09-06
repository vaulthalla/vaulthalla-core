#include "CLITestRunner.hpp"
#include "UsageManager.hpp"
#include "protocols/shell/Router.hpp"
#include "protocols/shell/commands/all.hpp"
#include "database/Transactions.hpp"
#include "database/Queries/UserQueries.hpp"
#include "seed/include/init_db_tables.hpp"
#include "seed/include/seed_db.hpp"
#include "config/ConfigRegistry.hpp"
#include "logging/LogRegistry.hpp"
#include "concurrency/ThreadPoolManager.hpp"
#include "services/ServiceManager.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "storage/Filesystem.hpp"
#include "storage/StorageManager.hpp"

#include <pdfium/fpdfview.h>
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;
using namespace vh::shell;
using namespace vh::types;
using namespace vh::database;
using namespace vh::config;
using namespace vh::logging;
using namespace vh::concurrency;
using namespace vh::services;
using namespace vh::storage;
using namespace vh::seed;

// --- Example DB/cache validators -------------------------------------------
// Keep these in C++, call your real prepared statements and caches — no duplication.
static vh::test::AssertionResult assert_user_exists(const vh::test::Context& ctx) {
    const auto name = ctx.contains("name") ? ctx.at("name") : "";
    if (name.empty()) return vh::test::AssertionResult::Fail("No 'name' in context");

    if (!UserQueries::userExists(name))
        return vh::test::AssertionResult::Fail("User '" + name + "' not found in DB");

    return vh::test::AssertionResult::Pass(); // stub
}

int main() {
    ConfigRegistry::init();
    LogRegistry::init(fs::temp_directory_path() / "vaulthalla-test");

    FPDF_LIBRARY_CONFIG config;
    config.version = 3;
    config.m_pUserFontPaths = nullptr;
    config.m_pIsolate = nullptr;
    config.m_v8EmbedderSlot = 0;
    FPDF_InitLibraryWithConfig(&config);

    ThreadPoolManager::instance().init();

    Transactions::init();
    seed::init_tables_if_not_exists();
    Transactions::dbPool_->initPreparedStatements();
    if (!UserQueries::adminUserExists()) init();

    ServiceDepsRegistry::init();
    ServiceDepsRegistry::setSyncController(ServiceManager::instance().getSyncController());

    const auto mountPoint = fs::temp_directory_path() / "vaulthalla-test-mnt";
    ServiceManager::instance().setFuseMountPoint(mountPoint);
    ServiceManager::instance().setCtlSocketPath("/tmp/vaulthalla-cli-test.sock");

    Filesystem::init(ServiceDepsRegistry::instance().storageManager);
    ServiceDepsRegistry::instance().storageManager->initStorageEngines();
    ServiceManager::instance().startAll();

    const auto router = ServiceManager::instance().getCLIRouter();

    UsageManager usage;

    const auto admin = UserQueries::getUserByName("admin");
    if (!admin) {
        LogRegistry::vaulthalla()->error("No admin user found; cannot run CLI tests");
        return 1;
    }

    std::unique_ptr<SocketIO> io;

    vh::test::CLITestRunner runner(
        usage,
        /*exec*/ [&](const std::string& cmd) { return router->executeLine(cmd, admin, io.get()); }
    );

    // Example: For "user/create" positive path, assert DB contains the user we created
    runner.registerValidator("user/create", [](const std::string& /*cmd*/,
                                               const CommandResult& /*res*/,
                                               const vh::test::Context& ctx) {
        return assert_user_exists(ctx);
    });

    // Auto-generate tests from your Usage tree (help + happy + negative)
    runner.generateFromUsage(
        /*help_tests     =*/ false,
        /*happy_path     =*/ true,
        /*negative_reqs  =*/ true,
        /*max_examples   =*/ 2
    );

    vh::test::TestCase t;
    t.name = "manual: users alias lists";
    t.path = "user/list";
    t.command = "users";     // if your router supports plural alias mapping
    t.expect_exit = 0;
    runner.addTest(std::move(t));

    // Run
    int exit_status = runner.runAll(std::cout) == 0 ? 0 : 1;

    ServiceManager::instance().stopAll(SIGKILL);
    ThreadPoolManager::instance().shutdown();
    FPDF_DestroyLibrary();

    exit(exit_status);
}
