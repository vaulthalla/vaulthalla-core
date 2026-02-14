#include "concurrency/sync/UploadTask.hpp"
#include "storage/cloud/CloudStorageEngine.hpp"
#include "types/fs/File.hpp"
#include "logging/LogRegistry.hpp"
#include "types/sync/ScopedOp.hpp"

using namespace vh::concurrency;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::logging;

UploadTask::UploadTask(std::shared_ptr<CloudStorageEngine> eng, std::shared_ptr<File> f, sync::ScopedOp& op)
    : engine(std::move(eng)), file(std::move(f)), op(op) {}

void UploadTask::operator()() {
    try {
        op.start(file->size_bytes);
        engine->upload(file);
        op.stop();
        promise.set_value(true);
    } catch (const std::exception& e) {
        LogRegistry::sync()->error("[UploadTask] Failed to upload file: {} - {}", file->path.string(), e.what());
        op.stop();
        promise.set_value(false);
    }
}

