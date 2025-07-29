#include "crypto/encrypt.hpp"

#include <sodium.h>
#include <stdexcept>
#include <openssl/evp.h>

namespace vh::crypto {

std::vector<uint8_t> encrypt_aes256_gcm(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& key,
    std::vector<uint8_t>& out_iv)
{
    if (key.size() != AES_KEY_SIZE)
        throw std::invalid_argument("Invalid AES-256 key size");

    out_iv.resize(AES_IV_SIZE);
    randombytes_buf(out_iv.data(), AES_IV_SIZE);

    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-256-GCM", NULL);
    if (!cipher) throw std::runtime_error("EVP_CIPHER_fetch AES-256-GCM failed");

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        EVP_CIPHER_free(cipher);
        throw std::runtime_error("Failed to allocate cipher ctx");
    }

    if (EVP_EncryptInit_ex2(ctx, cipher, key.data(), out_iv.data(), NULL) <= 0) {
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
        throw std::runtime_error("EncryptInit failed");
    }

    std::vector<uint8_t> ciphertext(plaintext.size() + AES_TAG_SIZE);
    int len = 0, outlen = 0;

    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(), plaintext.size()) <= 0)
        throw std::runtime_error("EncryptUpdate failed");
    outlen = len;

    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) <= 0)
        throw std::runtime_error("EncryptFinal failed");
    outlen += len;

    // Append authentication tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, AES_TAG_SIZE,
                            ciphertext.data() + outlen) <= 0)
        throw std::runtime_error("GetTag failed");
    outlen += AES_TAG_SIZE;

    ciphertext.resize(outlen);

    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
    return ciphertext;
}

std::vector<uint8_t> decrypt_aes256_gcm(
    const std::vector<uint8_t>& ciphertext_with_tag,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& iv) {
    if (key.size() != AES_KEY_SIZE || iv.size() != AES_IV_SIZE)
        throw std::invalid_argument("Invalid key or IV size");

    if (ciphertext_with_tag.size() < AES_TAG_SIZE)
        throw std::invalid_argument("Ciphertext too short");

    // Split into ciphertext and tag
    const size_t ciphertext_len = ciphertext_with_tag.size() - AES_TAG_SIZE;
    const uint8_t* tag = ciphertext_with_tag.data() + ciphertext_len;

    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-256-GCM", NULL);
    if (!cipher) throw std::runtime_error("EVP_CIPHER_fetch AES-256-GCM failed");

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        EVP_CIPHER_free(cipher);
        throw std::runtime_error("Failed to allocate cipher ctx");
    }

    if (EVP_DecryptInit_ex2(ctx, cipher, key.data(), iv.data(), NULL) <= 0) {
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
        throw std::runtime_error("DecryptInit failed");
    }

    std::vector<uint8_t> plaintext(ciphertext_len);
    int len = 0, outlen = 0;

    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len,
                          ciphertext_with_tag.data(), ciphertext_len) <= 0)
    {
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
        throw std::runtime_error("DecryptUpdate failed");
    }
    outlen = len;

    // Set expected authentication tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, AES_TAG_SIZE,
                            const_cast<uint8_t*>(tag)) <= 0)
    {
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
        throw std::runtime_error("SetTag failed");
    }

    // Auth check happens here
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + outlen, &len) <= 0) {
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
        throw std::runtime_error("Decryption failed: authentication error");
    }
    outlen += len;

    plaintext.resize(outlen);

    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
    return plaintext;
}

}
