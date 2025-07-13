#include "crypto/Hash.hpp"

#include <sodium.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>

using namespace vh::crypto;

std::string Hash::blake2b(const std::filesystem::path& filepath) {
    constexpr size_t hash_len = crypto_generichash_BYTES;
    unsigned char hash[hash_len];

    std::ifstream file(filepath, std::ios::binary);
    if (!file) throw std::runtime_error("Failed to open file for hashing: " + filepath.string());

    crypto_generichash_state state;
    crypto_generichash_init(&state, nullptr, 0, hash_len);

    char buffer[8192];
    while (file.good()) {
        file.read(buffer, sizeof(buffer));
        crypto_generichash_update(&state, reinterpret_cast<unsigned char*>(buffer), file.gcount());
    }

    crypto_generichash_final(&state, hash, hash_len);

    std::ostringstream result;
    for (size_t i = 0; i < hash_len; ++i)
        result << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];

    return result.str();
}
