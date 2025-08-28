#include "concurrency/sync/UploadTask.hpp"
#include "storage/cloud/CloudStorageEngine.hpp"
#include "types/File.hpp"
#include "services/LogRegistry.hpp"

using namespace vh::concurrency;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::logging;

UploadTask::UploadTask(std::shared_ptr<CloudStorageEngine> eng, std::shared_ptr<File> f)
    : engine(std::move(eng)), file(std::move(f)) {}

void UploadTask::operator()() {
    try {
        engine->upload(file);
        promise.set_value(true);
    } catch (const std::exception& e) {
        LogRegistry::sync()->error("[UploadTask] Failed to upload file: {} - {}", file->path.string(), e.what());
        promise.set_value(false);
    }
}

