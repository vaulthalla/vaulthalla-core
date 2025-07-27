#include "storage/StorageEngine.hpp"
#include "config/ConfigRegistry.hpp"
#include "types/Vault.hpp"
#include "database/Queries/FileQueries.hpp"
#include "util/Magic.hpp"
#include "util/files.hpp"
#include "storage/Filesystem.hpp"

#include <iostream>
#include <algorithm>
#include <fstream>
#include <utility>
#include <thread>

using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;
using namespace vh::encryption;
namespace fs = std::filesystem;

StorageEngine::StorageEngine(const std::shared_ptr<Vault>& vault)
    : StorageEngineBase(vault) {
    if (!Filesystem::exists(root)) Filesystem::mkVault(root, vault->id);
    if (!Filesystem::exists(cacheRoot)) Filesystem::mkCache(cacheRoot, vault->id);
}

uintmax_t StorageEngine::getDirectorySize(const fs::path& path) {
    uintmax_t total = 0;
    for (auto& p : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied))
        if (fs::is_regular_file(p.status())) total += fs::file_size(p);
    return total;
}

uintmax_t StorageEngine::getVaultSize() const { return getDirectorySize(root); }
uintmax_t StorageEngine::getCacheSize() const { return getDirectorySize(cacheRoot); }
uintmax_t StorageEngine::getVaultAndCacheTotalSize() const { return getVaultSize() + getCacheSize(); }

uintmax_t StorageEngine::freeSpace() const {
    return vault->quota - getVaultAndCacheTotalSize() - MIN_FREE_SPACE;
}

void StorageEngine::purgeThumbnails(const fs::path& rel_path) const {
    for (const auto& size : config::ConfigRegistry::get().caching.thumbnails.sizes) {
        const auto thumbnailPath = getAbsoluteCachePath(rel_path, fs::path("thumbnails") / std::to_string(size));
        if (fs::exists(thumbnailPath)) fs::remove(thumbnailPath);
    }
}

std::string StorageEngine::getMimeType(const fs::path& path) {
    static const std::unordered_map<std::string, std::string> mimeMap = {
        {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"}, {".png", "image/png"},
        {".pdf", "application/pdf"}, {".txt", "text/plain"}, {".html", "text/html"},
    };

    std::string ext = path.extension().string();
    std::ranges::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    const auto it = mimeMap.find(ext);
    return it != mimeMap.end() ? it->second : "application/octet-stream";
}

void StorageEngine::moveThumbnails(const std::filesystem::path& from, const std::filesystem::path& to) const {
    for (const auto& size : config::ConfigRegistry::get().caching.thumbnails.sizes) {
        auto fromPath = getAbsoluteCachePath(from, fs::path("thumbnails") / std::to_string(size));
        auto toPath = getAbsoluteCachePath(to, fs::path("thumbnails") / std::to_string(size));

        if (fromPath.extension() != ".jpg" && fromPath.extension() != ".jpeg") {
            fromPath += ".jpg";
            toPath += ".jpg";
        }

        if (!fs::exists(fromPath)) {
            std::cerr << "[StorageEngine] Thumbnail does not exist: " << fromPath.string() << std::endl;
            continue;
        }

        Filesystem::mkdir(toPath.parent_path());
        fs::rename(fromPath, toPath); // TODO: Handle rename properly
    }
}

void StorageEngine::copyThumbnails(const std::filesystem::path& from, const std::filesystem::path& to) const {
    for (const auto& size : config::ConfigRegistry::get().caching.thumbnails.sizes) {
        auto fromPath = getAbsoluteCachePath(from, fs::path("thumbnails") / std::to_string(size));
        auto toPath = getAbsoluteCachePath(to, fs::path("thumbnails") / std::to_string(size));

        if (fromPath.extension() != ".jpg" && fromPath.extension() != ".jpeg") {
            fromPath += ".jpg";
            toPath += ".jpg";
        }

        if (!fs::exists(fromPath)) {
            std::cerr << "[StorageEngine] Thumbnail does not exist: " << fromPath.string() << std::endl;
            continue;
        }

        Filesystem::mkdir(toPath.parent_path());
        fs::copy_file(fromPath, toPath, fs::copy_options::overwrite_existing);  // TODO: Handle copy properly
    }
}

fs::path StorageEngine::resolveAbsolutePathToVaultPath(const fs::path& path) const {
    auto normPath = fs::weakly_canonical(path);
    auto normRoot = fs::weakly_canonical(root);

    if (normPath.string().starts_with(normRoot.string())) {
        return fs::relative(normPath, normRoot);
    } else {
        std::cerr << "[StorageEngine] Path " << normPath << " is not under root " << normRoot << std::endl;
        return normPath.filename(); // fallback: just use the file name
    }
}
