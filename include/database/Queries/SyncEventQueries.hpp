#pragma once

#include <memory>
#include <vector>

namespace vh::types::sync {
struct Event;
}

namespace vh::database {

struct SyncEventQueries {
    static void create(const std::shared_ptr<types::sync::Event>& event);
    static void upsert(const std::shared_ptr<types::sync::Event>& event);
    static std::shared_ptr<types::sync::Event> getLatest(unsigned int vaultId);
    static std::vector<std::shared_ptr<types::sync::Event>> getEvents(unsigned int vaultId, unsigned int limit, unsigned int offset);
    static std::vector<std::shared_ptr<types::sync::Event>> getEvents(unsigned int vaultId);
};

}
