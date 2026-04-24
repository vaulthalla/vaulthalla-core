#include "crypto/util/encrypt.hpp"
#include "log/Registry.hpp"
#include "config/Registry.hpp"

#include <sodium.h>
#include <stdexcept>
#include <fstream>
#include <cstring>
#include <array>
#include <cstdlib>
#include <sstream>

using namespace vh::config;

namespace vh::crypto::util {

namespace {

struct AESGCMCapability {
    bool supported = false;
    bool devOverride = false;
    bool cpuFeaturesChecked = false;
    bool sodiumReportedAvailable = false;
    bool runtimeProbeTried = false;
    bool runtimeProbeSucceeded = false;
    std::vector<std::string> missingFeatures;
    std::string failureReason;
};

static std::string join_features(const std::vector<std::string> &features) {
    std::ostringstream oss;
    for (size_t i = 0; i < features.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << features[i];
    }
    return oss.str();
}

static bool probe_sodium_aes256gcm_encrypt() {
    std::array<unsigned char, AES_KEY_SIZE> key{};
    std::array<unsigned char, AES_IV_SIZE> nonce{};
    std::array<unsigned char, 1> plaintext{0x41};
    std::array<unsigned char, 1 + AES_TAG_SIZE> ciphertext{};
    unsigned long long ciphertext_len = 0;

    randombytes_buf(key.data(), key.size());
    randombytes_buf(nonce.data(), nonce.size());

    return crypto_aead_aes256gcm_encrypt(
        ciphertext.data(), &ciphertext_len,
        plaintext.data(), plaintext.size(),
        nullptr, 0,
        nullptr, nonce.data(), key.data()) == 0;
}

static AESGCMCapability detect_aes_gcm_capability() {
    AESGCMCapability capability{};

    if (Registry::get().dev.enabled || std::getenv("VH_ALLOW_FAKE_AES")) {
        capability.supported = true;
        capability.devOverride = true;
        return capability;
    }

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
    __builtin_cpu_init();
    capability.cpuFeaturesChecked = true;

    const bool hasAES = __builtin_cpu_supports("aes");
    const bool hasPCLMUL = __builtin_cpu_supports("pclmul");
    if (!hasAES) capability.missingFeatures.emplace_back("aes");
    if (!hasPCLMUL) capability.missingFeatures.emplace_back("pclmulqdq");

    if (!capability.missingFeatures.empty()) {
        capability.supported = false;
        return capability;
    }
#endif

    capability.sodiumReportedAvailable = (crypto_aead_aes256gcm_is_available() != 0);
    if (capability.sodiumReportedAvailable) {
        capability.supported = true;
        return capability;
    }

    capability.runtimeProbeTried = true;
    capability.runtimeProbeSucceeded = probe_sodium_aes256gcm_encrypt();
    if (capability.runtimeProbeSucceeded) {
        capability.supported = true;
        return capability;
    }

    capability.supported = false;
    capability.failureReason =
        "libsodium AES256-GCM provider unavailable (crypto_aead_aes256gcm_is_available=0 and runtime probe failed)";
    return capability;
}

static const AESGCMCapability &aes_gcm_capability() {
    static const AESGCMCapability capability = [] {
        auto detected = detect_aes_gcm_capability();
        if (detected.supported) {
            if (detected.devOverride) {
                log::Registry::crypto()->warn(
                    "[crypto::util] AES256-GCM support forced by dev/fake override");
            } else if (detected.cpuFeaturesChecked) {
                log::Registry::crypto()->info(
                    "[crypto::util] AES256-GCM CPU support detected: aes+pclmulqdq");
            }

            if (detected.runtimeProbeTried && detected.runtimeProbeSucceeded) {
                log::Registry::crypto()->warn(
                    "[crypto::util] AES256-GCM runtime probe succeeded despite libsodium availability false");
            }
        } else {
            if (!detected.missingFeatures.empty()) {
                log::Registry::crypto()->error(
                    "[crypto::util] AES256-GCM unavailable: missing {}",
                    join_features(detected.missingFeatures));
            } else if (!detected.failureReason.empty()) {
                log::Registry::crypto()->error(
                    "[crypto::util] AES256-GCM unavailable: {}",
                    detected.failureReason);
            } else {
                log::Registry::crypto()->error(
                    "[crypto::util] AES256-GCM unavailable: unknown capability detection failure");
            }
        }
        return detected;
    }();
    return capability;
}

static bool is_aes_gcm_supported() {
    return aes_gcm_capability().supported;
}

static std::string aes_gcm_unavailable_reason() {
    const auto &capability = aes_gcm_capability();
    if (!capability.missingFeatures.empty()) {
        return "AES256-GCM unavailable: missing " + join_features(capability.missingFeatures);
    }
    if (!capability.failureReason.empty()) {
        return "AES256-GCM unavailable: " + capability.failureReason;
    }
    return "AES256-GCM unavailable on this CPU";
}

} // namespace

std::vector<uint8_t> encrypt_aes256_gcm(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& key,
    std::vector<uint8_t>& out_iv)
{
    if (key.size() != AES_KEY_SIZE) {
        log::Registry::crypto()->error("[encrypt_aes256_gcm] Invalid AES-256 key size: {} bytes", key.size());
        throw std::invalid_argument("Invalid AES-256 key size");
    }

    out_iv.resize(AES_IV_SIZE);
    randombytes_buf(out_iv.data(), AES_IV_SIZE);

    std::vector<uint8_t> ciphertext(plaintext.size() + AES_TAG_SIZE);

    unsigned long long ciphertext_len = 0;
    if (!is_aes_gcm_supported())
        throw std::runtime_error(aes_gcm_unavailable_reason());

    if (crypto_aead_aes256gcm_encrypt(
            ciphertext.data(), &ciphertext_len,
            plaintext.data(), plaintext.size(),
            nullptr, 0,  // no AAD
            nullptr, out_iv.data(), key.data()) != 0)
    {
        throw std::runtime_error("AES256-GCM encryption failed");
    }

    ciphertext.resize(ciphertext_len);
    return ciphertext;
}

std::vector<uint8_t> decrypt_aes256_gcm(
    const std::vector<uint8_t>& ciphertext_with_tag,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& iv)
{
    if (key.size() != AES_KEY_SIZE || iv.size() != AES_IV_SIZE) {
        log::Registry::crypto()->error("[decrypt_aes256_gcm] Invalid key or IV size: "
                                     "key size = {}, iv size = {}",
                                     key.size(), iv.size());
        throw std::invalid_argument("Invalid key or IV size");
    }

    std::vector<uint8_t> decrypted(ciphertext_with_tag.size() - AES_TAG_SIZE);
    unsigned long long decrypted_len = 0;

    if (!is_aes_gcm_supported())
        throw std::runtime_error(aes_gcm_unavailable_reason());

    if (crypto_aead_aes256gcm_decrypt(
            decrypted.data(), &decrypted_len,
            nullptr,
            ciphertext_with_tag.data(), ciphertext_with_tag.size(),
            nullptr, 0,  // no AAD
            iv.data(), key.data()) != 0)
    {
        throw std::runtime_error("Decryption failed: authentication error");
    }

    decrypted.resize(decrypted_len);
    return decrypted;
}

std::vector<uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Failed to open vault key file: " + path.string());

    std::vector<uint8_t> buf(std::istreambuf_iterator<char>(in), {});
    return buf;
}

std::string b64_encode(const std::vector<uint8_t>& data) {
    const size_t encoded_len = sodium_base64_ENCODED_LEN(data.size(), sodium_base64_VARIANT_ORIGINAL);
    std::string result(encoded_len, '\0');

    sodium_bin2base64(result.data(), result.size(),
                      data.data(), data.size(),
                      sodium_base64_VARIANT_ORIGINAL);

    result.resize(std::strlen(result.c_str())); // Trim null terminator
    return result;
}

std::vector<uint8_t> b64_decode(const std::string& b64) {
    std::vector<uint8_t> decoded(AES_IV_SIZE);
    size_t out_len = 0;
    if (sodium_base642bin(decoded.data(), decoded.size(),
                          b64.c_str(), b64.size(),
                          nullptr, &out_len, nullptr,
                          sodium_base64_VARIANT_ORIGINAL) != 0)
    {
        throw std::runtime_error("Invalid base64 IV");
    }
    decoded.resize(out_len);
    return decoded;
}

}
