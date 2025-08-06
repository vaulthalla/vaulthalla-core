#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "../../include/keys/VaultEncryptionManager.hpp"
#include "config/ConfigRegistry.hpp"
#include "types/Vault.hpp"
#include "types/S3Vault.hpp"
#include "types/FSEntry.hpp"
#include "types/User.hpp"
#include "types/Path.hpp"
#include "types/APIKey.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "logging/LogRegistry.hpp"

// for test setup
#include "keys/APIKeyManager.hpp"
#include "types/S3Bucket.hpp"
#include "types/RSync.hpp"

using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;
using namespace vh::config;
using namespace vh::services;
using namespace vh::logging;

StorageManager::StorageManager() = default;

void StorageManager::initStorageEngines() {
    LogRegistry::storage()->debug("[StorageManager] Initializing storage engines...");
    std::scoped_lock lock(mutex_);

    if (ConfigRegistry::get().advanced.dev_mode) initDevCloudVault();

    engines_.clear();

    try {
        for (auto& vault : VaultQueries::listVaults()) {
            LogRegistry::storage()->debug("[StorageManager] Initializing StorageEngine for Vault {} (ID: {}, Type: {})",
                                          vault->name, vault->id, to_string(vault->type));
            std::shared_ptr<StorageEngine> engine;
            if (vault->type == VaultType::Local) engine = std::make_shared<StorageEngine>(vault);
            else if (vault->type == VaultType::S3) {
                const auto s3Vault = std::static_pointer_cast<S3Vault>(vault);
                engine = std::make_shared<CloudStorageEngine>(s3Vault);
            }
            engines_[engine->paths->absRelToRoot(engine->paths->vaultRoot, PathType::FUSE_ROOT)] = engine;
            vaultToEngine_[vault->id] = engine;
        }
    } catch (const std::exception& e) {
        LogRegistry::storage()->error("[StorageManager] Error initializing storage engines: {}", e.what());
        throw;
    }
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

    LogRegistry::storage()->warn("[StorageManager] No storage engine found for path: {}", absPath.string());
    LogRegistry::storage()->debug("[StorageManager] Available storage engines:");
    for (const auto& [path, engine] : engines_)
        LogRegistry::storage()->debug(" - {} (Vault ID: {}, Type: {})", path, engine->vault->id, to_string(engine->vault->type));

    return nullptr;
}

std::vector<std::shared_ptr<StorageEngine>> StorageManager::getEngines() const {
    std::scoped_lock lock(mutex_);
    std::vector<std::shared_ptr<StorageEngine>> engines;
    engines.reserve(engines_.size());
    for (const auto& [_, engine] : engines_) engines.push_back(engine);
    return engines;
}

void StorageManager::initUserStorage(const std::shared_ptr<User>& user) {
    try {
        LogRegistry::storage()->debug("[StorageManager] Initializing storage user storage...");

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

        LogRegistry::storage()->info("[StorageManager] User storage initialized for user: {} (ID: {})",
                                              user->name, user->id);
    } catch (const std::exception& e) {
        LogRegistry::storage()->error("[StorageManager] Error initializing user storage: {}", e.what());
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

    LogRegistry::storage()->info("[StorageManager] Added new vault with ID: {}, Name: {}, Type: {}",
                                              vault->id, vault->name, to_string(vault->type));

    return vault;
}

void StorageManager::updateVault(const std::shared_ptr<Vault>& vault) {
    if (!vault) throw std::invalid_argument("Vault cannot be null");
    if (vault->id == 0) throw std::invalid_argument("Vault ID cannot be zero");
    std::lock_guard lock(mutex_);
    VaultQueries::upsertVault(vault);
    vaultToEngine_[vault->id]->vault = vault;
    LogRegistry::storage()->info("[StorageManager] Updated vault with ID: {}", vault->id);
}

void StorageManager::removeVault(const unsigned int vaultId) {
    std::lock_guard lock(mutex_);
    VaultQueries::removeVault(vaultId);

    vaultToEngine_.erase(vaultId);
    LogRegistry::storage()->info("[StorageManager] Removed vault with ID: {}", vaultId);
}

std::shared_ptr<Vault> StorageManager::getVault(const unsigned int vaultId) const {
    std::lock_guard lock(mutex_);
    if (vaultToEngine_.find(vaultId) != vaultToEngine_.end()) return vaultToEngine_.at(vaultId)->vault;
    return VaultQueries::getVault(vaultId);
}

std::shared_ptr<StorageEngine> StorageManager::getEngine(const unsigned int id) const {
    std::scoped_lock lock(mutex_);
    if (!vaultToEngine_.contains(id))
        throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(id));
    return vaultToEngine_.at(id);
}

void StorageManager::registerOpenHandle(const fuse_ino_t ino) {
    std::scoped_lock lock(openHandleMutex_);
    ++openHandleCounts_[ino];
}

void StorageManager::closeOpenHandle(const fuse_ino_t ino) {
    std::scoped_lock lock(openHandleMutex_);
    if (--openHandleCounts_[ino] == 0) openHandleCounts_.erase(ino);
}

unsigned int StorageManager::getOpenHandleCount(const fuse_ino_t ino) const {
    std::scoped_lock lock(openHandleMutex_);
    if (openHandleCounts_.contains(ino)) return openHandleCounts_.at(ino);
    return 0;
}

void StorageManager::initDevCloudVault() {
    try {
        const std::string prefix = "VAULTHALLA_TEST_R2_";
        const auto accessKey = prefix + "ACCESS_KEY";
        const auto secretKey = prefix + "SECRET_ACCESS_KEY";
        const auto endpoint = prefix + "ENDPOINT";

        auto key = std::make_shared<api::APIKey>();
        key->user_id = 1; // Default user ID for dev mode
        key->name = "R2 Test Key";
        key->provider = api::S3Provider::CloudflareR2;
        key->region = "wnam";

        if (std::getenv(accessKey.c_str())) key->access_key = std::getenv(accessKey.c_str());
        else return;

        if (std::getenv(secretKey.c_str())) key->secret_access_key = std::getenv(secretKey.c_str());
        else return;

        if (std::getenv(endpoint.c_str())) key->endpoint = std::getenv(endpoint.c_str());
        else return;

        key->id = ServiceDepsRegistry::instance().apiKeyManager->addAPIKey(key);

        if (key->id == 0) {
            LogRegistry::storage()->error("[StorageManager] Failed to create API key for Cloudflare R2");
            return;
        }

        const auto bucket = std::make_shared<api::S3Bucket>();
        bucket->name = "vaulthalla-test";
        bucket->api_key = key;

        const auto vault = std::make_shared<S3Vault>();
        vault->name = "R2 Test Vault";
        vault->description = "Test vault for Cloudflare R2 in development mode";
        vault->mount_point = "cloud/r2_test_vault";
        vault->api_key_id = key->id;
        vault->owner_id = 1;
        vault->bucket = bucket->name;
        vault->type = VaultType::S3;

        const auto sync = std::make_shared<RSync>();
        sync->interval = std::chrono::minutes(10);
        sync->conflict_policy = RSync::ConflictPolicy::KeepLocal;
        sync->strategy = RSync::Strategy::Sync;

        vault->id = VaultQueries::upsertVault(vault, sync);
    } catch (const std::exception& e) {
        LogRegistry::storage()->error("[StorageManager] Error initializing dev Cloudflare R2 vault: {}", e.what());
    }
}

