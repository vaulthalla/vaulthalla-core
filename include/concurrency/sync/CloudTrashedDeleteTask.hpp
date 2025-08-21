#pragma once

#include "concurrency/Task.hpp"

#include <memory>
#include <filesystem>

namespace vh::storage {
class CloudStorageEngine;
}

namespace vh::types {
struct TrashedFile;
}

namespace vh::concurrency {

struct CloudTrashedDeleteTask final : PromisedTask {
    enum class Type { PURGE, LOCAL, REMOTE };

    std::shared_ptr<storage::CloudStorageEngine> engine;
    std::shared_ptr<types::TrashedFile> file;
    Type type{Type::PURGE};

    CloudTrashedDeleteTask(std::shared_ptr<storage::CloudStorageEngine> eng,
                 std::shared_ptr<types::TrashedFile> f,
                 const Type& type = Type::PURGE);

    void operator()() override;

    void purge(const std::filesystem::path& path) const;
    void handleLocalDelete() const;
};

}
