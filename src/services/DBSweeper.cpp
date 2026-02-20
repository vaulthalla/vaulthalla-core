#include "services/DBSweeper.hpp"
#include "config/ConfigRegistry.hpp"
#include "database/Queries/SyncEventQueries.hpp"
#include "logging/LogRegistry.hpp"

using namespace vh::services;
using namespace vh::config;
using namespace vh::database;
using namespace vh::logging;

DBSweeper::DBSweeper()
    : AsyncService("DBSweeper"),
      sweep_interval_(ConfigRegistry::get().services.db_sweeper.sweep_interval_minutes) {}

void DBSweeper::runLoop() {
    while (!shouldStop()) {
        try {
            SyncEventQueries::purgeOldEvents();
        } catch (const std::exception& e) {
            LogRegistry::vaulthalla()->warn("[DBSweeper] Failed to purge old sync events: {}", e.what());
        }

        lazySleep(sweep_interval_);
    }
}
