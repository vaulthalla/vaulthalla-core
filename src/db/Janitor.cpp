#include "db/Janitor.hpp"
#include "config/ConfigRegistry.hpp"
#include "db/query/sync/Event.hpp"
#include "log/Registry.hpp"

using namespace vh::config;

vh::db::Janitor::Janitor()
    : AsyncService("DBSweeper"),
      sweep_interval_(ConfigRegistry::get().services.db_sweeper.sweep_interval_minutes) {}

void vh::db::Janitor::runLoop() {
    while (!shouldStop()) {
        try {
            query::sync::Event::purgeOldEvents();
        } catch (const std::exception& e) {
            log::Registry::vaulthalla()->warn("[DBSweeper] Failed to purge old sync events: {}", e.what());
        }

        lazySleep(sweep_interval_);
    }
}
