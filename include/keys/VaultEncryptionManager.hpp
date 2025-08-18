#pragma once

#include "keys/TPMKeyProvider.hpp"

#include <string>
#include <vector>
#include <filesystem>

namespace vh::keys {

class VaultEncryptionManager {
public:
    explicit VaultEncryptionManager(unsigned int vault_id);

    // Must be called before encrypt/decrypt
    void load_key();

    // Encrypt data with vault key, returns ciphertext.
    // Populates out_b64_iv with base64-encoded IV.
    [[nodiscard]] std::vector<uint8_t> encrypt(const std::vector<uint8_t>& plaintext,
                                 std::string& out_b64_iv) const;

    // Decrypt using base64-encoded IV and ciphertext
    [[nodiscard]] std::vector<uint8_t> decrypt(const std::vector<uint8_t>& ciphertext,
                                 const std::string& b64_iv) const;

    [[nodiscard]] std::vector<uint8_t> get_key(const std::string& callingFunctionName) const;

private:
    std::unique_ptr<crypto::TPMKeyProvider> tpmKeyProvider_;
    unsigned int vault_id_;
    std::vector<uint8_t> key_;
};

}
