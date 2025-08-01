#include "crypto/PasswordHash.hpp"
#include <iostream>
#include <sodium.h>
#include <stdexcept>

namespace vh::crypto {

constexpr std::size_t OPSLIMIT = crypto_pwhash_OPSLIMIT_MODERATE;
constexpr std::size_t MEMLIMIT = crypto_pwhash_MEMLIMIT_MODERATE;

std::string hashPassword(const std::string& password) {
    char hashed[crypto_pwhash_STRBYTES];

    if (crypto_pwhash_str(hashed, password.c_str(), password.size(), OPSLIMIT, MEMLIMIT) != 0)
        throw std::runtime_error("Password hashing failed (out of memory?)");

    return {hashed};
}

bool verifyPassword(const std::string& password, const std::string& hash) {
    return crypto_pwhash_str_verify(hash.c_str(), password.c_str(), password.size()) == 0;
}

} // namespace vh::crypto