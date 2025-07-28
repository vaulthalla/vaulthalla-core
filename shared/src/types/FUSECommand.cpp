#include "types/FUSECommand.hpp"
#include <unordered_map>
#include <iostream>

namespace vh::types::fuse {

CommandType FUSECommand::commandTypeFromString(const std::string& s) {
    static const std::unordered_map<std::string, CommandType> map = {
        {"sync", CommandType::SYNC},
        {"register", CommandType::REGISTER},
        {"rename", CommandType::RENAME}
    };

    const auto it = map.find(s);
    return it != map.end() ? it->second : CommandType::SYNC;
}

std::string to_string(const CommandType& type) {
    switch (type) {
        case CommandType::SYNC: return "sync";
        case CommandType::REGISTER: return "register";
        case CommandType::RENAME: return "rename";
        default: return "unknown";
    }
}

FUSECommand FUSECommand::fromJson(const std::string& jsonStr) {
    return fromJson(nlohmann::json::parse(jsonStr));
}

FUSECommand FUSECommand::fromJson(const nlohmann::json& j) {
    FUSECommand cmd;
    if (!j.contains("op")) throw std::invalid_argument("Missing op field");

    cmd.type = commandTypeFromString(j.at("op").get<std::string>());
    cmd.vaultId = j.value("vaultId", 0);
    if (j.contains("fsEntryId")) cmd.fsEntryId = j.at("fsEntryId").get<unsigned int>();
    if (j.contains("from")) cmd.from = j.at("from").get<std::string>();
    if (j.contains("to")) cmd.to = j.at("to").get<std::string>();

    return cmd;
}

}
