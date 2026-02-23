#include "concurrency/DeleteTask.hpp"
#include "storage/cloud/CloudStorageEngine.hpp"
#include "types/fs/TrashedFile.hpp"
#include "logging/LogRegistry.hpp"
#include "types/sync/ScopedOp.hpp"

using namespace vh::concurrency;
using namespace vh::types;
using namespace vh::storage;
using namespace vh::logging;

DeleteTask::DeleteTask(std::shared_ptr<StorageEngine> eng, std::shared_ptr<TrashedFile> f, sync::ScopedOp& op, const Type& type)
    : engine(std::move(eng)), file(std::move(f)), op(op), type(type) {}


void DeleteTask::operator()() {
    try {
        op.start(file->size_bytes);

        if (engine->type() == StorageType::Local) engine->removeLocally(file);
        else if (const auto cloudEngine = std::static_pointer_cast<CloudStorageEngine>(engine)) {
            if (type == Type::PURGE) cloudEngine->purge(file);
            else if (type == Type::LOCAL) cloudEngine->removeLocally(file);
            else if (type == Type::REMOTE) cloudEngine->removeRemotely(file);
            else throw std::runtime_error("Unknown delete task type");
        } else throw std::runtime_error("Unsupported storage engine type");

        op.success = true;
    } catch (const std::exception& e) {
        LogRegistry::sync()->error("[DeleteTask] Failed to delete file: {} - {}", file->path.string(), e.what());
    }

    op.stop();
    promise.set_value(op.success);
}
