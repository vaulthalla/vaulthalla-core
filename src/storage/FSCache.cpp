#include "storage/FSCache.hpp"
#include "types/FSEntry.hpp"
#include "types/Directory.hpp"
#include "database/Queries/FSEntryQueries.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "util/fsPath.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "storage/StorageManager.hpp"
#include "logging/LogRegistry.hpp"
#include "crypto/IdGenerator.hpp"

#include <mutex>

using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;
using namespace vh::services;
using namespace vh::logging;

FSCache::FSCache() {
    inodeToPath_[FUSE_ROOT_ID] = "/";
    pathToInode_["/"] = FUSE_ROOT_ID;
    nextInode_ = FSEntryQueries::getNextInode();
    initRoot();
    restoreCache();
    LogRegistry::storage()->info("[FSCache] Initialized with next inode: {}", nextInode_);
}

void FSCache::initRoot() {
    const auto rootEntry = FSEntryQueries::getRootEntry();
    if (!rootEntry) {
        LogRegistry::storage()->error("[FSCache] Root entry not found in the database");
        throw std::runtime_error("Root entry not found in the database");
    }

    if (!rootEntry->inode) {
        rootEntry->inode = std::make_optional(getOrAssignInode("/"));
        FSEntryQueries::updateFSEntry(rootEntry);
    } else if (rootEntry->inode != FUSE_ROOT_ID) {
        LogRegistry::storage()->warn("[FSCache] Root entry inode is not FUSE_ROOT_ID, resetting to 1");
        rootEntry->inode = FUSE_ROOT_ID;
        FSEntryQueries::updateFSEntry(rootEntry);
    }

    cacheEntry(rootEntry);
}

void FSCache::restoreCache() {
    const auto rootEntry = getEntry(FUSE_ROOT_ID);
    if (!rootEntry) throw std::runtime_error("[FSCache] Root entry not found, cannot restore cache");

    for (const auto& entry : FSEntryQueries::listDir(rootEntry->id, true)) {
        if (!entry) {
            LogRegistry::storage()->warn("[FSCache] Null entry found in directory listing for root");
            continue;
        }

        if (entry->inode) cacheEntry(entry);
        else {
            entry->inode = std::make_optional(getOrAssignInode(entry->fuse_path));
            FSEntryQueries::updateFSEntry(entry);
            cacheEntry(entry);
        }
    }
}

std::shared_ptr<FSEntry> FSCache::getEntry(const fs::path& absPath) {
    const auto path = makeAbsolute(absPath);
    LogRegistry::storage()->debug("[FSCache] Retrieving entry for path: {}", path.string());
    const auto it = pathToEntry_.find(path);
    if (it != pathToEntry_.end()) return it->second;

    try {
        const auto ino = getOrAssignInode(path);
        auto entry = FSEntryQueries::getFSEntryByInode(ino);
        if (!entry) {
            LogRegistry::storage()->warn("[FSCache] No entry found for path: {}", path.string());
            return nullptr;
        }

        if (entry->inode) cacheEntry(entry);
        else {
            entry->inode = std::make_optional(getOrAssignInode(path));
            cacheEntry(entry);
        }
        return entry;
    } catch (const std::exception& e) {
        LogRegistry::storage()->error("[FSCache] Error retrieving entry for path {}: {}", path.string(), e.what());
        return nullptr;
    }
}

std::shared_ptr<FSEntry> FSCache::getEntry(const fuse_ino_t ino) {
    LogRegistry::storage()->debug("[FSCache] Retrieving entry for inode: {}", ino);

    {
        std::shared_lock lock(mutex_);
        if (inodeToEntry_.contains(ino)) return inodeToEntry_.at(ino);
    }

    auto entry = FSEntryQueries::getFSEntryByInode(ino);
    if (entry) cacheEntry(entry);
    else LogRegistry::storage()->warn("[FSCache] No entry found for inode: {}", ino);
    return entry;
}

fuse_ino_t FSCache::resolveInode(const fs::path& absPath) {
    std::shared_lock lock(mutex_);
    auto it = pathToInode_.find(absPath);
    if (it != pathToInode_.end()) return it->second;

    const auto entry = ServiceDepsRegistry::instance().fsCache->getEntry(absPath);
    if (!entry) {
        LogRegistry::storage()->warn("[FSCache] No entry found for path: {}", absPath.string());
        return FUSE_ROOT_ID; // or some error code
    }

    fuse_ino_t ino = entry && entry->inode ? *entry->inode : getOrAssignInode(absPath);
    if (ino == FUSE_ROOT_ID) {
        LogRegistry::storage()->error("[FSCache] Failed to resolve inode for path: {}", absPath.string());
        return FUSE_ROOT_ID; // or some error code
    }

    // Cache the resolved inode
    linkPath(absPath, ino);
    return ino;
}

std::shared_ptr<FSEntry> FSCache::getEntryById(unsigned int id) {
    std::shared_lock lock(mutex_);
    auto it = idToEntry_.find(id);
    if (it != idToEntry_.end()) return it->second;

    LogRegistry::storage()->warn("[FSCache] No entry found for ID: {}", id);
    return nullptr;
}

fuse_ino_t FSCache::assignInode(const fs::path& path) {
    std::unique_lock lock(mutex_);
    const auto it = pathToInode_.find(path);
    if (it != pathToInode_.end()) return it->second;

    const fuse_ino_t ino = nextInode_++;
    inodeToPath_[ino] = path;
    pathToInode_[path] = ino;
    return ino;
}

fuse_ino_t FSCache::getOrAssignInode(const fs::path& path) {
    std::scoped_lock lock(mutex_);
    auto it = pathToInode_.find(path);
    if (it != pathToInode_.end()) return it->second;

    fuse_ino_t new_ino = nextInode_++;
    pathToInode_[path] = new_ino;
    inodeToPath_[new_ino] = path;
    return new_ino;
}

void FSCache::linkPath(const fs::path& absPath, const fuse_ino_t ino) {
    std::unique_lock lock(mutex_);
    if (pathToInode_.contains(absPath)) {
        LogRegistry::storage()->debug("[FSCache] Path {} already linked to inode {}", absPath.string(), ino);
        return; // Path already exists
    }
    pathToInode_[absPath] = ino;
    inodeToPath_[ino] = absPath;
}

fs::path FSCache::resolvePath(fuse_ino_t ino) {
    std::shared_lock lock(mutex_);
    if (!inodeToPath_.contains(ino)) throw std::runtime_error("[FSCache] Inode not found: " + std::to_string(ino));
    return inodeToPath_.at(ino);
}

void FSCache::decrementInodeRef(const fuse_ino_t ino, const uint64_t nlookup) {
    if (nlookup > 1) return;  // This is also a no-op, but could be extended
    evictIno(ino);
}

bool FSCache::entryExists(const fs::path& absPath) const {
    std::shared_lock lock(mutex_);
    return pathToEntry_.contains(absPath);
}

std::shared_ptr<FSEntry> FSCache::getEntryFromInode(const fuse_ino_t ino) const {
    std::shared_lock lock(mutex_);
    if (inodeToEntry_.contains(ino)) return inodeToEntry_.at(ino);
    LogRegistry::storage()->warn("[FSCache] No entry found for inode: {}", ino);
    return nullptr;
}

void FSCache::cacheEntry(const std::shared_ptr<FSEntry>& entry, const bool isFirstSeeding) {
    if (!entry || !entry->inode) throw std::invalid_argument("Entry or inode is null");

    if (*entry->inode != FUSE_ROOT_ID) {
        if (!entry->vault_id) {
            LogRegistry::storage()->error("[FSCache] Entry {} has no vault_id, cannot cache", entry->id);
            throw std::runtime_error("Entry has no vault_id");
        }

        if (entry->fuse_path.empty() || entry->backing_path.empty()) {
            if (!entry->parent_id) {
                LogRegistry::storage()->error("[FSCache] Entry {} has no parent_id, cannot cache", entry->id);
                throw std::runtime_error("Entry has no parent_id");
            }

            if (const auto parent = ServiceDepsRegistry::instance().fsCache->getEntryById(*entry->parent_id)) {
                entry->fuse_path = parent->fuse_path / entry->name;
                entry->backing_path = parent->backing_path / entry->base32_alias;
            } else {
                LogRegistry::storage()->error("[FSCache] No parent entry found for entry {}", entry->id);
                throw std::runtime_error("No parent entry found");
            }
        }
    }

    const pqxx::result parentStats = !isFirstSeeding && entry->parent_id && !entry->isDirectory() ?
            DirectoryQueries::collectParentStats(*entry->parent_id) : pqxx::result{};

    std::unique_lock lock(mutex_);

    auto insert = [&](const std::shared_ptr<FSEntry>& e) {
        if (!e) {
            LogRegistry::fs()->error("[FSCache] Attempted to cache a null entry");
            return;
        }

        if (!e->inode) {
            LogRegistry::fs()->error("[FSCache] Entry {} has no inode, cannot cache", e->id);
            throw std::runtime_error("Entry has no inode");
        }

        inodeToPath_[*e->inode] = e->fuse_path;
        inodeToEntry_[*e->inode] = e;
        pathToInode_[e->fuse_path] = *e->inode;
        pathToEntry_[e->fuse_path] = e;
        inodeToId_[*e->inode] = e->id;
        idToEntry_[e->id] = e;
        if (e->parent_id) childToParent_[e->id] = *e->parent_id;
    };

    insert(entry);

    // Ensure upstream directories stats are updated
    if (!parentStats.empty()) {
        for (const auto& s : parentStats) {
            const auto id = s["id"].as<unsigned int>();
            if (idToEntry_.contains(id)) {
                const auto dir = std::static_pointer_cast<Directory>(idToEntry_[id]);
                dir->size_bytes = s["size_bytes"].as<uintmax_t>();
                dir->file_count = s["file_count"].as<unsigned int>();
                dir->subdirectory_count = s["subdirectory_count"].as<unsigned int>();
            } else insert(FSEntryQueries::getFSEntryById(id));
        }
    }

    LogRegistry::fs()->info("[FSCache] Cached entry: {} with inode {}", entry->fuse_path.string(), *entry->inode);
}

void FSCache::evictIno(fuse_ino_t ino) {
    std::unique_lock lock(mutex_);

    if (!inodeToPath_.contains(ino) || !inodeToId_.contains(ino)) {
        LogRegistry::fs()->warn("[FSCache] Attempted to destroy references for non-existent inode: {}", ino);
        return;
    }

    const auto path = inodeToPath_.at(ino);
    const auto id = inodeToId_.at(ino);
    inodeToPath_.erase(ino);
    pathToInode_.erase(path);
    inodeToEntry_.erase(ino);
    pathToEntry_.erase(path);
    inodeToId_.erase(ino);
    idToEntry_.erase(id);
    if (childToParent_.contains(ino)) childToParent_.erase(ino);
}


void FSCache::evictPath(const fs::path& path) {
    fuse_ino_t ino;

    {
        std::unique_lock lock(mutex_);
        if (!pathToInode_.contains(path)) {
            LogRegistry::fs()->warn("[FSCache] Attempted to evict non-existent path: {}", path.string());
            return;
        }
        ino = pathToInode_[path];
    }

    evictIno(ino);
}

std::vector<std::shared_ptr<FSEntry>> FSCache::listDir(const unsigned int parentId, const bool recursive) const {
    const auto parent = FSEntryQueries::getFSEntryById(parentId);
    if (!parent->isDirectory()) throw std::runtime_error("Parent ID is not a directory");
    const auto parentDir = std::static_pointer_cast<Directory>(parent);
    unsigned int numEntries = parentDir->file_count + parentDir->subdirectory_count;
    if (!recursive)
        for (const auto& subdir : DirectoryQueries::listDirectoriesInDir(parentId))
            numEntries -= subdir->file_count + subdir->subdirectory_count;

    std::vector<std::shared_ptr<FSEntry>> entries;
    entries.reserve(childToParent_.size()); // cheap upper bound

    auto append_children = [&](const unsigned int pid) {
        for (const auto& [childId, pId] : childToParent_) {
            if (pId == pid) {
                if (auto it = idToEntry_.find(childId); it != idToEntry_.end()) {
                    entries.push_back(it->second);
                }
            }
        }
    };

    append_children(parentId);

    if (recursive) {
        // BFS over 'entries' as it grows. Track visited to guard against bad cycles.
        std::unordered_set<unsigned int> visited;
        visited.reserve(childToParent_.size());

        for (const auto & e : entries) {
            if (!e) continue;

            const unsigned int eid = e->id;
            if (!visited.insert(eid).second) continue;

            append_children(eid);
        }
    }

    if (entries.size() != numEntries) {
        LogRegistry::fs()->warn("[FSCache] Expected {} entries, but found {}", numEntries, entries.size());
        const auto expected = FSEntryQueries::listDir(parentId, recursive);
        if (expected.size() != numEntries) {
            if (expected.size() == entries.size()) LogRegistry::fs()->warn("Computed number of entries mismatch with actual");
            throw std::runtime_error(fmt::format("[FSCache] Inconsistent entry count for parent ID {}: expected: {}, found {}, actual: {}",
                                                  parentId, numEntries, entries.size(), expected.size()));
        }
        return expected;
    }

    return entries;
}
