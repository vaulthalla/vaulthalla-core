#include "storage/StorageEngine.hpp"
#include "config/ConfigRegistry.hpp"
#include "types/Vault.hpp"

#include <iostream>

namespace vh::storage {

StorageEngine::StorageEngine(const std::shared_ptr<types::Vault>& vault, fs::path root_mount_path)
: vault_(vault) {
    const auto conf = config::ConfigRegistry::get();
    cache_path_ = conf.fuse.root_mount_path / conf.caching.path / std::to_string(vault->id);

    if (root_mount_path.empty()) root_ = cache_path_;
    else root_ = std::move(root_mount_path);

    if (!std::filesystem::exists(root_)) {
        std::cout << "[StorageEngine] Creating root directory: " << root_ << std::endl;
        std::filesystem::create_directories(root_);
    } else std::cout << "[StorageEngine] Root directory already exists: " << root_ << std::endl;
}

std::filesystem::path StorageEngine::getAbsoluteCachePath(const std::filesystem::path& rel_path, const std::filesystem::path& prefix) const {
    const auto relPath = rel_path.string().starts_with("/") ? fs::path(rel_path.string().substr(1)) : rel_path;
    if (prefix.empty()) return cache_path_ / relPath;

    const auto prefixPath = prefix.string().starts_with("/") ? fs::path(prefix.string().substr(1)) : prefix;
    return cache_path_ / prefixPath / relPath;
}


}
