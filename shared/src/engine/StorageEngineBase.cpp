#include "engine/StorageEngineBase.hpp"
#include "config/ConfigRegistry.hpp"
#include "types/Vault.hpp"
#include "types/File.hpp"
#include "util/Magic.hpp"
#include "crypto/Hash.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/SyncQueries.hpp"
#include "engine/VaultEncryptionManager.hpp"

#include <iostream>

using namespace vh::types;
using namespace vh::database;
using namespace vh::encryption;
using namespace vh::config;
namespace fs = std::filesystem;

namespace vh::engine {

StorageEngineBase::StorageEngineBase(const std::shared_ptr<Vault>& vault)
    : vault(vault),
      sync(SyncQueries::getSync(vault->id)),
      cacheRoot(ConfigRegistry::get().caching.path / std::to_string(vault->id)),
      root(vault->mount_point),
      encryptionManager(std::make_shared<VaultEncryptionManager>(root)) {}

bool StorageEngineBase::isDirectory(const fs::path& rel_path) const {
    return DirectoryQueries::isDirectory(vault->id, rel_path);
}

bool StorageEngineBase::isFile(const fs::path& rel_path) const {
    return FileQueries::isFile(vault->id, rel_path);
}

fs::path StorageEngineBase::getRelativePath(const fs::path& abs_path) const {
    return abs_path.lexically_relative(root).make_preferred();
}

fs::path StorageEngineBase::getAbsolutePath(const fs::path& rel_path) const {
    if (rel_path.empty()) return root;

    fs::path safe_rel = rel_path;
    if (safe_rel.is_absolute()) safe_rel = safe_rel.lexically_relative("/");

    return root / safe_rel;
}

fs::path StorageEngineBase::getRelativeCachePath(const fs::path& abs_path) const {
    return abs_path.lexically_relative(cacheRoot).make_preferred();
}

fs::path StorageEngineBase::getAbsoluteCachePath(const fs::path& rel_path,
                                                          const fs::path& prefix) const {
    const auto relPath = rel_path.string().starts_with("/") ? fs::path(rel_path.string().substr(1)) : rel_path;
    if (prefix.empty()) return cacheRoot / relPath;

    const auto prefixPath = prefix.string().starts_with("/") ? fs::path(prefix.string().substr(1)) : prefix;
    return cacheRoot / prefixPath / relPath;
}

std::shared_ptr<File> StorageEngineBase::createFile(const fs::path& rel_path, const std::vector<uint8_t>& buffer) const {
    const auto absPath = getAbsolutePath(rel_path);

    if (!fs::exists(absPath))
        throw std::runtime_error("File does not exist at path: " + absPath.string());
    if (!fs::is_regular_file(absPath))
        throw std::runtime_error("Path is not a regular file: " + absPath.string());

    auto file = std::make_shared<File>();
    file->vault_id = vault->id;
    file->name = absPath.filename().string();
    file->size_bytes = fs::file_size(absPath);
    file->created_by = file->last_modified_by = vault->owner_id;
    file->path = rel_path;
    file->abs_path = absPath;
    file->mime_type = buffer.empty() ? util::Magic::get_mime_type(absPath) : util::Magic::get_mime_type_from_buffer(buffer);
    file->content_hash = crypto::Hash::blake2b(absPath.string());
    const auto parentPath = file->path.has_parent_path() ? fs::path{"/"} / file->path.parent_path() : fs::path("/");
    file->parent_id = DirectoryQueries::getDirectoryIdByPath(vault->id, parentPath);

    return file;
}

std::vector<uint8_t> StorageEngineBase::decrypt(const unsigned int vaultId, const fs::path& relPath, const std::vector<uint8_t>& payload) const {
    const auto iv = FileQueries::getEncryptionIV(vaultId, relPath);
    if (iv.empty()) throw std::runtime_error("No encryption IV found for file: " + relPath.string());
    return encryptionManager->decrypt(payload, iv);
}

}