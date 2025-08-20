#pragma once

#include "TPMKeyProvider.hpp"

#include <string>
#include <vector>
#include <memory>
#include <atomic>

namespace vh::crypto {

class VaultEncryptionManager {
public:
    explicit VaultEncryptionManager(unsigned int vault_id);

    // Must be called before encrypt/decrypt
    void load_key();

    void prepare_key_rotation();
    void finish_key_rotation();

    [[nodiscard]] std::vector<uint8_t> rotateDecryptEncrypt(const std::vector<uint8_t>& ciphertext, std::string& b64_iv_ref) const;

    // Encrypt data with vault key, returns ciphertext.
    // Populates out_b64_iv with base64-encoded IV.
    [[nodiscard]] std::pair<std::vector<uint8_t>, unsigned int> encrypt(const std::vector<uint8_t>& plaintext, std::string& out_b64_iv) const;

    // Decrypt using base64-encoded IV and ciphertext
    [[nodiscard]] std::vector<uint8_t> decrypt(const std::vector<uint8_t>& ciphertext,
                                 const std::string& b64_iv, unsigned int keyVersion) const;

    [[nodiscard]] std::vector<uint8_t> get_key(const std::string& callingFunctionName) const;

    [[nodiscard]] unsigned int get_key_version() const;

private:
    std::unique_ptr<TPMKeyProvider> tpmKeyProvider_;
    std::atomic<bool> rotation_in_progress_;
    unsigned int vault_id_, version_{};
    std::vector<uint8_t> key_, old_key_;
};

}
