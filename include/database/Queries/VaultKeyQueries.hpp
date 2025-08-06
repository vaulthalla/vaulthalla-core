#pragma once

#include <memory>

namespace vh::types {
    struct VaultKey;
}

namespace vh::database {

struct VaultKeyQueries {
    static void addVaultKey(const std::shared_ptr<types::VaultKey>& key);
    static void deleteVaultKey(unsigned int vaultId);
    static void updateVaultKey(const std::shared_ptr<types::VaultKey>& key);
    static std::shared_ptr<types::VaultKey> getVaultKey(unsigned int vaultId);
};

}
