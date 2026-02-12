#include "concurrency/sync/CloudDeleteTask.hpp"
#include "storage/cloud/CloudStorageEngine.hpp"
#include "types/fs/File.hpp"
#include "logging/LogRegistry.hpp"

using namespace vh::concurrency;
using namespace vh::types;
using namespace vh::database;
using namespace vh::storage;
using namespace vh::logging;

CloudDeleteTask::CloudDeleteTask(std::shared_ptr<CloudStorageEngine> eng, std::shared_ptr<File> f, const Type& type)
    : engine(std::move(eng)), file(std::move(f)), type(type) {}


void CloudDeleteTask::operator()() {
    try {
        if (type == Type::PURGE) engine->purge(file->path);
        else if (type == Type::LOCAL) engine->removeLocally(file->path);
        else if (type == Type::REMOTE) engine->removeRemotely(file->path);
        else throw std::runtime_error("Unknown delete task type");
        promise.set_value(true);
    } catch (const std::exception& e) {
        LogRegistry::sync()->error("[CloudDeleteTask] Failed to delete file: {} - {}", file->path.string(), e.what());
        promise.set_value(false);
    }
}
