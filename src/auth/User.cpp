#include "auth/User.hpp"

namespace vh::auth {

    User::User(const std::string& username,
               const std::string& passwordHash,
               const std::string& encryptedPrivateKey,
               const std::string& publicKey)
            : username_(username),
              passwordHash_(passwordHash),
              encryptedPrivateKey_(encryptedPrivateKey),
              publicKey_(publicKey) {}

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
