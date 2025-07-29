#pragma once

#include "concurrency/FSTask.hpp"

#include <memory>

namespace vh::storage {
class StorageEngine;
}

namespace vh::types {
struct Operation;
}

namespace vh::services {
class SyncController;
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

    std::shared_ptr<storage::StorageEngine> localEngine() const;
};

}
