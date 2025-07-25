#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace vh::encryption {

class VaultEncryptionManager {
public:
    explicit VaultEncryptionManager(const std::filesystem::path& vault_root);

    // Must be called before encrypt/decrypt
    void load_key();

    // Encrypt data with vault key, returns ciphertext.
    // Populates out_b64_iv with base64-encoded IV.
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& plaintext,
                                 std::string& out_b64_iv) const;

    // Decrypt using base64-encoded IV and ciphertext
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& ciphertext,
                                 const std::string& b64_iv) const;

private:
    std::filesystem::path vault_key_path_;
    std::vector<uint8_t> key_;
};

}
