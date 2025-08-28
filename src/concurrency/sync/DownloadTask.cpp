#include "concurrency/sync/DownloadTask.hpp"
#include "storage/cloud/CloudStorageEngine.hpp"
#include "types/File.hpp"
#include "services/LogRegistry.hpp"

using namespace vh::concurrency;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::logging;

DownloadTask::DownloadTask(std::shared_ptr<CloudStorageEngine> eng,
                           std::shared_ptr<File> f,
                           const bool freeAfter)
    : engine(std::move(eng)), file(std::move(f)), freeAfterDownload(freeAfter) {}

void DownloadTask::operator()() {
    try {
        if (freeAfterDownload) engine->indexAndDeleteFile(file->path);
        else engine->downloadFile(file->path);
        promise.set_value(true);
    } catch (const std::exception& e) {
        LogRegistry::sync()->error("[DownloadTask] Failed to download file: {} - {}", file->path.string(), e.what());
        promise.set_value(false);
    }
}

