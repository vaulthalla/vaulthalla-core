#pragma once

#include "ArgsGenerator.hpp"
#include "EntityType.hpp"
#include "permsUtil.hpp"

#include <string>
#include <string_view>
#include <memory>
#include <chrono>
#include <stdexcept>
#include <cstdlib>
#include <random>

namespace vh::test::cli {

static inline thread_local std::mt19937_64 rng{std::random_device{}()};

static std::size_t generateRandomIndex(const std::size_t max) {
    if (max == 0) throw std::runtime_error("CLITestContext: cannot generate index for empty collection");
    std::uniform_int_distribution<std::size_t> dist(0, max - 1);
    return dist(rng);
}

static std::string generateNow(args::Gen g,
                           const std::string_view token = "default",
                           const std::string_view usage = "example") {
    args::Rng rng{static_cast<uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    const args::GenContext ctx{std::string(token), std::string(usage)};
    return args::to_string_value(g->generate(rng, ctx));
}

static std::string generateName(const std::string& usage) {
    return generateNow(args::ArgGenerator::Join({
        args::ArgGenerator::Constant(std::string("user_")),
        args::ArgGenerator::RandomString(6, 10, "abcdefghijklmnopqrstuvwxyz0123456789")
    }, ""), "name", usage);
}

static std::string generateEmail(const std::string& usage) {
    return generateNow(args::ArgGenerator::Join({
        args::ArgGenerator::RandomString(6, 10, "abcdefghijklmnopqrstuvwxyz0123456789"),
        args::ArgGenerator::Constant(std::string("@example.org"))
    }, ""), "email", usage);
}

static std::string generateRoleName(const EntityType& type, const std::string& usage) {
    if (type == EntityType::USER_ROLE) {
        return generateNow(args::ArgGenerator::Join({
            args::ArgGenerator::Constant(std::string("role_")),
            args::ArgGenerator::RandomString(6, 10, "abcdefghijklmnopqrstuvwxyz0123456789")
        }, ""), "role_name", usage);
    }
    if (type == EntityType::VAULT_ROLE) {
        return generateNow(args::ArgGenerator::Join({
            args::ArgGenerator::Constant(std::string("vrole_")),
            args::ArgGenerator::RandomString(6, 10, "abcdefghijklmnopqrstuvwxyz0123456789")
        }, ""), "role_name", usage);
    }
    throw std::runtime_error("EntityFactory: unsupported entity type for role name generation");
}

static uint16_t generateBitmask(const std::size_t numBits) {
    if (numBits == 0 || numBits > 16) throw std::runtime_error("EntityFactory: invalid bitmask size");
    uint16_t mask = 0;
    for (std::size_t i = 0; i < numBits; ++i) if (rand() % 2) mask |= (1 << i);
    return mask;
}

static std::string generateQuotaStr(const std::string& usage) {
    return generateNow(args::ArgGenerator::Join({
        args::ArgGenerator::OneOf({"unlimited", "100MB", "1G", "10G", "100G", "1T"})
    }, ""), "quota", usage);
}

static std::string randomAlias(const std::vector<std::string>& aliases) {
    if (aliases.empty()) throw std::runtime_error("AliasHandler: no aliases provided");
    return aliases[generateRandomIndex(aliases.size())];
}

static std::string randomAllowDenyOrNoOptMakeFlag(const std::string& flag) {
    const auto choice = generateRandomIndex(3);
    if (choice == 0) return "--allow-" + flag;
    if (choice == 1) return "--deny-" + flag;
    return "--" + flag;
}

static std::vector<std::string> randomUserPermsFlags() {
    const auto numFlags = std::max(static_cast<size_t>(1), generateRandomIndex(ADMIN_SHELL_PERMS.size()));
    std::unordered_set<std::string> chosen;
    std::vector<std::string> userPerms;
    while (chosen.size() < numFlags) {
        const auto perm = std::string(ADMIN_SHELL_PERMS[generateRandomIndex(ADMIN_SHELL_PERMS.size())]);
        if (chosen.contains(perm)) continue;
        chosen.insert(perm);
        userPerms.push_back(randomAllowDenyOrNoOptMakeFlag(perm));
    }
    return userPerms;
}

static std::vector<std::string> randomVaultPermsFlags() {
    const auto numFlags = std::max(static_cast<size_t>(1), generateRandomIndex(VAULT_SHELL_PERMS.size()));
    std::unordered_set<std::string> chosen;
    std::vector<std::string> vaultPerms;
    while (chosen.size() < numFlags) {
        const auto perm = std::string(VAULT_SHELL_PERMS[generateRandomIndex(VAULT_SHELL_PERMS.size())]);
        if (chosen.contains(perm)) continue;
        chosen.insert(perm);
        vaultPerms.push_back(randomAllowDenyOrNoOptMakeFlag(perm));
    }
    return vaultPerms;
}

}
