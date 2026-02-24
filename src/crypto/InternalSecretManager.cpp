#include "crypto/InternalSecretManager.hpp"
#include "database/Queries/InternalSecretQueries.hpp"
#include "crypto/model/InternalSecret.hpp"
#include "crypto/util/encrypt.hpp"
#include "crypto/PasswordHash.hpp"

#include <paths.h>

using namespace vh::crypto;
using namespace vh::crypto::util;
using namespace vh::crypto::model;
using namespace vh::database;

InternalSecretManager::InternalSecretManager()
: tpmKeyProvider_(std::make_unique<TPMKeyProvider>(paths::testMode ? "test_master" : "master")) {
    tpmKeyProvider_->init();
}

std::string InternalSecretManager::jwtSecret() const {
    return getOrInitSecret("jwt_secret");
}

void InternalSecretManager::setJWTSecret(const std::string& secret) const {
    return setEncryptedValue("jwt_secret", secret);
}

std::string InternalSecretManager::getOrInitSecret(const std::string& key) const {
    const auto secret = InternalSecretQueries::getSecret(key);
    if (!secret) {
        const auto newSecret = generate_secure_password(64);
        setEncryptedValue(key, newSecret);
        return newSecret;
    }

    {
        std::scoped_lock lock(mutex_);
        const auto masterKey = tpmKeyProvider_->getMasterKey();
        const auto decrypted = decrypt_aes256_gcm(secret->value, masterKey, secret->iv);
        return {decrypted.begin(), decrypted.end()};
    }
}

void InternalSecretManager::setEncryptedValue(const std::string& key, const std::string& value) const {
    std::scoped_lock lock(mutex_);

    const auto masterKey = tpmKeyProvider_->getMasterKey();
    std::vector<uint8_t> iv;
    const auto plaintext = std::vector<uint8_t>(value.begin(), value.end());
    const auto ciphertext = encrypt_aes256_gcm(plaintext, masterKey, iv);

    const auto secret = std::make_shared<InternalSecret>();
    secret->key = key;
    secret->value = ciphertext;
    secret->iv = iv;

    InternalSecretQueries::upsertSecret(secret);
}
