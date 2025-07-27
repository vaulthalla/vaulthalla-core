#include "types/FUSECommand.hpp"
#include <unordered_map>
#include <iostream>

namespace vh::types::fuse {

CommandType FUSECommand::commandTypeFromString(const std::string& s) {
    static const std::unordered_map<std::string, CommandType> map = {
        {"sync", CommandType::SYNC}
    };

    const auto it = map.find(s);
    return it != map.end() ? it->second : CommandType::SYNC;
}

std::string to_string(const CommandType& type) {
    if (type != CommandType::SYNC) throw std::invalid_argument("Unsupported command type");
    return "sync";
}

FUSECommand FUSECommand::fromJson(const std::string& jsonStr) {
    return fromJson(nlohmann::json::parse(jsonStr));
}

FUSECommand FUSECommand::fromJson(const nlohmann::json& j) {
    FUSECommand cmd;
    if (!j.contains("op")) throw std::invalid_argument("Missing required command fields");

    cmd.type = commandTypeFromString(j.at("op").get<std::string>());
    cmd.vaultId = j.value("vaultId", 0);

    return cmd;
}

}
