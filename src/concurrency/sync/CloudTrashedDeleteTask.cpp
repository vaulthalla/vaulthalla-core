#include "concurrency/sync/CloudTrashedDeleteTask.hpp"
#include "storage/cloud/CloudStorageEngine.hpp"
#include "types/fs/TrashedFile.hpp"
#include "database/Queries/FileQueries.hpp"
#include "config/Config.hpp"
#include "logging/LogRegistry.hpp"
#include "types/sync/ScopedOp.hpp"

using namespace vh::concurrency;
using namespace vh::types;
using namespace vh::database;
using namespace vh::storage;
using namespace vh::config;
using namespace vh::logging;

CloudTrashedDeleteTask::CloudTrashedDeleteTask(std::shared_ptr<CloudStorageEngine> eng,
                                                 std::shared_ptr<TrashedFile> f,
                                                 sync::ScopedOp& op,
                                                 const Type& type)
    : engine(std::move(eng)), file(std::move(f)), op(op), type(type) {}

void CloudTrashedDeleteTask::operator()() {
    try {
        op.start(file->size_bytes);
        if (type == Type::PURGE) engine->purge(file);
        else if (type == Type::LOCAL) engine->removeLocally(file);
        else if (type == Type::REMOTE) engine->removeRemotely(file);
        else throw std::runtime_error("Unknown delete task type");
        FileQueries::markTrashedFileDeleted(file->id);
        op.success = true;
    } catch (const std::exception& e) {
        LogRegistry::sync()->error("[CloudTrashedDeleteTask] Failed to delete trashed file: {} - {}", file->backing_path.string(), e.what());
    }

    op.stop();
    promise.set_value(op.success);
}
