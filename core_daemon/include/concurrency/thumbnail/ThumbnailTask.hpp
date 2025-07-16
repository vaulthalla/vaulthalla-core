#pragma once

#include "concurrency/Task.hpp"
#include "config/ConfigRegistry.hpp"
#include "util/imageUtil.hpp"
#include "storage/StorageEngine.hpp"
#include "types/CacheIndex.hpp"
#include "database/Queries/CacheQueries.hpp"
#include "types/File.hpp"

#include <memory>
#include <string>
#include <filesystem>
#include <iostream>

namespace vh::concurrency {

class ThumbnailTask : public Task {
public:
    ThumbnailTask(std::shared_ptr<const storage::StorageEngine> engine,
                  std::string buffer,
                  std::shared_ptr<types::File> file)
        : engine_(std::move(engine)), buffer_(std::move(buffer)), file_(std::move(file)) {}

    void operator()() override {
        namespace fs = std::filesystem;

        try {
            const auto& sizes = config::ConfigRegistry::get().caching.thumbnails.sizes;
            for (const auto& size : sizes) {
                fs::path cachePath = engine_->getAbsoluteCachePath(file_->path, fs::path("thumbnails") / std::to_string(size));
                if (cachePath.extension() != ".jpg" && cachePath.extension() != ".jpeg") {
                    cachePath += ".jpg";
                }

                if (!fs::exists(cachePath.parent_path())) {
                    fs::create_directories(cachePath.parent_path());
                }

                util::generateAndStoreThumbnail(buffer_, cachePath, file_->mime_type, size);

                auto index = std::make_shared<types::CacheIndex>();
                index->vault_id = engine_->vaultId();
                index->file_id = file_->id;
                index->path = engine_->getRelativeCachePath(cachePath);
                index->type = types::CacheIndex::Type::Thumbnail;
                index->size = fs::file_size(cachePath);

                database::CacheQueries::upsertCacheIndex(index);
            }
        } catch (const std::exception& e) {
            std::cerr << "[ThumbnailTask] Failed to generate thumbnail(s): " << e.what() << std::endl;
        }
    }

private:
    std::shared_ptr<const storage::StorageEngine> engine_;
    std::string buffer_;
    std::shared_ptr<types::File> file_;
};

}
