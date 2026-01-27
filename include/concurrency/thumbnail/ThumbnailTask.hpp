#pragma once

#include "concurrency/Task.hpp"
#include "config/ConfigRegistry.hpp"
#include "util/imageUtil.hpp"
#include "storage/StorageEngine.hpp"
#include "types/CacheIndex.hpp"
#include "database/Queries/CacheQueries.hpp"
#include "types/File.hpp"
#include "types/Vault.hpp"
#include "types/Path.hpp"
#include "logging/LogRegistry.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "types/stats/CacheStats.hpp"

#include <memory>
#include <string>
#include <filesystem>
#include <chrono>

using namespace vh::types;
using namespace vh::logging;
using namespace vh::config;
using namespace vh::database;
using namespace vh::services;
using namespace std::chrono;

namespace vh::concurrency {

class ThumbnailTask : public Task {
public:
    ThumbnailTask(const std::shared_ptr<const storage::StorageEngine>& engine,
                  const std::vector<uint8_t>& buffer,
                  const std::shared_ptr<File>& file)
        : engine_(engine), buffer_(buffer), file_(file) {}

    void operator()() override {
        namespace fs = std::filesystem;

        try {
            const auto& sizes = ConfigRegistry::get().caching.thumbnails.sizes;
            const auto basePath = engine_->paths->thumbnailRoot / file_->base32_alias;
            for (const auto& size : sizes) {
                const auto filename = std::to_string(size) + ".jpg";
                fs::path cachePath = basePath / filename;

                if (!fs::exists(cachePath.parent_path())) fs::create_directories(cachePath.parent_path());

                if (!file_->mime_type || file_->mime_type->empty()) {
                    LogRegistry::thumb()->warn("[ThumbnailTask] No MIME type for file ID {}. Skipping thumbnail generation.", file_->id);
                    return;
                }

                const auto now = steady_clock::now();
                util::generateAndStoreThumbnail(buffer_, cachePath, *file_->mime_type, size);
                const auto end = steady_clock::now();
                ServiceDepsRegistry::instance().httpCacheStats->record_op_us(duration_cast<microseconds>(end - now).count());

                auto index = std::make_shared<CacheIndex>();
                index->vault_id = engine_->vault->id;
                index->file_id = file_->id;
                index->path = engine_->paths->relPath(cachePath, PathType::CACHE_ROOT);
                index->type = CacheIndex::Type::Thumbnail;
                index->size = fs::file_size(cachePath);

                CacheQueries::upsertCacheIndex(index);

                ServiceDepsRegistry::instance().httpCacheStats->record_insert(index->size);
            }
        } catch (const std::exception& e) {
            LogRegistry::thumb()->error("[ThumbnailTask] Error generating thumbnail for file ID {}: {}", file_->id, e.what());
        } catch (...) {
            LogRegistry::thumb()->error("[ThumbnailTask] Unknown error generating thumbnail for file ID {}", file_->id);
        }
    }

private:
    std::shared_ptr<const storage::StorageEngine> engine_;
    std::vector<uint8_t> buffer_;
    std::shared_ptr<File> file_;
};

}
