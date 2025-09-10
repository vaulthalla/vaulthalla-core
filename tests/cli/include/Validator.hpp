#pragma once

#include "EntityType.hpp"
#include "database/Queries/UserQueries.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/GroupQueries.hpp"
#include "database/Queries/PermsQueries.hpp"

#include "types/User.hpp"
#include "types/Vault.hpp"
#include "types/Group.hpp"
#include "types/UserRole.hpp"
#include "types/VaultRole.hpp"

#include <type_traits>
#include <string>
#include <memory>
#include <vector>

namespace vh::test::cli {

struct AssertionResult {
    bool ok{true};
    std::string message;
    static AssertionResult Pass() { return {true, {}}; }
    static AssertionResult Fail(std::string msg) { return {false, std::move(msg)}; }
};

template <EntityType type, typename T>
struct Validator {
    // Nice hard error if someone mismatches <type, T>
    static constexpr bool TypeOK =
        (type == EntityType::USER      && std::is_same_v<T, vh::types::User>)      ||
        (type == EntityType::VAULT     && std::is_same_v<T, vh::types::Vault>)     ||
        (type == EntityType::GROUP     && std::is_same_v<T, vh::types::Group>)     ||
        (type == EntityType::USER_ROLE && std::is_same_v<T, vh::types::UserRole>)  ||
        (type == EntityType::VAULT_ROLE&& std::is_same_v<T, vh::types::VaultRole>);
    static_assert(TypeOK, "Validator<type,T> mismatch: T must match the entity class for 'type'.");

    static AssertionResult assert_all_exist(const std::vector<std::shared_ptr<T>>& entities) {
        for (const auto& e : entities) {
            const auto r = assert_exists(e);
            if (!r.ok) return r;
        }
        return AssertionResult::Pass();
    }

    static AssertionResult assert_exists(const std::shared_ptr<T>& entity) {
        if constexpr (type == EntityType::USER) {
            if (!database::UserQueries::userExists(entity->name))
                return AssertionResult::Fail("User '" + entity->name + "' not found in DB");
            return AssertionResult::Pass();
        } else if constexpr (type == EntityType::VAULT) {
            // T is vh::types::Vault here
            if (!database::VaultQueries::vaultExists(entity->name, entity->owner_id))
                return AssertionResult::Fail("Vault '" + entity->name + "' not found in DB");
            return AssertionResult::Pass();
        } else if constexpr (type == EntityType::GROUP) {
            if (!database::GroupQueries::groupExists(entity->name))
                return AssertionResult::Fail("Group '" + entity->name + "' not found in DB");
            return AssertionResult::Pass();
        } else if constexpr (type == EntityType::USER_ROLE || type == EntityType::VAULT_ROLE) {
            if (!database::PermsQueries::roleExists(entity->name))
                return AssertionResult::Fail("Role '" + entity->name + "' not found in DB");
            return AssertionResult::Pass();
        } else {
            return AssertionResult::Fail("Validator: unsupported entity type for existence check");
        }
    }

    static AssertionResult assert_not_exists(const std::shared_ptr<T>& entity) {
        if constexpr (type == EntityType::USER) {
            if (database::UserQueries::userExists(entity->name))
                return AssertionResult::Fail("User '" + entity->name + "' unexpectedly found in DB");
            return AssertionResult::Pass();
        } else if constexpr (type == EntityType::VAULT) {
            if (database::VaultQueries::vaultExists(entity->name, entity->owner_id))
                return AssertionResult::Fail("Vault '" + entity->name + "' unexpectedly found in DB");
            return AssertionResult::Pass();
        } else if constexpr (type == EntityType::GROUP) {
            if (database::GroupQueries::groupExists(entity->name))
                return AssertionResult::Fail("Group '" + entity->name + "' unexpectedly found in DB");
            return AssertionResult::Pass();
        } else if constexpr (type == EntityType::USER_ROLE || type == EntityType::VAULT_ROLE) {
            if (database::PermsQueries::roleExists(entity->name))
                return AssertionResult::Fail("Role '" + entity->name + "' unexpectedly found in DB");
            return AssertionResult::Pass();
        } else {
            return AssertionResult::Fail("Validator: unsupported entity type for non-existence check");
        }
    }

    static AssertionResult assert_count_at_least(unsigned int count) {
        if constexpr (type == EntityType::USER) {
            const auto actual = database::UserQueries::listUsers().size();
            if (actual < count)
                return AssertionResult::Fail("Expected at least " + std::to_string(count) + " users, found " + std::to_string(actual));
            return AssertionResult::Pass();
        } else if constexpr (type == EntityType::VAULT) {
            const auto actual = database::VaultQueries::listVaults().size();
            if (actual < count)
                return AssertionResult::Fail("Expected at least " + std::to_string(count) + " vaults, found " + std::to_string(actual));
            return AssertionResult::Pass();
        } else if constexpr (type == EntityType::GROUP) {
            const auto actual = database::GroupQueries::listGroups().size();
            if (actual < count)
                return AssertionResult::Fail("Expected at least " + std::to_string(count) + " groups, found " + std::to_string(actual));
            return AssertionResult::Pass();
        } else if constexpr (type == EntityType::USER_ROLE || type == EntityType::VAULT_ROLE) {
            const auto actual = database::PermsQueries::listRoles().size();
            if (actual < count)
                return AssertionResult::Fail("Expected at least " + std::to_string(count) + " roles, found " + std::to_string(actual));
            return AssertionResult::Pass();
        } else {
            return AssertionResult::Fail("Validator: unsupported entity type for count check");
        }
    }
};

}
