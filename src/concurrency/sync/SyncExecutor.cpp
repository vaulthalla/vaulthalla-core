#include "concurrency/sync/SyncExecutor.hpp"
#include "concurrency/sync/SyncTask.hpp"
#include "types/sync/Action.hpp"

using namespace vh::concurrency;
using namespace vh::types::sync;

void SyncExecutor::run(const std::shared_ptr<SyncTask>& ctx, const std::vector<Action>& plan) {
    // Option: stable grouping for determinism & perf
    // EnsureDirectories first, then uploads, then downloads, then deletes
    for (const auto type : {ActionType::EnsureDirectories, ActionType::Upload, ActionType::Download,
                      ActionType::DeleteRemote, ActionType::DeleteLocal}) {
        for (const auto& a : plan) {
            if (a.type != type) continue;
            dispatch(ctx, a);
        }
        ctx->processFutures(); // barrier between phases
        }
}

void SyncExecutor::dispatch(const std::shared_ptr<SyncTask>& ctx, const Action& action) {
    switch (action.type) {
    case ActionType::EnsureDirectories:
        ctx->ensureDirectoriesFromRemote(); // extracted shared function
        return;
    case ActionType::Upload:
        ctx->upload(action.local);
        return;
    case ActionType::Download:
        // if cache mode sets freeAfterDownload, forward it
        ctx->download(action.remote ? action.remote : action.local, action.freeAfterDownload);
        return;
    case ActionType::DeleteLocal:
        ctx->remove(action.local, CloudDeleteTask::Type::LOCAL);
        return;
    case ActionType::DeleteRemote:
        ctx->remove(action.remote, CloudDeleteTask::Type::REMOTE);
        return;
    }
}

