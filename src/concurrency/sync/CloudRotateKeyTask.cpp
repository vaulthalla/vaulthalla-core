#include "concurrency/sync/CloudRotateKeyTask.hpp"
#include "storage/cloud/CloudStorageEngine.hpp"
#include "types/fs/File.hpp"
#include "types/sync/RSync.hpp"
#include "database/Queries/FileQueries.hpp"
#include "util/files.hpp"
#include "crypto/VaultEncryptionManager.hpp"

#include <filesystem>

using namespace vh::concurrency;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;
using namespace vh::util;

CloudRotateKeyTask::CloudRotateKeyTask(std::shared_ptr<CloudStorageEngine> eng,
                                       const std::vector<std::shared_ptr<File>>& f,
                                       const unsigned int begin, const unsigned int end)
    : engine(std::move(eng)), files(f), begin(begin), end(end) {
    if (begin >= end || end > files.size()) throw std::invalid_argument("Invalid range for CloudRotateKeyTask");
}

void CloudRotateKeyTask::operator()() {
    try {
        const auto rsync = std::static_pointer_cast<RSync>(engine->sync);
        for (unsigned int i = begin; i < end; ++i) {
            const auto& file = files[i];
            if (!file || !file->vault_id) continue;

            if (!std::filesystem::exists(file->backing_path)) {
                const auto buffer = engine->downloadToBuffer(file->path);
                if (buffer.empty()) throw std::runtime_error("Failed to download file: " + file->backing_path.string());

                std::vector<uint8_t> ciphertext;

                if (engine->remoteFileIsEncrypted(file->path)) {
                    auto payload = engine->getRemoteIVBase64AndVersion(file->path);
                    if (!payload) payload = FileQueries::getEncryptionIVAndVersion(*file->vault_id, file->path);
                    if (!payload) throw std::runtime_error(
                        "No IV found for encrypted file: " + file->backing_path.string());
                    const auto& [iv_b64, key_version] = *payload;
                    file->encryption_iv = iv_b64;
                    file->encrypted_with_key_version = key_version;
                    ciphertext = engine->encryptionManager->rotateDecryptEncrypt(buffer, file);
                } else ciphertext = engine->encryptionManager->encrypt(buffer, file);

                if (ciphertext.empty())
                    throw std::runtime_error("Failed to encrypt file: " + file->backing_path.string());

                engine->upload(file, ciphertext);
                FileQueries::setEncryptionIVAndVersion(file);

                if (rsync->strategy == RSync::Strategy::Cache && ciphertext.size() * 2 < engine->freeSpace()) continue;
                writeFile(file->backing_path, ciphertext);
                continue;
            }

            const auto encryptedBuffer = readFileToVector(file->backing_path);
            if (encryptedBuffer.empty()) {
                LogRegistry::sync()->warn("[CloudRotateKeyTask] Empty file buffer for: {}", file->backing_path.string());
                continue;
            }

            const auto ciphertext = engine->encryptionManager->rotateDecryptEncrypt(encryptedBuffer, file);
            if (ciphertext.empty())
                throw std::runtime_error("Failed to rotate key for file: " + file->backing_path.string());

            engine->upload(file, ciphertext);
            FileQueries::setEncryptionIVAndVersion(file);

            if (rsync->strategy == RSync::Strategy::Cache && ciphertext.size() * 2 < engine->freeSpace()) continue;
            writeFile(file->backing_path, ciphertext);
        }
        promise.set_value(true);
    } catch (const std::exception& e) {
        LogRegistry::sync()->error("[CloudRotateKeyTask] Exception during key rotation: {}", e.what());
        promise.set_value(false);
    }
}