#pragma once

#include "concurrency/FSTask.hpp"

#include <memory>

namespace vh::storage {
class StorageEngine;
}

namespace vh::types {
struct Operation;
}

namespace vh::concurrency {

class LocalFSTask final : public FSTask {
public:
    ~LocalFSTask() override = default;

    explicit LocalFSTask(const std::shared_ptr<storage::StorageEngine>& engine) : FSTask(engine) {}

    void operator()() override;

protected:
    void removeTrashedFiles() override;

    std::shared_ptr<storage::StorageEngine> localEngine() const;

    void pushKeyRotationTask(const std::vector<std::shared_ptr<types::File>>& files,
                                     unsigned int begin, unsigned int end) override;
};

}
