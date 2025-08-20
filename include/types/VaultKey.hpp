#pragma once

#include <vector>
#include <cstdint>
#include <ctime>
#include <nlohmann/json_fwd.hpp>

namespace pqxx {
    class row;
}

namespace vh::types {

struct VaultKey {
    unsigned int vaultId{}, version{};
    std::vector<uint8_t> key, encrypted_key, iv;
    std::time_t created_at{}, updated_at{};

    VaultKey() = default;
    explicit VaultKey(const pqxx::row& row);
};

void to_json(nlohmann::json &j, const VaultKey &vk);
void from_json(const nlohmann::json &j, VaultKey &vk);

}
