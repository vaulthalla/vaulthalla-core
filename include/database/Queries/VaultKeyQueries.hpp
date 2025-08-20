#pragma once

#include <memory>

namespace vh::types {
    struct VaultKey;
}

namespace vh::database {

struct VaultKeyQueries {
    [[nodiscard]] static unsigned int addVaultKey(const std::shared_ptr<types::VaultKey>& key);
    static void deleteVaultKey(unsigned int vaultId);
    static void updateVaultKey(const std::shared_ptr<types::VaultKey>& key);
    static std::shared_ptr<types::VaultKey> getVaultKey(unsigned int vaultId);
    [[nodiscard]] static unsigned int rotateVaultKey(const std::shared_ptr<types::VaultKey>& newKey);
    static void markKeyRotationFinished(unsigned int vaultId);
    [[nodiscard]] static bool keyRotationInProgress(unsigned int vaultId);
    [[nodiscard]] static std::shared_ptr<types::VaultKey> getRotationInProgressOldKey(unsigned int vaultId);
};

}
