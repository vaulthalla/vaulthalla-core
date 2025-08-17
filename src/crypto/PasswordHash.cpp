#include "crypto/PasswordHash.hpp"

#include <sodium.h>
#include <stdexcept>
#include <vector>

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

std::string generate_secure_password(const size_t length) {
    if (sodium_init() < 0) throw std::runtime_error("libsodium failed to initialize");

    constexpr std::string_view alphabet =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789"
        "!@#$%^&*()-_=+[]{}<>?/|\\:;,.~";

    std::string password;
    password.reserve(length);
    for (size_t i = 0; i < length; ++i) password.push_back(alphabet[randombytes_uniform(alphabet.size())]);
    return password;
}

}