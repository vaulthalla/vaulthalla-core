#pragma once

#include "storage/StorageEngine.hpp"
#include "types/Path.hpp"

#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <random>
#include <sstream>

using namespace vh::types;

namespace vh::util {

inline std::vector<uint8_t> readFileToVector(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("Failed to open file: " + path.string());

    std::streamsize size = in.tellg();
    in.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!in.read(reinterpret_cast<char*>(buffer.data()), size))
        throw std::runtime_error("Failed to read file: " + path.string());
    in.close();

    return buffer;
}

inline std::string readFileToString(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("Failed to open file: " + path.string());

    std::streamsize size = in.tellg();
    in.seekg(0, std::ios::beg);

    std::string buffer(size, '\0');
    if (!in.read(buffer.data(), size))
        throw std::runtime_error("Failed to read file: " + path.string());
    in.close();

    return buffer;
}

inline void writeFile(const std::filesystem::path& absPath, const std::vector<uint8_t>& ciphertext) {
    std::ofstream out(absPath, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Failed to write encrypted file: " + absPath.string());
    out.write(reinterpret_cast<const char*>(ciphertext.data()), ciphertext.size());
    out.close();
}

inline std::string generate_random_suffix(size_t length = 8) {
    static constexpr char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    static thread_local std::mt19937 rng{std::random_device{}()};
    static thread_local std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += charset[dist(rng)];
    }
    return result;
}

inline std::filesystem::path decrypt_file_to_temp(const unsigned int vault_id,
                                                  const std::filesystem::path& rel_path,
                                                  const std::shared_ptr<storage::StorageEngine>& engine) {
    namespace fs = std::filesystem;

    const auto abs_path = engine->paths->absPath(rel_path, PathType::BACKING_VAULT_ROOT);

    // Read encrypted file into memory
    std::ifstream in(abs_path, std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("Failed to open encrypted file: " + abs_path.string());

    std::streamsize size = in.tellg();
    in.seekg(0, std::ios::beg);

    std::vector<uint8_t> ciphertext(size);
    if (!in.read(reinterpret_cast<char*>(ciphertext.data()), size))
        throw std::runtime_error("Failed to read encrypted file: " + abs_path.string());

    // Decrypt
    auto plaintext = engine->decrypt(vault_id, rel_path, ciphertext);

    if (plaintext.empty())
        throw std::runtime_error("Decryption failed or returned empty data for file: " + abs_path.string());

    // Write to temp file
    fs::path tmp_dir = fs::temp_directory_path();
    fs::path tmp_file = tmp_dir / ("vaulthalla_dec_" + generate_random_suffix() + ".tmp");

    std::ofstream out(tmp_file, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Failed to create temp decrypted file: " + tmp_file.string());
    }

    out.write(reinterpret_cast<const char*>(plaintext.data()), plaintext.size());
    out.close();

    // Optional: tie temp file lifetime to vault or PID
    return tmp_file;
}

inline bool isProbablyEncrypted(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return false;
    const auto size = in.tellg();
    return size >= 12 + 16;  // IV + tag = minimum
}

}
