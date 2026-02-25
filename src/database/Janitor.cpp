#include "database/Janitor.hpp"
#include "config/ConfigRegistry.hpp"
#include "database/queries/SyncEventQueries.hpp"
#include "log/Registry.hpp"

using namespace vh::config;
using namespace vh::database;

Janitor::Janitor()
    : AsyncService("DBSweeper"),
      sweep_interval_(ConfigRegistry::get().services.db_sweeper.sweep_interval_minutes) {}

void Janitor::runLoop() {
    while (!shouldStop()) {
        try {
            SyncEventQueries::purgeOldEvents();
        } catch (const std::exception& e) {
            log::Registry::vaulthalla()->warn("[DBSweeper] Failed to purge old sync events: {}", e.what());
        }

        lazySleep(sweep_interval_);
    }
}
