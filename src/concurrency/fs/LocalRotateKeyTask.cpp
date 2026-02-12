#include "concurrency/fs/LocalRotateKeyTask.hpp"
#include "storage/StorageEngine.hpp"
#include "types/fs/File.hpp"
#include "database/Queries/FileQueries.hpp"
#include "util/files.hpp"
#include "crypto/VaultEncryptionManager.hpp"

using namespace vh::concurrency;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;
using namespace vh::util;

LocalRotateKeyTask::LocalRotateKeyTask(std::shared_ptr<StorageEngine> eng,
                                       const std::vector<std::shared_ptr<File> >& f,
                                       const unsigned int begin, const unsigned int end)
    : engine(std::move(eng)), files(f), begin(begin), end(end) {
    if (begin >= end || end > files.size()) throw std::invalid_argument("Invalid range for LocalRotateKeyTask");
}


void LocalRotateKeyTask::operator()() {
    try {
        for (unsigned int i = begin; i < end; ++i) {
            const auto& file = files[i];
            if (!file || !file->vault_id) continue;

            const auto encryptedBuffer = readFileToVector(file->backing_path);
            const auto ciphertext = engine->encryptionManager->rotateDecryptEncrypt(encryptedBuffer, file);
            if (ciphertext.empty()) throw std::runtime_error("Failed to rotate key for file: " + file->backing_path.string());
            writeFile(file->backing_path, ciphertext);
            FileQueries::setEncryptionIVAndVersion(file);
        }
        promise.set_value(true);
    } catch (const std::exception& e) {
        LogRegistry::sync()->error("[LocalRotateKeyTask] Exception during key rotation: {}", e.what());
        promise.set_value(false);
    }
}