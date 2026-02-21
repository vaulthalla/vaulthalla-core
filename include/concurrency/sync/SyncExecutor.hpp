#pragma once

#include <memory>
#include <vector>

namespace vh::types::sync {
struct Action;
}

namespace vh::concurrency {

struct SyncTask;

class SyncExecutor {
public:
    static void run(const std::shared_ptr<SyncTask>& ctx, const std::vector<types::sync::Action>& plan);

private:
    static void dispatch(const std::shared_ptr<SyncTask>& ctx, const types::sync::Action& action);
};

}
