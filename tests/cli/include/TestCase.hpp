#pragma once

#include "EntityType.hpp"
#include "AssertionResult.hpp"

#include <string>
#include <vector>

namespace vh::test::cli {

struct TestCase {
    std::string name{};
    std::string path{};          // e.g. "user/create"
    int expect_exit{0};
    std::vector<std::string> must_contain{};     // stdout contains
    std::vector<std::string> must_not_contain{}; // stdout must NOT contain
    std::shared_ptr<void> entity{nullptr}; // optional entity to be used in the test
    shell::CommandResult result{}; // filled after execution
    AssertionResult assertion{};

    static TestCase List(const EntityType& type, const int expect_exit = 0) {
        const auto typeStr = EntityTypeToString(type);
        const std::string actionStr = "list";
        auto name = actionStr + " " + typeStr;
        auto path = typeStr + "/" + actionStr;
        if (type == EntityType::USER_ROLE) {
            name += " (user)";
            path += "/user";
        } else if (type == EntityType::VAULT_ROLE) {
            name += " (vault)";
            path += "/vault";
        }
        return TestCase{
            .name = name,
            .path = path,
            .expect_exit = expect_exit,
            .must_contain = {},
            .must_not_contain = {},
            .entity = nullptr,
            .assertion = {}
        };
    }

    static TestCase Delete(const EntityType& type, const std::shared_ptr<void>& entity, const int expect_exit = 0) {
        const auto typeStr = EntityTypeToString(type);
        const std::string actionStr = "delete";
        auto name = actionStr + " " + typeStr;
        auto path = typeStr + "/" + actionStr;
        if (type == EntityType::USER_ROLE) {
            name += " (user)";
            path += "/user";
        } else if (type == EntityType::VAULT_ROLE) {
            name += " (vault)";
            path += "/vault";
        }
        return TestCase{
            .name = name,
            .path = path,
            .expect_exit = expect_exit,
            .must_contain = {},
            .must_not_contain = {},
            .entity = entity,
            .assertion = {}
        };
    }

    static TestCase Generate(const EntityType& type, const CommandType& action, const std::shared_ptr<void>& entity = nullptr) {
        const auto typeStr = EntityTypeToString(type);
        const auto actionStr = CommandTypeToString(action);
        auto name = actionStr + " " + typeStr;
        auto path = typeStr + "/" + actionStr;
        if (type == EntityType::USER_ROLE) {
            name += " (user)";
            path += "/user";
        } else if (type == EntityType::VAULT_ROLE) {
            name += " (vault)";
            path += "/vault";
        }
        return TestCase{
            .name = name,
            .path = path,
            .expect_exit = 0,
            .must_contain = {},
            .must_not_contain = {},
            .entity = entity,
            .assertion = {}
        };
    }
};

}
