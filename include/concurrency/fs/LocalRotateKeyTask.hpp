#pragma once

#include "concurrency/Task.hpp"

#include <memory>
#include <vector>

namespace vh::storage {
    class StorageEngine;
}

namespace vh::types {
    struct File;
}

namespace vh::concurrency {

struct LocalRotateKeyTask final : PromisedTask {

    std::shared_ptr<storage::StorageEngine> engine;
    std::vector<std::shared_ptr<types::File>> files;
    unsigned int begin, end;

    LocalRotateKeyTask(std::shared_ptr<storage::StorageEngine> eng,
                       const std::vector<std::shared_ptr<types::File>>& f,
                       unsigned int begin, unsigned int end);

    void operator()() override;
};

}