#pragma once

#include <functional>
#include <string>
#include <nlohmann/json.hpp>
#include <unordered_set>

namespace vh::types {
    struct User;
}

namespace vh::shell {

struct FlagKV {
    std::string key;
    std::optional<std::string> value;
};

struct CommandCall {
    std::string name;
    std::vector<FlagKV> options;
    std::vector<std::string> positionals;
    bool rewrote = false;
    std::shared_ptr<types::User> user;

    // owns any strings you create at runtime (JSON, rewrites, etc.)
    std::vector<std::string> arena;
};

struct CommandResult {
    int exit_code = 0;                 // 0 = success
    std::string stdout_text;           // CLI stdout
    std::string stderr_text;           // CLI stderr
    nlohmann::json data;               // optional machine-readable payload
    bool has_data = false;
};

using CommandHandler = std::function<CommandResult(const CommandCall&)>;

struct CommandInfo {
    std::string description;                 // own
    CommandHandler handler;
    std::unordered_set<std::string> aliases; // own normalized aliases (no dashes)
    void print(std::string canonical) const;
};

}
