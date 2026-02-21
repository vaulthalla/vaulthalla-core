#pragma once

#include "types/sync/Action.hpp"

#include <vector>
#include <memory>

namespace vh::concurrency {
class SyncTask;
}

namespace vh::types::sync {

class RemotePolicy;

struct Planner {
    static std::vector<Action> build(const std::shared_ptr<concurrency::SyncTask>& ctx, const std::shared_ptr<RemotePolicy>& policy);
};

}
