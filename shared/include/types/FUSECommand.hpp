#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace vh::types::fuse {

enum class CommandType {
    SYNC
};

struct FUSECommand {
    CommandType type = CommandType::SYNC;
    unsigned int vaultId;

    static FUSECommand fromJson(const nlohmann::json& j);
    static CommandType commandTypeFromString(const std::string& s);
    static FUSECommand fromJson(const std::string& jsonStr);
};

std::string to_string(const CommandType& type);

}
