#pragma once

#include "concurrency/Task.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "types/File.hpp"
#include "types/CacheIndex.hpp"

#include <memory>
#include <iostream>

namespace vh::concurrency {

struct DeleteTask : PromisedTask {
    enum class Type { PURGE, LOCAL, REMOTE };

    std::shared_ptr<storage::CloudStorageEngine> engine;
    std::shared_ptr<types::File> file;
    Type type{Type::PURGE};

    DeleteTask(std::shared_ptr<storage::CloudStorageEngine> eng,
                 std::shared_ptr<types::File> f,
                 const Type& type = Type::PURGE)
        : engine(std::move(eng)), file(std::move(f)), type(type) {}

    void operator()() override {
        try {
            if (type == Type::PURGE) engine->purge(file->path);
            else if (type == Type::LOCAL) engine->removeLocally(file->path);
            else if (type == Type::REMOTE) engine->removeRemotely(file->path);
            else throw std::runtime_error("Unknown delete task type");
            promise.set_value(true);
        } catch (const std::exception& e) {
            std::cerr << "[DeleteTask] Error: " << e.what() << " (path = " << file->path << ")\n";
            promise.set_value(false);
        }
    }
};

}
