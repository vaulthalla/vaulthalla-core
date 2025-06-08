#pragma once

#include <string>
#include <memory>
#include <optional>

namespace vh::auth {

    class User {
    public:
        User(std::string  username,
             std::string  passwordHash,
             std::string  encryptedPrivateKey,
             std::string  publicKey);

        [[nodiscard]] const std::string& getUsername() const;
        [[nodiscard]] const std::string& getPasswordHash() const;
        [[nodiscard]] const std::string& getEncryptedPrivateKey() const;
        [[nodiscard]] const std::string& getPublicKey() const;

        void setPasswordHash(const std::string& hash);
        void setEncryptedPrivateKey(const std::string& encryptedKey);

    private:
        std::string username_;
        std::string passwordHash_;           // BCrypt hash
        std::string encryptedPrivateKey_;    // Encrypted ED25519 private key (Base64)
        std::string publicKey_;              // ED25519 public key (Base64)
    };

} // namespace vh::auth
