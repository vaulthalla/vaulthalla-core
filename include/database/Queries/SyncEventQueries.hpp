#pragma once

#include <memory>
#include <vector>

namespace vh::sync::model { struct Event; }

namespace vh::database {

struct SyncEventQueries {
    static void create(const std::shared_ptr<sync::model::Event>& event);
    static void upsert(const std::shared_ptr<sync::model::Event>& event);
    static std::shared_ptr<sync::model::Event> getLatest(unsigned int vaultId);
    static std::vector<std::shared_ptr<sync::model::Event>> getEvents(unsigned int vaultId, unsigned int limit, unsigned int offset);
    static std::vector<std::shared_ptr<sync::model::Event>> getEvents(unsigned int vaultId);
    static void heartbeat(const std::shared_ptr<sync::model::Event>& event);

    static void purgeOldEvents();
};

}
