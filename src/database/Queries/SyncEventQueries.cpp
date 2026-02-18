#include "database/Queries/SyncEventQueries.hpp"
#include "database/Transactions.hpp"
#include "types/sync/Event.hpp"
#include "types/sync/Throughput.hpp"
#include "util/timestamp.hpp"

using namespace vh::database;
using namespace vh::types::sync;
using namespace vh::util;

void SyncEventQueries::create(const std::shared_ptr<Event>& event) {
    const pqxx::params p {
        event->vault_id,
        timestampToString(event->timestamp_begin),
        std::string(Event::toString(event->status)),
        std::string(Event::toString(event->trigger)),
        event->retry_attempt,
        event->config_hash
    };

    Transactions::exec("SyncQueries::createSyncEvent", [&](pqxx::work& txn) {
        const auto res = txn.exec(pqxx::prepped{"sync_event.create"}, p);
        if (res.empty()) throw std::runtime_error("Failed to create sync event");
        const auto row = res.one_row();
        event->id = row["id"].as<unsigned int>();
        event->run_uuid = row["run_uuid"].as<std::string>();
    });
}

void SyncEventQueries::upsert(const std::shared_ptr<Event>& event) {
    Transactions::exec("SyncQueries::upsertSyncEvent", [&](pqxx::work& txn) {
        if (const auto res = txn.exec(pqxx::prepped{"sync_event.upsert"}, event->getParams());
            res.empty()) throw std::runtime_error("Failed to upsert sync event");

        for (const auto& throughput : event->throughputs) {
            const pqxx::params p {
                event->vault_id,
                event->run_uuid,
                throughput->metricToString(),
                throughput->num_ops,
                throughput->size_bytes,
                throughput->duration_ms
            };
            if (const auto res = txn.exec(pqxx::prepped{"sync_throughput.upsert"}, p);
                res.empty()) throw std::runtime_error("Failed to upsert sync throughput");
        }
    });
}

std::vector<std::shared_ptr<Event> > SyncEventQueries::getEvents(unsigned int vaultId, unsigned int limit, unsigned int offset) {
    return Transactions::exec("SyncQueries::getSyncEvents", [&](pqxx::work& txn) {
        const pqxx::params p { vaultId, limit, offset };
        const auto res = txn.exec(pqxx::prepped{"sync_event.list_for_vault"}, p);
        auto events = sync_events_from_pqxx_res(res);

        for (const auto& event : events) {
            const auto throughput_res = txn.exec(pqxx::prepped{"sync_throughput.read_all_for_run"},
                pqxx::params{event->vault_id, event->run_uuid});
            if (throughput_res.empty()) continue;
            for (const auto& row : throughput_res) event->throughputs.push_back(std::make_unique<Throughput>(row));
        }

        return events;
    });
}

std::vector<std::shared_ptr<Event>> SyncEventQueries::getEvents(const unsigned int vaultId) {
    constexpr unsigned int limit = 100; // Arbitrary limit to prevent fetching too many events at once
    constexpr unsigned int offset = 0; // For pagination, can be adjusted as needed
    return getEvents(vaultId, limit, offset);
}

std::shared_ptr<Event> SyncEventQueries::getLatest(unsigned int vaultId) {
    constexpr unsigned int limit = 1;
    constexpr unsigned int offset = 0;

    return Transactions::exec("SyncQueries::getLatestSyncEvent", [&](pqxx::work& txn) -> std::shared_ptr<Event> {
        const pqxx::params p { vaultId, limit, offset };
        const auto res = txn.exec(pqxx::prepped{"sync_event.list_for_vault"}, p);
        if (res.empty()) return nullptr;
        auto event = std::make_shared<Event>(res.one_row());

        const auto throughput_res = txn.exec(pqxx::prepped{"sync_throughput.read_all_for_run"},
                pqxx::params{event->vault_id, event->run_uuid});
        if (throughput_res.empty()) return event;
        for (const auto& row : throughput_res) event->throughputs.push_back(std::make_unique<Throughput>(row));

        return event;
    });
}
