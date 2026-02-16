#include "concurrency/sync/DownloadTask.hpp"
#include "storage/cloud/CloudStorageEngine.hpp"
#include "types/fs/File.hpp"
#include "logging/LogRegistry.hpp"
#include "types/sync/ScopedOp.hpp"

using namespace vh::concurrency;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::logging;

DownloadTask::DownloadTask(std::shared_ptr<CloudStorageEngine> eng,
                           std::shared_ptr<File> f,
                           sync::ScopedOp& op,
                           const bool freeAfter)
    : engine(std::move(eng)), file(std::move(f)), op(op), freeAfterDownload(freeAfter) {}

void DownloadTask::operator()() {
    try {
        op.start(file->size_bytes);
        if (freeAfterDownload) engine->indexAndDeleteFile(file->path);
        else engine->downloadFile(file->path);
        op.stop();
        promise.set_value(true);
    } catch (const std::exception& e) {
        LogRegistry::sync()->error("[DownloadTask] Failed to download file: {} - {}", file->path.string(), e.what());
        op.stop();
        promise.set_value(false);
    }
}
