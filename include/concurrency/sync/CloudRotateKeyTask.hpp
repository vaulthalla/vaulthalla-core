#pragma once

#include "concurrency/Task.hpp"

#include <memory>
#include <vector>

namespace vh::storage {
class CloudStorageEngine;
}

namespace vh::types {
struct File;
}

namespace vh::concurrency {

struct CloudRotateKeyTask final : PromisedTask {

    std::shared_ptr<storage::CloudStorageEngine> engine;
    std::vector<std::shared_ptr<types::File>> files;
    unsigned int begin, end;

    CloudRotateKeyTask(std::shared_ptr<storage::CloudStorageEngine> eng,
                       const std::vector<std::shared_ptr<types::File>>& f,
                       unsigned int begin, unsigned int end);

    void operator()() override;
};

}
