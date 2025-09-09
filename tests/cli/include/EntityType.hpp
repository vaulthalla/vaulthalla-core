#pragma once

namespace vh::test::cli {

enum class EntityType {
    USER,
    VAULT,
    GROUP,
    USER_ROLE,
    VAULT_ROLE
};

enum class CommandType {
    CREATE,
    UPDATE,
    DELETE,
    LIST,
    INFO
};

inline std::string EntityTypeToString(const EntityType& type) {
    switch (type) {
    case EntityType::USER: return "user";
    case EntityType::VAULT: return "vault";
    case EntityType::GROUP: return "group";
    case EntityType::USER_ROLE:
    case EntityType::VAULT_ROLE: return "role";
    default: return "unknown";
    }
}

inline std::string CommandTypeToString(const CommandType& type) {
    switch (type) {
    case CommandType::CREATE: return "create";
    case CommandType::UPDATE: return "update";
    case CommandType::DELETE: return "delete";
    case CommandType::LIST: return "list";
    case CommandType::INFO: return "info";
    default: return "unknown";
    }
}

}
