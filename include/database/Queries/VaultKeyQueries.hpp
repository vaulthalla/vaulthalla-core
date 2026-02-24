#pragma once

#include <memory>

namespace vh::vault::model { struct Key; }

namespace vh::database {

struct VaultKeyQueries {
    [[nodiscard]] static unsigned int addVaultKey(const std::shared_ptr<vault::model::Key>& key);
    static void deleteVaultKey(unsigned int vaultId);
    static void updateVaultKey(const std::shared_ptr<vault::model::Key>& key);
    static std::shared_ptr<vault::model::Key> getVaultKey(unsigned int vaultId);
    [[nodiscard]] static unsigned int rotateVaultKey(const std::shared_ptr<vault::model::Key>& newKey);
    static void markKeyRotationFinished(unsigned int vaultId);
    [[nodiscard]] static bool keyRotationInProgress(unsigned int vaultId);
    [[nodiscard]] static std::shared_ptr<vault::model::Key> getRotationInProgressOldKey(unsigned int vaultId);
};

}
