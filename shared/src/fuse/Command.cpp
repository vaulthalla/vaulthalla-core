#include "fuse/Command.hpp"
#include <unordered_map>

namespace vh::types::fuse {

CommandType Command::commandTypeFromString(const std::string& s) {
    static const std::unordered_map<std::string, CommandType> map = {
        {"create", CommandType::CREATE},
        {"delete", CommandType::DELETE},
        {"mkdir", CommandType::MKDIR},
        {"rmdir", CommandType::RMDIR},
        {"rename", CommandType::RENAME},
        {"chmod", CommandType::CHMOD},
        {"chown", CommandType::CHOWN},
        {"sync", CommandType::SYNC},
        {"touch", CommandType::TOUCH},
        {"truncate", CommandType::TRUNCATE},
        {"ping", CommandType::PING},
        {"exists", CommandType::EXISTS},
        {"stat", CommandType::STAT},
        {"listdir", CommandType::LISTDIR},
        {"flush", CommandType::FLUSH},
        {"read", CommandType::READ},
        {"write", CommandType::WRITE},
    };

    auto it = map.find(s);
    return it != map.end() ? it->second : CommandType::UNKNOWN;
}

std::string to_string(CommandType type) {
    switch (type) {
    case CommandType::CREATE:    return "create";
    case CommandType::DELETE:    return "delete";
    case CommandType::MKDIR:     return "mkdir";
    case CommandType::RMDIR:     return "rmdir";
    case CommandType::RENAME:    return "rename";
    case CommandType::CHMOD:     return "chmod";
    case CommandType::CHOWN:     return "chown";
    case CommandType::SYNC:      return "sync";
    case CommandType::TOUCH:     return "touch";
    case CommandType::TRUNCATE:  return "truncate";
    case CommandType::PING:      return "ping";
    case CommandType::EXISTS:    return "exists";
    case CommandType::STAT:      return "stat";
    case CommandType::LISTDIR:   return "listdir";
    case CommandType::FLUSH:     return "flush";
    case CommandType::READ:      return "read";
    case CommandType::WRITE:     return "write";
    default:                     return "unknown";
    }
}

Command Command::fromJson(const nlohmann::json& j) {
    Command cmd;
    if (!j.contains("op") || !j.contains("path")) {
        throw std::invalid_argument("Missing required command fields");
    }

    cmd.type = commandTypeFromString(j.at("op").get<std::string>());
    cmd.path = j.at("path").get<std::string>();

    if (j.contains("newPath")) cmd.newPath = j.at("newPath").get<std::string>();
    if (j.contains("mode")) cmd.mode = j.at("mode").get<mode_t>();
    if (j.contains("uid")) cmd.uid = j.at("uid").get<uid_t>();
    if (j.contains("gid")) cmd.gid = j.at("gid").get<gid_t>();
    if (j.contains("size")) cmd.size = j.at("size").get<size_t>();

    return cmd;
}

} // namespace vh::types::fuse
