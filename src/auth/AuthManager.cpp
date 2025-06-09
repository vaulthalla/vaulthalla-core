#include "auth/AuthManager.hpp"
#include "crypto/PasswordHash.hpp"

#include <sodium.h>
#include <stdexcept>
#include <iostream>
#include <sstream>

namespace vh::auth {

    AuthManager::AuthManager()
                     : sessionManager_(std::make_shared<SessionManager>()),
                       tokenValidator_(std::make_shared<TokenValidator>()) {
        if (sodium_init() < 0) throw std::runtime_error("libsodium initialization failed in AuthManager");
    }

    std::shared_ptr<SessionManager> AuthManager::sessionManager() const { return sessionManager_; }

    std::shared_ptr<TokenValidator> AuthManager::tokenValidator() const { return tokenValidator_; }

    std::shared_ptr<User> AuthManager::registerUser(const std::string& username, const std::string& password) {
        if (users_.count(username) > 0) {
            throw std::runtime_error("User already exists: " + username);
        }

        std::string hashedPassword = hashPassword(password);
        auto keyPair = generateVolumeKeyPair();
        std::string encryptedPrivateKey = encryptPrivateKey(keyPair.first, password);
        std::string publicKey = keyPair.second;

        auto user = std::make_shared<User>(username, hashedPassword, encryptedPrivateKey, publicKey);
        users_[username] = user;

        std::cout << "[AuthManager] Registered new user: " << username << "\n";

        return user;
    }

    std::shared_ptr<User> AuthManager::loginUser(const std::string& username, const std::string& password) {
        auto user = findUser(username);
        if (!user) {
            throw std::runtime_error("User not found: " + username);
        }

        if (!verifyPassword(password, user->getPasswordHash())) {
            throw std::runtime_error("Invalid password for user: " + username);
        }

        std::cout << "[AuthManager] User logged in: " << username << "\n";

        return user;
    }

    void AuthManager::changePassword(const std::string& username, const std::string& oldPassword, const std::string& newPassword) {
        auto user = findUser(username);
        if (!user) {
            throw std::runtime_error("User not found: " + username);
        }

        if (!verifyPassword(oldPassword, user->getPasswordHash())) {
            throw std::runtime_error("Invalid old password for user: " + username);
        }

        std::string newHashed = hashPassword(newPassword);

        // Re-encrypt volume key with new password
        std::string decryptedPrivateKey = decryptPrivateKey(user->getEncryptedPrivateKey(), oldPassword);
        std::string newEncryptedPrivateKey = encryptPrivateKey(decryptedPrivateKey, newPassword);

        user->setPasswordHash(newHashed);
        user->setEncryptedPrivateKey(newEncryptedPrivateKey);

        std::cout << "[AuthManager] Changed password for user: " << username << "\n";
    }

    std::shared_ptr<User> AuthManager::findUser(const std::string& username) {
        auto it = users_.find(username);
        if (it != users_.end()) {
            return it->second;
        }
        return nullptr;
    }

    // === Internal Helpers ===

    std::string AuthManager::hashPassword(const std::string& password) {
        return vh::crypto::hashPassword(password);
    }

    bool AuthManager::verifyPassword(const std::string& password, const std::string& hash) {
        return vh::crypto::verifyPassword(password, hash);
    }

    std::pair<std::string, std::string> AuthManager::generateVolumeKeyPair() {
        unsigned char publicKey[crypto_sign_PUBLICKEYBYTES];
        unsigned char privateKey[crypto_sign_SECRETKEYBYTES];

        if (crypto_sign_keypair(publicKey, privateKey) != 0) {
            throw std::runtime_error("Failed to generate ED25519 keypair");
        }

        std::string privStr(reinterpret_cast<char*>(privateKey), crypto_sign_SECRETKEYBYTES);
        std::string pubStr(reinterpret_cast<char*>(publicKey), crypto_sign_PUBLICKEYBYTES);

        return {privStr, pubStr};
    }

    std::string AuthManager::encryptPrivateKey(const std::string& privateKey, const std::string& password) {
        // Simple example → uses password to derive key → encrypts privateKey using XChaCha20-Poly1305
        unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
        unsigned char salt[crypto_pwhash_SALTBYTES];

        randombytes_buf(salt, sizeof salt);

        if (crypto_pwhash(
                key, sizeof key,
                password.c_str(), password.size(),
                salt,
                crypto_pwhash_OPSLIMIT_MODERATE,
                crypto_pwhash_MEMLIMIT_MODERATE,
                crypto_pwhash_ALG_DEFAULT) != 0) {
            throw std::runtime_error("Key derivation failed");
        }

        unsigned char nonce[crypto_secretbox_NONCEBYTES];
        randombytes_buf(nonce, sizeof nonce);

        std::string ciphertext(privateKey.size() + crypto_secretbox_MACBYTES, '\0');

        crypto_secretbox_easy(reinterpret_cast<unsigned char*>(&ciphertext[0]),
                              reinterpret_cast<const unsigned char*>(privateKey.data()),
                              privateKey.size(),
                              nonce,
                              key);

        // Store: salt || nonce || ciphertext → concat
        std::ostringstream oss;
        oss.write(reinterpret_cast<char*>(salt), sizeof salt);
        oss.write(reinterpret_cast<char*>(nonce), sizeof nonce);
        oss.write(ciphertext.data(), ciphertext.size());

        return oss.str();
    }

    std::string AuthManager::decryptPrivateKey(const std::string& encryptedPrivateKey, const std::string& password) {
        const size_t saltSize = crypto_pwhash_SALTBYTES;
        const size_t nonceSize = crypto_secretbox_NONCEBYTES;

        if (encryptedPrivateKey.size() < saltSize + nonceSize + crypto_secretbox_MACBYTES) {
            throw std::runtime_error("Encrypted private key too small");
        }

        const unsigned char* salt = reinterpret_cast<const unsigned char*>(encryptedPrivateKey.data());
        const unsigned char* nonce = reinterpret_cast<const unsigned char*>(encryptedPrivateKey.data() + saltSize);
        const unsigned char* ciphertext = reinterpret_cast<const unsigned char*>(encryptedPrivateKey.data() + saltSize + nonceSize);

        size_t ciphertextLen = encryptedPrivateKey.size() - saltSize - nonceSize;

        unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES];

        if (crypto_pwhash(
                key, sizeof key,
                password.c_str(), password.size(),
                salt,
                crypto_pwhash_OPSLIMIT_MODERATE,
                crypto_pwhash_MEMLIMIT_MODERATE,
                crypto_pwhash_ALG_DEFAULT) != 0) {
            throw std::runtime_error("Key derivation failed");
        }

        std::string decrypted(ciphertextLen - crypto_secretbox_MACBYTES, '\0');

        if (crypto_secretbox_open_easy(reinterpret_cast<unsigned char*>(&decrypted[0]),
                                       ciphertext,
                                       ciphertextLen,
                                       nonce,
                                       key) != 0) {
            throw std::runtime_error("Invalid password or corrupted private key");
        }

        return decrypted;
    }

} // namespace vh::auth
