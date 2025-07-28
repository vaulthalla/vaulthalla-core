#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <optional>

namespace vh::types::fuse {

enum class CommandType {
    SYNC,
    REGISTER,
    RENAME
};

struct FUSECommand {
    CommandType type = CommandType::SYNC;
    unsigned int vaultId;
    std::optional<unsigned int> fsEntryId;
    std::optional<std::filesystem::path> from, to;

    static FUSECommand fromJson(const nlohmann::json& j);
    static CommandType commandTypeFromString(const std::string& s);
    static FUSECommand fromJson(const std::string& jsonStr);
};

std::string to_string(const CommandType& type);

}
