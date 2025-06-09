#include "include/auth/User.hpp"

#include <utility>

namespace vh::auth {

    User::User(std::string  username,
               std::string  passwordHash,
               std::string  encryptedPrivateKey,
               std::string  publicKey)
            : username_(std::move(username)),
              passwordHash_(std::move(passwordHash)),
              encryptedPrivateKey_(std::move(encryptedPrivateKey)),
              publicKey_(std::move(publicKey)) {}

    const std::string& User::getUsername() const {
        return username_;
    }

    const std::string& User::getPasswordHash() const {
        return passwordHash_;
    }

    const std::string& User::getEncryptedPrivateKey() const {
        return encryptedPrivateKey_;
    }

    const std::string& User::getPublicKey() const {
        return publicKey_;
    }

    void User::setPasswordHash(const std::string& hash) {
        passwordHash_ = hash;
    }

    void User::setEncryptedPrivateKey(const std::string& encryptedKey) {
        encryptedPrivateKey_ = encryptedKey;
    }

} // namespace vh::auth
