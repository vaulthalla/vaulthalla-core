#include "sync/tasks/Upload.hpp"
#include "storage/cloud/CloudStorageEngine.hpp"
#include "types/fs/File.hpp"
#include "logging/LogRegistry.hpp"
#include "sync/model/ScopedOp.hpp"

using namespace vh::sync::tasks;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::logging;

Upload::Upload(std::shared_ptr<CloudStorageEngine> eng, std::shared_ptr<File> f, model::ScopedOp& op)
    : engine(std::move(eng)), file(std::move(f)), op(op) {}

void Upload::operator()() {
    try {
        op.start(file->size_bytes);
        engine->upload(file);
        op.success = true;
    } catch (const std::exception& e) {
        LogRegistry::sync()->error("[UploadTask] Failed to upload file: {} - {}", file->path.string(), e.what());
    }

    op.stop();
    promise.set_value(op.success);
}

