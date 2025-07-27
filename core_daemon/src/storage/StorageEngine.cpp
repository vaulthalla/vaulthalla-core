#include "storage/StorageEngine.hpp"
#include "types/Vault.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "types/Operation.hpp"
#include "concurrency/thumbnail/ThumbnailWorker.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/OperationQueries.hpp"
#include "util/Magic.hpp"
#include "util/files.hpp"
#include "engine/VaultEncryptionManager.hpp"
#include "protocols/FUSECmdClient.hpp"

#include <fstream>
#include <utility>

using namespace vh::types;
using namespace vh::database;
using namespace vh::encryption;
using namespace vh::concurrency;
namespace fs = std::filesystem;

namespace vh::storage {

StorageEngine::StorageEngine(const std::shared_ptr<Vault>& vault)
    : StorageEngineBase(vault) {}

void StorageEngine::finishUpload(const unsigned int userId, const std::filesystem::path& relPath) {
    const auto absPath = getAbsolutePath(relPath);

    if (!std::filesystem::exists(absPath)) throw std::runtime_error("File does not exist at path: " + absPath.string());
    if (!std::filesystem::is_regular_file(absPath)) throw std::runtime_error("Path is not a regular file: " + absPath.string());

    const auto buffer = util::readFileToVector(absPath);

    std::string iv_b64;
    const auto ciphertext = encryptionManager->encrypt(buffer, iv_b64);
    util::writeFile(absPath, ciphertext);

    const auto f = createFile(relPath);
    f->created_by = f->last_modified_by = userId;
    f->encryption_iv = iv_b64;
    f->id = FileQueries::upsertFile(f);

    ThumbnailWorker::enqueue(shared_from_this(), buffer, f);

    sendRegisterCommand(vault->id, f->id);
}

void StorageEngine::mkdir(const fs::path& relPath, const unsigned int userId) const {
    const auto absPath = getAbsolutePath(relPath);
    if (!fs::exists(absPath)) fs::create_directories(absPath);

    const auto d = std::make_shared<Directory>();

    d->vault_id = vault->id;
    d->name = fs::path(relPath).filename().string();
    d->created_by = d->last_modified_by = userId;
    d->path = relPath;
    d->abs_path = absPath;
    d->parent_id = DirectoryQueries::getDirectoryIdByPath(vault->id, fs::path(relPath).parent_path());

    DirectoryQueries::upsertDirectory(d);
}

void StorageEngine::move(const fs::path& from, const fs::path& to, const unsigned int userId) const {
    if (from == to) return;

    const bool isFile = this->isFile(from);

    if (!isFile && !isDirectory(from)) throw std::runtime_error("[StorageEngine] Path does not exist: " + from.string());

    std::shared_ptr<FSEntry> entry;
    if (isFile) entry = FileQueries::getFileByPath(vault->id, from);
    else entry = DirectoryQueries::getDirectoryByPath(vault->id, from);

    if (isFile) FileQueries::moveFile(std::static_pointer_cast<File>(entry), to, userId);
    else DirectoryQueries::moveDirectory(std::static_pointer_cast<Directory>(entry), to, userId);

    OperationQueries::addOperation(std::make_shared<Operation>(entry, to, userId, Operation::Op::Move));
}

void StorageEngine::rename(const fs::path& from, const fs::path& to, const unsigned int userId) const {
    if (from == to) return;

    const bool isFile = this->isFile(from);

    if (!isFile && !isDirectory(from)) throw std::runtime_error("[StorageEngine] Path does not exist: " + from.string());

    std::shared_ptr<FSEntry> entry;
    if (isFile) entry = FileQueries::getFileByPath(vault->id, from);
    else entry = DirectoryQueries::getDirectoryByPath(vault->id, from);

    OperationQueries::addOperation(std::make_shared<Operation>(entry, to, userId, Operation::Op::Rename));

    entry->name = to.filename().string();
    entry->path = to;
    entry->abs_path = getAbsolutePath(to);
    entry->last_modified_by = userId;

    if (isFile) FileQueries::upsertFile(std::static_pointer_cast<File>(entry));
    else DirectoryQueries::upsertDirectory(std::static_pointer_cast<Directory>(entry));
}

void StorageEngine::copy(const fs::path& from, const fs::path& to, const unsigned int userId) const {
    if (from == to) return;

    const bool isFile = this->isFile(from);

    if (!isFile && !isDirectory(from)) throw std::runtime_error("[StorageEngine] Path does not exist: " + from.string());

    std::shared_ptr<FSEntry> entry;
    if (isFile) entry = FileQueries::getFileByPath(vault->id, from);
    else entry = DirectoryQueries::getDirectoryByPath(vault->id, from);

    OperationQueries::addOperation(std::make_shared<Operation>(entry, to, userId, Operation::Op::Copy));

    entry->id = 0;
    entry->path = to;
    entry->abs_path = getAbsolutePath(to);
    entry->name = to.filename().string();
    entry->created_at = {};
    entry->updated_at = {};
    entry->created_by = entry->last_modified_by = userId;
    entry->parent_id = DirectoryQueries::getDirectoryIdByPath(vault->id, to.parent_path());

    if (isFile) FileQueries::upsertFile(std::make_shared<File>(*std::static_pointer_cast<File>(entry)));
    else DirectoryQueries::upsertDirectory(std::make_shared<Directory>(*std::static_pointer_cast<Directory>(entry)));
}

void StorageEngine::remove(const fs::path& rel_path, const unsigned int userId) const {
    if (isDirectory(rel_path)) removeDirectory(rel_path, userId);
    else if (isFile(rel_path)) removeFile(rel_path, userId);
    else throw std::runtime_error("[StorageEngine] Path does not exist: " + rel_path.string());
}

void StorageEngine::removeFile(const fs::path& rel_path, const unsigned int userId) const {
    FileQueries::markFileAsTrashed(userId, vault->id, rel_path);
}

void StorageEngine::removeDirectory(const fs::path& rel_path, const unsigned int userId) const {
    for (const auto& file : FileQueries::listFilesInDir(vault->id, rel_path, true))
        FileQueries::markFileAsTrashed(userId, file->id);
}

}