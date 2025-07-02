#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace vh::types::fuse {

enum class CommandType {
    UNKNOWN,
    CREATE,        // Create a file
    DELETE,        // Delete a file
    MKDIR,         // Make directory
    RMDIR,         // Remove directory
    RENAME,        // Rename file or directory
    CHMOD,         // Change permissions
    CHOWN,         // Change owner/group
    SYNC,          // Sync a file or dir to storage
    TOUCH,         // Update timestamp or create empty file
    TRUNCATE,      // Truncate file to size
    PING,          // Health check
    EXISTS,        // Check existence of file/dir
    STAT,          // Get file/directory metadata
    LISTDIR,       // List directory contents
    FLUSH,         // Flush file handle (if tracking in your FUSE layer)
    READ,          // Optional — read data (if your socket supports direct file transfer ops)
    WRITE          // Optional — write data
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
