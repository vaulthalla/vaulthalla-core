#include "database/Queries/WaiverQueries.hpp"
#include "database/Transactions.hpp"
#include "types/sync/Waiver.hpp"
#include "types/vault/S3Vault.hpp"
#include "types/entities/User.hpp"
#include "types/vault/APIKey.hpp"
#include "types/rbac/Role.hpp"

using namespace vh::database;

void WaiverQueries::addWaiver(const std::shared_ptr<types::Waiver>& waiver) {
    if (!waiver) throw std::invalid_argument("Invalid waiver");
    if (!waiver->vault) throw std::invalid_argument("Invalid vault in waiver");
    if (!waiver->user) throw std::invalid_argument("Invalid user in waiver");
    if (!waiver->apiKey) throw std::invalid_argument("Invalid API key in waiver");
    if (waiver->waiver_text.empty()) throw std::invalid_argument("Waiver text cannot be empty");

    Transactions::exec("WaiverQueries::addWaiver", [&](pqxx::work& txn) {
        const pqxx::params p{
            waiver->vault->id,
            waiver->user->id,
            waiver->apiKey->id,
            waiver->waiver_text
        };

        const auto res = txn.exec(pqxx::prepped{"insert_cloud_encryption_waiver"}, p);
        if (res.empty() || res.affected_rows() == 0)
            throw std::runtime_error("Failed to insert waiver for user " + std::to_string(waiver->user->id));
        waiver->id = res.one_field().as<unsigned int>();

        if (waiver->owner && waiver->vault->owner_id != waiver->user->id) {
            pqxx::params p2{
                waiver->id,
                waiver->owner->id,
                waiver->owner->name,
                waiver->owner->email,
                waiver->overridingRole->id,
                waiver->overridingRole->name,
                waiver->overridingRole->type,
                waiver->overridingRole->permissions
            };

            const auto res2 = txn.exec(pqxx::prepped{"insert_waiver_owner_override"}, p2);
            if (res2.empty() || res2.affected_rows() == 0)
                throw std::runtime_error("Failed to insert waiver for owner " + std::to_string(waiver->owner->id));

            const pqxx::params p3{waiver->id, waiver->overridingRole->type};
            txn.exec(pqxx::prepped{"insert_permission_snapshots_for_waiver"}, p3);
        }
    });
}
