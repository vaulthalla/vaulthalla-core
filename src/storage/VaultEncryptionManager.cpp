#include "storage/VaultEncryptionManager.hpp"
#include "crypto/encrypt.hpp"

#include <sodium.h>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <cstring>

namespace vh::encryption {

using namespace crypto;

static std::vector<uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Failed to open vault key file: " + path.string());

    std::vector<uint8_t> buf(std::istreambuf_iterator<char>(in), {});
    return buf;
}

static std::string b64_encode(const std::vector<uint8_t>& data) {
    const size_t encoded_len = sodium_base64_ENCODED_LEN(data.size(), sodium_base64_VARIANT_ORIGINAL);
    std::string result(encoded_len, '\0');

    sodium_bin2base64(result.data(), result.size(),
                      data.data(), data.size(),
                      sodium_base64_VARIANT_ORIGINAL);

    result.resize(std::strlen(result.c_str())); // Trim null terminator
    return result;
}

static std::vector<uint8_t> b64_decode(const std::string& b64) {
    std::vector<uint8_t> decoded(AES_IV_SIZE);
    size_t out_len = 0;
    if (sodium_base642bin(decoded.data(), decoded.size(),
                          b64.c_str(), b64.size(),
                          nullptr, &out_len, nullptr,
                          sodium_base64_VARIANT_ORIGINAL) != 0)
    {
        throw std::runtime_error("Invalid base64 IV");
    }
    decoded.resize(out_len);
    return decoded;
}

VaultEncryptionManager::VaultEncryptionManager(const std::filesystem::path& vault_root)
    : vault_key_path_(vault_root / ".keys" / "vault.key") {
    load_key();
}

void VaultEncryptionManager::load_key() {
    namespace fs = std::filesystem;

    // Ensure .keys dir exists
    const fs::path key_dir = vault_key_path_.parent_path();
    if (!fs::exists(key_dir)) {
        fs::create_directories(key_dir);
        fs::permissions(key_dir,
                        fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec,
                        fs::perm_options::replace);
    }

    if (!fs::exists(vault_key_path_)) {
        std::cout << "Vault key size: " << AES_KEY_SIZE << " bytes" << std::endl;
        std::vector<uint8_t> new_key(AES_KEY_SIZE);
        randombytes_buf(new_key.data(), AES_KEY_SIZE);

        std::ofstream out(vault_key_path_, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw std::runtime_error("Failed to create vault key: " + vault_key_path_.string());
        }
        out.write(reinterpret_cast<const char*>(new_key.data()), new_key.size());
        out.close();

        fs::permissions(vault_key_path_,
                        fs::perms::owner_read | fs::perms::owner_write,
                        fs::perm_options::replace);

        key_ = std::move(new_key);
        return;
    }

    // Load existing key
    key_ = read_file(vault_key_path_);
    if (key_.size() != AES_KEY_SIZE) {
        throw std::runtime_error("Vault key must be 32 bytes (AES-256)");
    }
}

std::vector<uint8_t> VaultEncryptionManager::encrypt(
    const std::vector<uint8_t>& plaintext,
    std::string& out_b64_iv) const
{
    std::vector<uint8_t> iv;
    auto ciphertext = encrypt_aes256_gcm(plaintext, key_, iv);
    out_b64_iv = b64_encode(iv);
    return ciphertext;
}

std::vector<uint8_t> VaultEncryptionManager::decrypt(
    const std::vector<uint8_t>& ciphertext,
    const std::string& b64_iv) const
{
    const auto iv = b64_decode(b64_iv);
    return decrypt_aes256_gcm(ciphertext, key_, iv);
}

}
