#pragma once

#include "concurrency/Task.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "types/File.hpp"
#include "types/RSync.hpp"
#include "database/Queries/FileQueries.hpp"
#include "util/files.hpp"
#include "crypto/VaultEncryptionManager.hpp"

#include <memory>
#include <filesystem>

using namespace vh::types;
using namespace vh::database;
using namespace vh::util;

namespace vh::concurrency {

struct CloudRotateKeyTask final : PromisedTask {

    std::shared_ptr<storage::CloudStorageEngine> engine;
    std::vector<std::shared_ptr<File> > files;
    unsigned int begin, end;

    CloudRotateKeyTask(std::shared_ptr<storage::CloudStorageEngine> eng,
                       const std::vector<std::shared_ptr<File>>& f,
                       const unsigned int begin, const unsigned int end)
        : engine(std::move(eng)), files(f), begin(begin), end(end) {
        if (begin >= end || end > files.size())
            throw std::invalid_argument("Invalid range for CloudRotateKeyTask");
    }

    void operator()() override {
        try {
            const auto rsync = std::static_pointer_cast<RSync>(engine->sync);
            for (unsigned int i = begin; i <= end; ++i) {
                const auto& file = files[i];
                if (!file || !file->vault_id) continue;

                if (!std::filesystem::exists(file->backing_path)) {
                    const auto buffer = engine->downloadToBuffer(file->path);
                    if (buffer.empty()) throw std::runtime_error("Failed to download file: " + file->backing_path.string());

                    std::pair<std::vector<uint8_t>, unsigned int> cipherVersionPair;

                    if (engine->remoteFileIsEncrypted(file->path)) {
                        auto payload = engine->getRemoteIVBase64AndVersion(file->path);
                        if (!payload) payload = FileQueries::getEncryptionIVAndVersion(*file->vault_id, file->path);
                        if (!payload) throw std::runtime_error("No IV found for encrypted file: " + file->backing_path.string());
                        const auto& [iv_b64, key_version] = *payload;
                        file->encryption_iv = iv_b64;
                        file->encrypted_with_key_version = key_version;
                        cipherVersionPair = engine->encryptionManager->rotateDecryptEncrypt(buffer, file->encryption_iv, file->encrypted_with_key_version);
                    } else cipherVersionPair = engine->encryptionManager->encrypt(buffer, file->encryption_iv);

                    const auto& [ciphertext, keyVersion] = cipherVersionPair;
                    if (ciphertext.empty()) throw std::runtime_error("Failed to rotate key for file: " + file->backing_path.string());

                    // encryption iv is already updated in the encrypt method
                    file->encrypted_with_key_version = keyVersion;
                    engine->uploadFileBuffer(file, ciphertext);
                    FileQueries::setEncryptionIVAndVersion(file);

                    if (rsync->strategy == RSync::Strategy::Cache && ciphertext.size() * 2 < engine->freeSpace()) continue;
                    writeFile(file->backing_path, ciphertext);
                    continue;
                }

                const auto encryptedBuffer = readFileToVector(file->backing_path);
                const auto [ciphertext, keyVersion] = engine->encryptionManager->rotateDecryptEncrypt(encryptedBuffer, file->encryption_iv, file->encrypted_with_key_version);
                if (ciphertext.empty()) throw std::runtime_error("Failed to rotate key for file: " + file->backing_path.string());

                engine->uploadFileBuffer(file, ciphertext);

                // encryption iv is already updated in the encrypt method
                file->encrypted_with_key_version = keyVersion;
                FileQueries::setEncryptionIVAndVersion(file);

                if (rsync->strategy == RSync::Strategy::Cache && ciphertext.size() * 2 < engine->freeSpace()) continue;
                writeFile(file->backing_path, ciphertext);
            }
            promise.set_value(true);
        } catch (const std::exception& e) {
            promise.set_value(false);
        }
    }
};

}
