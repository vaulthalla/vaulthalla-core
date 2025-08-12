#pragma once

#include <string>

namespace vh::crypto {

// Hashes a password with Argon2id using libsodium
std::string hashPassword(const std::string& password);

// Verifies a password against a given Argon2id hash
bool verifyPassword(const std::string& password, const std::string& hash);

std::string generate_secure_password(size_t length = 128);

} // namespace vh::crypto
