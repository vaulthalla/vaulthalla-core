#pragma once

#include <sodium.h>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <algorithm>

namespace vh::ids {

// ---------- Alphabet: Crockford Base32 (no I, L, O, U) – filesystem/email safe
// 32 symbols => each char encodes 5 bits; 128-bit payload => 26 chars
static inline constexpr char kBase32Crockford[] =
    "0123456789ABCDEFGHJKMNPQRSTVWXYZ"; // [0..31]

// Optional: allow lowercase output by postprocessing if desired.
enum class Case { Upper, Lower };

// ---------- Small helper: sodium init (thread-safe, idempotent)
inline void ensure_sodium_init() {
    static const int init = []{
        if (sodium_init() < 0) throw std::runtime_error("libsodium init failed");
        return 1;
    }();
    (void)init;
}

// ---------- Base32 (Crockford) encode for arbitrary byte buffers
inline std::string b32_crockford_encode(const uint8_t* data, size_t len, Case out_case = Case::Upper) {
    if (len == 0) return {};
    // 5-bit packing
    std::string out;
    out.reserve((len * 8 + 4) / 5);

    uint32_t buffer = 0;
    int bits = 0;

    for (size_t i = 0; i < len; ++i) {
        buffer = (buffer << 8) | data[i];
        bits += 8;
        while (bits >= 5) {
            bits -= 5;
            uint8_t idx = (buffer >> bits) & 0x1F;
            out.push_back(kBase32Crockford[idx]);
        }
    }
    if (bits > 0) {
        uint8_t idx = (buffer << (5 - bits)) & 0x1F;
        out.push_back(kBase32Crockford[idx]);
    }
    if (out_case == Case::Lower) {
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    }
    return out;
}

// ---------- RFC 4122 v4 UUID (hex string) if you still want it
inline std::string uuid4_hex() {
    ensure_sodium_init();
    std::array<uint8_t,16> b{};
    randombytes_buf(b.data(), b.size());
    b[6] = (b[6] & 0x0F) | 0x40; // version 4
    b[8] = (b[8] & 0x3F) | 0x80; // variant

    char s[37];
    std::snprintf(s, sizeof s,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
        b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
    return std::string(s);
}

// ---------- Generator options
struct IdOptions {
    // Namespace to derive a *stable*, short, unique prefix per vault (or per anything).
    // Feed this anything stable: vault UUID, DB row id, S3 bucket, etc.
    std::string_view namespace_token;

    // Number of characters to take from the derived per-namespace prefix.
    // 6 chars ~ 30 bits worth of space => negligible collision chance across namespaces.
    size_t prefix_chars = 6;

    // How many random bytes per ID (not counting prefix). 16 bytes => 128-bit => 26 chars.
    // You can crank this down if you really want shorter IDs, at collision risk.
    size_t random_bytes = 16;

    // Separator between the namespace prefix and the random body. "_" and "-" are safe.
    char separator = '_';

    // Output case for encoded chars.
    Case out_case = Case::Upper;
};

// ---------- Derive a short, *deterministic* namespace prefix from the namespace_token
// We use BLAKE2b(keyed or unkeyed). If you want *private* prefixes, add a server secret key.
inline std::string derive_namespace_prefix(std::string_view namespace_token,
                                           size_t prefix_chars,
                                           Case out_case) {
    if (namespace_token.empty() || prefix_chars == 0) return {};
    std::array<uint8_t, 16> digest{}; // 128-bit digest is plenty for a short prefix
    crypto_generichash(digest.data(), digest.size(),
                       reinterpret_cast<const unsigned char*>(namespace_token.data()),
                       namespace_token.size(),
                       /*key=*/nullptr, 0);

    std::string enc = b32_crockford_encode(digest.data(), digest.size(), out_case);
    if (enc.size() < prefix_chars) {
        // Extremely unlikely, but be safe.
        enc.append(prefix_chars - enc.size(), '0');
    }
    enc.resize(prefix_chars);
    return enc;
}

// ---------- The main generator
class IdGenerator {
public:
    explicit IdGenerator(const IdOptions& opt)
        : options_(opt),
          ns_prefix_(derive_namespace_prefix(opt.namespace_token, opt.prefix_chars, opt.out_case)) {
        ensure_sodium_init();
        if (options_.random_bytes == 0) {
            throw std::invalid_argument("random_bytes must be > 0");
        }
        // Sanity: forbid separators that are likely to be annoying
        if (options_.separator == ' ' || options_.separator == '\0' || options_.separator == '\n')
            throw std::invalid_argument("bad separator");
    }

    // Generate a single ID: "<ns_prefix><sep><body>"
    // body is Crockford Base32 encoding of `random_bytes` of secure randomness.
    std::string generate() const {
        std::string id;
        const auto body = random_body_();

        if (!ns_prefix_.empty()) {
            id.reserve(ns_prefix_.size() + 1 + body.size());
            id.append(ns_prefix_);
            id.push_back(options_.separator);
            id.append(body);
        } else {
            id = body;
        }
        return id;
    }

    // Generate N IDs.
    std::vector<std::string> generate_batch(size_t n) const {
        std::vector<std::string> out;
        out.reserve(n);
        for (size_t i = 0; i < n; ++i) out.push_back(generate());
        return out;
    }

    // Accessors
    std::string_view namespace_prefix() const { return ns_prefix_; }
    const IdOptions& options() const { return options_; }

private:
    std::string random_body_() const {
        std::vector<uint8_t> buf(options_.random_bytes);
        randombytes_buf(buf.data(), buf.size());
        // If you want RFC4122-ish semantics embedded here, you can set version/variant bits
        // on the first few bytes *before* encoding. With raw Base32 bodies this is optional.
        return b32_crockford_encode(buf.data(), buf.size(), options_.out_case);
    }

    IdOptions options_;
    std::string ns_prefix_;
};

}
