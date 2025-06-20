#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace vh::types::fuse {

enum class CommandType {
    UNKNOWN,
    CREATE,
    DELETE,
    MKDIR,
    RMDIR,
    RENAME,
    CHMOD,
    CHOWN,
    SYNC,
    TOUCH,
    TRUNCATE,
    PING,
};

struct Command {
    CommandType type = CommandType::UNKNOWN;

    std::string path;
    std::optional<std::string> newPath; // For rename
    std::optional<mode_t> mode;         // For create/chmod
    std::optional<uid_t> uid;           // For chown
    std::optional<gid_t> gid;           // For chown
    std::optional<size_t> size;         // For truncate

    static Command fromJson(const nlohmann::json& j);
    static CommandType commandTypeFromString(const std::string& s);
};

std::string to_string(CommandType type);
} // namespace vh::types::fuse
