#pragma once

#include "../FSTask.hpp"

#include <memory>

namespace vh::storage {
class LocalDiskStorageEngine;
}

namespace vh::types {
struct Operation;
}

namespace vh::concurrency {

class LocalFSTask : public FSTask {
public:
    ~LocalFSTask() override = default;

    LocalFSTask(const std::shared_ptr<storage::StorageEngine>& engine,
             const std::shared_ptr<services::SyncController>& controller) : FSTask(engine, controller) {}

    void operator()() override;

protected:
    void removeTrashedFiles() override;

    std::shared_ptr<storage::LocalDiskStorageEngine> localEngine() const;
};

}
