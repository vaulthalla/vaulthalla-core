#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "storage/VaultEncryptionManager.hpp"
#include "config/ConfigRegistry.hpp"
#include "types/Vault.hpp"
#include "types/S3Vault.hpp"
#include "types/FSEntry.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "types/User.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/FSEntryQueries.hpp"
#include "util/fsPath.hpp"
#include "util/files.hpp"

#include <iostream>
#include <fstream>
#include <thread>

#include "util/Magic.hpp"

using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;
using namespace vh::config;

StorageManager::StorageManager() {
    inodeToPath_[FUSE_ROOT_ID] = "/";
    pathToInode_["/"] = FUSE_ROOT_ID;
}

void StorageManager::initStorageEngines() {
    std::scoped_lock lock(mutex_);

    engines_.clear();

    try {
        for (auto& vault : VaultQueries::listVaults()) {
            std::shared_ptr<StorageEngine> engine;
            if (vault->type == VaultType::Local) engine = std::make_shared<StorageEngine>(vault);
            else if (vault->type == VaultType::S3) {
                const auto s3Vault = std::static_pointer_cast<S3Vault>(vault);
                engine = std::make_shared<CloudStorageEngine>(s3Vault);
            }
            engines_[makeAbsolute(engine->root)] = engine;
        }
    } catch (const std::exception& e) {
        std::cerr << "[StorageManager] Error initializing storage engines: " << e.what() << std::endl;
        throw;
    }
}

void StorageManager::registerEntry(const unsigned int entryId) {
    const auto entry = FSEntryQueries::getFSEntryById(entryId);
    if (!entry) {
        std::cerr << "[StorageManager] Entry not found for ID: " << entryId << std::endl;
        return;
    }

    if (entry->inode) cacheEntry(entry);
    else {
        fuse_ino_t ino = assignInode(entry->path);
        entry->inode = ino;
        cacheEntry(entry);
    }
}

std::vector<std::shared_ptr<FSEntry>> StorageManager::listDir(const fs::path& absPath, const bool recursive) const {
    const auto entryId = FSEntryQueries::getEntryIdByPath(absPath);
    if (!entryId) {
        std::cerr << "[StorageManager] No entry found for path: " << absPath << std::endl;
        return {};
    }

    std::cout << "[StorageManager] Listing directory: " << absPath << " (ID: " << *entryId << ")" << std::endl;

    return FSEntryQueries::listDir(*entryId, recursive);
}

std::shared_ptr<StorageEngine> StorageManager::resolveStorageEngine(const fs::path& absPath) const {
    std::scoped_lock lock(mutex_);

    fs::path current;
    for (const auto& part : absPath.lexically_normal()) {
        current /= part;

        // Remove trailing slash by converting to generic form and trimming
        std::string currentStr = current.generic_string();
        if (!currentStr.empty() && currentStr.back() == '/')
            currentStr.pop_back();

        auto it = engines_.find(currentStr);
        if (it != engines_.end()) {
            return it->second;
        }
    }

    std::cout << "[StorageManager] No storage engine found for path: " << absPath << std::endl;

    std::cout << "[StorageManager] Available engines: " << std::endl;
    for (const auto& [path, engine] : engines_) {
        std::cout << " - " << path << " (Vault ID: " << engine->vault->id << ", Type: " << to_string(engine->vault->type) << ")" << std::endl;
    }

    return nullptr;
}

fuse_ino_t StorageManager::resolveInode(const fs::path& absPath) {
    std::shared_lock lock(inodeMutex_);
    auto it = pathToInode_.find(absPath);
    if (it != pathToInode_.end()) return it->second;

    const auto entry = FSEntryQueries::getFSEntry(absPath);
    if (!entry) {
        std::cerr << "[StorageManager] Entry not found for path: " << absPath << std::endl;
        return FUSE_ROOT_ID; // or some error code
    }

    fuse_ino_t ino = entry && entry->inode ? *entry->inode : getOrAssignInode(absPath);
    if (ino == FUSE_ROOT_ID) {
        std::cerr << "[StorageManager] Failed to resolve inode for path: " << absPath << std::endl;
        return FUSE_ROOT_ID; // or some error code
    }

    // Cache the resolved inode
    linkPath(absPath, ino);
    return ino;
}

fuse_ino_t StorageManager::assignInode(const fs::path& path) {
    std::unique_lock lock(inodeMutex_);
    const auto it = pathToInode_.find(path);
    if (it != pathToInode_.end()) return it->second;

    const fuse_ino_t ino = nextInode_++;
    inodeToPath_[ino] = path;
    pathToInode_[path] = ino;
    return ino;
}

fuse_ino_t StorageManager::getOrAssignInode(const fs::path& path) {
    std::scoped_lock lock(inodeMutex_);
    auto it = pathToInode_.find(path);
    if (it != pathToInode_.end()) return it->second;

    fuse_ino_t new_ino = nextInode_++;
    pathToInode_[path] = new_ino;
    inodeToPath_[new_ino] = path;
    return new_ino;
}

void StorageManager::linkPath(const fs::path& absPath, const fuse_ino_t ino) {
    std::unique_lock lock(inodeMutex_);
    if (pathToInode_.find(absPath) != pathToInode_.end()) {
        std::cerr << "[StorageManager] Path already linked: " << absPath << std::endl;
        return; // Path already exists
    }
    pathToInode_[absPath] = ino;
    inodeToPath_[ino] = absPath;
}

fs::path StorageManager::resolvePathFromInode(fuse_ino_t ino) {
    // {
    //     std::scoped_lock lock(renameQueueMutex_);
    //     const auto it = renameRequests_.find(ino);
    //     if (it != renameRequests_.end()) {
    //         const auto& rename = it->second;
    //         std::cout << "[StorageManager] Resolving path from pending rename: " << rename.oldPath << " → " << rename.newPath << std::endl;
    //         return rename.newPath;
    //     }
    // }

    std::shared_lock lock(inodeMutex_);
    auto it = inodeToPath_.find(ino);
    if (it == inodeToPath_.end()) throw std::runtime_error("No path for inode");
    return it->second;
}

void StorageManager::decrementInodeRef(const fuse_ino_t ino, const uint64_t nlookup) {
    std::unique_lock lock(inodeMutex_);
    auto it = inodeToPath_.find(ino);
    if (it == inodeToPath_.end()) return; // No such inode

    if (nlookup > 1) {
        // Decrement the reference count
        // This is a no-op, but could be extended
        return;
    }

    // If nlookup is 1, we can remove the inode
    pathToInode_.erase(it->second);
    inodeToPath_.erase(it);
}

std::vector<std::shared_ptr<StorageEngine>> StorageManager::getEngines() const {
    std::scoped_lock lock(mutex_);
    std::vector<std::shared_ptr<StorageEngine>> engines;
    engines.reserve(engines_.size());
    for (const auto& [_, engine] : engines_) engines.push_back(engine);
    return engines;
}

char StorageManager::getPathType(const fs::path& absPath) const {
    if (absPath.empty()) return 'd';
    const auto engine = resolveStorageEngine(absPath);
    if (!engine) {
        std::cerr << "[StorageManager] No storage engine found for path: " << absPath << std::endl;
        return 'u'; // unknown type
    }

    if (engine->isFile(engine->getRelativePath(absPath))) return 'f';
    if (engine->isDirectory(engine->getRelativePath(absPath))) return 'd';
    return 'u'; // unknown type
}

std::shared_ptr<FSEntry> StorageManager::getEntry(const fs::path& absPath) {
    const auto path = makeAbsolute(absPath);
    std::cout << "[StorageManager] Retrieving entry for path: " << path << std::endl;
    const auto it = pathToEntry_.find(path);
    if (it != pathToEntry_.end()) return it->second;

    try {
        auto entry = FSEntryQueries::getFSEntry(path);
        if (!entry) {
            std::cerr << "[StorageManager] Entry not found for path: " << path << std::endl;
            return nullptr;
        }

        if (entry->inode) cacheEntry(entry);
        else {
            entry->inode = std::make_optional(getOrAssignInode(path));
            cacheEntry(entry);
        }
        return entry;
    } catch (const std::exception& e) {
        std::cerr << "[StorageManager] Error retrieving entry for path " << path << ": " << e.what() << std::endl;
        return nullptr;
    }
}

void StorageManager::cacheEntry(const std::shared_ptr<FSEntry>& entry) {
    if (!entry || !entry->inode) throw std::invalid_argument("Entry or inode is null");
    std::cout << "[StorageManager] Caching entry: " << entry->abs_path << " with inode: " << *entry->inode << std::endl;

    std::unique_lock lock(inodeMutex_);
    inodeToPath_[*entry->inode] = entry->abs_path;
    inodeToEntry_[*entry->inode] = entry;
    pathToInode_[entry->abs_path] = *entry->inode;
    pathToEntry_[entry->abs_path] = entry;
}

bool StorageManager::entryExists(const fs::path& absPath) const {
    std::shared_lock lock(inodeMutex_);
    return pathToEntry_.contains(absPath);
}

std::shared_ptr<FSEntry> StorageManager::getEntryFromInode(fuse_ino_t ino) const {
    std::shared_lock lock(inodeMutex_);
    auto it = inodeToEntry_.find(ino);
    if (it != inodeToEntry_.end()) return it->second;
    return nullptr;
}

void StorageManager::evictEntry(fuse_ino_t ino) {
    std::unique_lock lock(inodeMutex_);
    inodeToEntry_.erase(ino);
}

void StorageManager::evictPath(const fs::path& path) {
    std::unique_lock lock(inodeMutex_);
    auto it = pathToInode_.find(path);
    if (it != pathToInode_.end()) {
        fuse_ino_t ino = it->second;
        inodeToPath_.erase(ino);
        inodeToEntry_.erase(ino);
        pathToInode_.erase(path);
        pathToEntry_.erase(path);
    }
}

void StorageManager::updateCachedEntry(const std::shared_ptr<FSEntry>& entry) {
    if (!entry) return;
    std::unique_lock lock(inodeMutex_);
    for (auto& [ino, cached] : inodeToEntry_) {
        if (cached && cached->id == entry->id) {
            inodeToEntry_[ino] = entry;
            break;
        }
    }
}

std::shared_ptr<FSEntry> StorageManager::createFile(const fs::path& path, mode_t mode, uid_t uid, gid_t gid) {
    const auto engine = resolveStorageEngine(path);
    if (!engine) {
        std::cerr << "[StorageManager] No storage engine found for path: " << path << std::endl;
        return nullptr;
    }

    std::cout << "[StorageManager] Creating file at path: " << path << std::endl;

    const fs::path fullDiskPath = ConfigRegistry::get().fuse.backing_path / stripLeadingSlash(path);

    const auto file = std::make_shared<File>();
    file->parent_id = FSEntryQueries::getEntryIdByPath(resolveParent(path));
    file->vault_id = engine->vault->id;
    file->name = path.filename();
    file->path = engine->resolveAbsolutePathToVaultPath(path);
    file->abs_path = path;
    file->backing_path = fullDiskPath;
    file->mode = mode;
    file->owner_uid = uid;
    file->group_gid = gid;
    file->is_hidden = path.filename().string().starts_with('.');
    file->created_at = std::time(nullptr);
    file->updated_at = file->created_at;
    file->inode = std::make_optional(assignInode(path));

    std::error_code ec;
    std::filesystem::create_directories(fullDiskPath.parent_path(), ec); // ensure dirs exist
    std::ofstream(fullDiskPath).close(); // create empty file
    if (!std::filesystem::exists(fullDiskPath))
        std::cerr << "[StorageManager] Failed to create real file at: " << fullDiskPath << std::endl;

    std::cout << "[StorageManager] File created on disk at: " << fullDiskPath << std::endl;

    FileQueries::upsertFile(file);
    cacheEntry(file);

    std::cout << "[StorageManager] File created successfully at: " << path << std::endl;
    return file;
}

void StorageManager::renamePath(const fs::path& oldPath, const fs::path& newPath) {
    std::cout << "[StorageManager] Renaming path from " << oldPath << " to " << newPath << std::endl;

    const auto entry = getEntry(oldPath);
    const auto engine = resolveStorageEngine(oldPath);
    if (!engine) {
        throw std::filesystem::filesystem_error(
            "[StorageManager] No storage engine found for DB-backed rename",
            oldPath,
            std::make_error_code(std::errc::no_such_file_or_directory));
    }

    if (entry->isDirectory()) {
        auto children = listDir(oldPath, true);
        if (!children.empty()) {
            std::cerr << "[StorageManager] WARN: Directory rename does not update children entries yet" << std::endl;
        }
    }

    queuePendingRename(*entry->inode, oldPath, newPath);

    std::cout << "[StorageManager] Rename scheduled successfully for inode " << *entry->inode << std::endl;
}

void StorageManager::updatePaths(const fs::path& oldPath, const fs::path& newPath) {
    std::cout << "[StorageManager] Applying DB and cache updates: " << oldPath << " → " << newPath << std::endl;

    const auto entry = getEntry(oldPath);
    const auto engine = resolveStorageEngine(newPath);

    const auto oldAbsPath = ConfigRegistry::get().fuse.backing_path / stripLeadingSlash(oldPath);
    const auto newAbsPath = ConfigRegistry::get().fuse.backing_path / stripLeadingSlash(newPath);

    std::cout << "[StorageManager] Processing pending rename: " << oldAbsPath << " → " << newAbsPath << std::endl;

    std::cout << "[StorageManager] Renaming on disk with encryption: " << oldAbsPath << " → " << newAbsPath << std::endl;

    const auto buffer = util::readFileToVector(oldAbsPath);

    std::string iv_b64;
    const auto ciphertext = engine->encryptionManager->encrypt(buffer, iv_b64);

    util::writeFile(newAbsPath, ciphertext);
    std::filesystem::remove(oldAbsPath);

    entry->name = newPath.filename();
    entry->path = engine->resolveAbsolutePathToVaultPath(newPath);
    entry->abs_path = newPath;
    entry->backing_path = ConfigRegistry::get().fuse.backing_path / stripLeadingSlash(newPath);
    entry->parent_id = FSEntryQueries::getEntryIdByPath(resolveParent(newPath));

    if (entry->isDirectory()) DirectoryQueries::upsertDirectory(std::static_pointer_cast<Directory>(entry));
    else {
        const auto file = std::static_pointer_cast<File>(entry);
         file->encryption_iv = iv_b64;
         file->size_bytes = std::filesystem::file_size(newAbsPath);
        FileQueries::upsertFile(file);
    }

    evictPath(oldPath);
    evictPath(newPath);
    cacheEntry(entry);

    {
        std::scoped_lock lock(renameQueueMutex_);
        renameRequests_.erase(*entry->inode);
    }

    std::cout << "[StorageManager] updatePaths committed: " << oldPath << " → " << newPath << std::endl;
}

void StorageManager::queuePendingRename(fuse_ino_t ino, const fs::path& oldPath, const fs::path& newPath) {
    std::scoped_lock lock(renameQueueMutex_);
    renameRequests_[ino] = PendingRename{ oldPath, newPath };
}

std::optional<PendingRename> StorageManager::getPendingRename(fuse_ino_t ino) const {
    std::scoped_lock lock(renameQueueMutex_);
    const auto it = renameRequests_.find(ino);
    if (it != renameRequests_.end()) return it->second;
    return std::nullopt;
}

void StorageManager::initUserStorage(const std::shared_ptr<User>& user) {
    try {
        std::cout << "[StorageManager] Initializing storage for user: " << user->name << std::endl;

        if (!user->id) throw std::runtime_error("User ID is not set. Cannot initialize storage.");

        auto vault = std::make_shared<Vault>();
        vault->name = user->name + "'s Local Disk Vault";
        vault->description = "Default local disk vault for " + user->name;
        vault->mount_point = fs::path(config::ConfigRegistry::get().fuse.root_mount_path) / "users" / user->name;

        {
            std::lock_guard lock(mutex_);
            vault->id = VaultQueries::upsertVault(vault);
            vault = VaultQueries::getVault(vault->id);
        }

        if (!vault) throw std::runtime_error("Failed to create or retrieve vault for user: " + user->name);

        vaultToEngine_[vault->id] = std::make_shared<StorageEngine>(vault);

        std::cout << "[StorageManager] Initialized storage for user: " << user->name << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[StorageManager] Error initializing user storage: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<Vault> StorageManager::addVault(std::shared_ptr<Vault> vault,
                                                const std::shared_ptr<Sync>& sync) {
    if (!vault) throw std::invalid_argument("Vault cannot be null");
    std::lock_guard lock(mutex_);

    vault->id = VaultQueries::upsertVault(vault, sync);
    vault = VaultQueries::getVault(vault->id);
    vaultToEngine_[vault->id] = std::make_shared<StorageEngine>(vault);

    return vault;
}

void StorageManager::updateVault(const std::shared_ptr<Vault>& vault) {
    if (!vault) throw std::invalid_argument("Vault cannot be null");
    if (vault->id == 0) throw std::invalid_argument("Vault ID cannot be zero");
    std::lock_guard lock(mutex_);
    VaultQueries::upsertVault(vault);
    vaultToEngine_[vault->id]->vault = vault;
}

void StorageManager::removeVault(const unsigned int vaultId) {
    std::lock_guard lock(mutex_);
    VaultQueries::removeVault(vaultId);

    vaultToEngine_.erase(vaultId);
    std::cout << "[StorageManager] Removed vault with ID: " << vaultId << std::endl;
}

std::vector<std::shared_ptr<Vault> > StorageManager::listVaults(const std::shared_ptr<User>& user) const {
    std::lock_guard lock(mutex_);
    if (user->isAdmin()) return VaultQueries::listVaults();
    return VaultQueries::listUserVaults(user->id);
}

std::shared_ptr<Vault> StorageManager::getVault(const unsigned int vaultId) const {
    std::lock_guard lock(mutex_);
    if (vaultToEngine_.find(vaultId) != vaultToEngine_.end()) return vaultToEngine_.at(vaultId)->vault;
    return VaultQueries::getVault(vaultId);
}

void StorageManager::finishUpload(const unsigned int vaultId, const unsigned int userId,
                                  const std::filesystem::path& relPath) const {
    const auto engine = getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
    engine->finishUpload(userId, relPath);
}

void StorageManager::removeEntry(const unsigned int userId, const unsigned int vaultId, const std::filesystem::path& relPath) const {
    const auto engine = getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
    engine->remove(relPath, userId);
    syncNow(vaultId);
}

void StorageManager::mkdir(const unsigned int vaultId, const std::string& relPath,
                           const std::shared_ptr<User>& user) const {
    const auto engine = getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
    engine->mkdir(relPath, user->id);
}

void StorageManager::move(const unsigned int vaultId, const unsigned int userId,
                          const std::filesystem::path& from, const std::filesystem::path& to) const {
    const auto engine = getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
    engine->move(from, to, userId);
    syncNow(engine->vault->id);
}

void StorageManager::rename(const unsigned int vaultId, const unsigned int userId,
                            const std::string& from, const std::string& to) const {
    const auto engine = getEngine(vaultId);
    engine->rename(from, to, userId);
    syncNow(engine->vault->id);
}

void StorageManager::copy(const unsigned int vaultId, const unsigned int userId,
                          const std::filesystem::path& from, const std::filesystem::path& to) const {
    const auto engine = getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
    engine->copy(from, to, userId);
    syncNow(engine->vault->id);
}

std::vector<std::shared_ptr<FSEntry> > StorageManager::listDir(const unsigned int vaultId,
                                                               const std::string& relPath,
                                                               const bool recursive) const {
    std::lock_guard lock(mutex_);
    return DirectoryQueries::listDir(vaultId, relPath, recursive);
}

std::shared_ptr<StorageEngine> StorageManager::getEngine(const unsigned int id) const {
    std::scoped_lock lock(mutex_);
    if (!vaultToEngine_.contains(id))
        throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(id));
    return vaultToEngine_.at(id);
}

void StorageManager::syncNow(unsigned int vaultId) {
    // TODO: Implement immediate sync logic
}

