#include "services/FUSE.hpp"
#include "services/ServiceManager.hpp"
#include "storage/FUSEBridge.hpp"
#include "concurrency/ThreadPool.hpp"
#include "concurrency/ThreadPoolManager.hpp"
#include "concurrency/FUSERequestTask.hpp"
#include "logging/LogRegistry.hpp"

#include <cstring>
#include <thread>
#include <atomic>
#include <paths.h>

using namespace vh::services;
using namespace vh::concurrency;
using namespace vh::fuse;
using namespace vh::logging;

FUSE::FUSE()
    : AsyncService("FUSE") {}

void fuse_ll_init(void* userdata, fuse_conn_info* conn) {
    LogRegistry::fuse()->debug("[FUSE] Initializing FUSE connection...");

    constexpr uintmax_t MB = 1024 * 1024;

    conn->want |= FUSE_CAP_ASYNC_READ;
    conn->want |= FUSE_CAP_WRITEBACK_CACHE;
    conn->max_readahead = MB;
    conn->max_write = MB;

    LogRegistry::fuse()->debug("[FUSE] Connection initialized with max_readahead={} bytes, max_write={} bytes",
                              conn->max_readahead, conn->max_write);
}

void FUSE::stop() {
    if (!isRunning()) return;

    LogRegistry::fuse()->info("[FUSE] Stopping FUSE connection...");
    interruptFlag_.store(true);

    if (session_) {
        fuse_session_exit(session_);
        fuse_session_unmount(session_);
        fuse_remove_signal_handlers(session_);
        fuse_session_destroy(session_);
        session_ = nullptr;
    }

    // Only join if we’re not calling stop() from the same thread
    if (worker_.joinable() && std::this_thread::get_id() != worker_.get_id()) worker_.join();

    running_.store(false);
    interruptFlag_.store(false);

    LogRegistry::fuse()->info("[FUSE] FUSE service stopped");
}

void FUSE::runLoop() {
    LogRegistry::fuse()->debug("[FUSE] Running FUSE service");

    std::vector<std::string> argsStr = {
        "vaulthalla-fuse",
        "-f",
        "-o", "allow_other",
        paths::getMountPath()
    };

    std::vector<std::unique_ptr<char[]>> ownedCStrs;
    std::vector<char*> argsCStr;
    for (const auto& str : argsStr) {
        auto buf = std::make_unique<char[]>(str.size() + 1);
        std::memcpy(buf.get(), str.c_str(), str.size() + 1);
        argsCStr.push_back(buf.get());
        ownedCStrs.push_back(std::move(buf));
    }

    fuse_args args = FUSE_ARGS_INIT(static_cast<int>(argsCStr.size()), argsCStr.data());

    if (fuse_opt_parse(&args, nullptr, nullptr, nullptr) == -1) {
        LogRegistry::fuse()->error("[FUSE] Failed to parse FUSE options");
        return;
    }

    fuse_cmdline_opts opts{};
    if (fuse_parse_cmdline(&args, &opts) != 0) {
        LogRegistry::fuse()->error("[FUSE] Failed to parse FUSE options");
        return;
    }

    fuse_lowlevel_ops ops = getOperations();
    ops.init = fuse_ll_init;

    session_ = fuse_session_new(&args, &ops, sizeof(ops), nullptr);
    if (!session_) {
        LogRegistry::fuse()->error("[FUSE] Failed to create FUSE session");
        free(opts.mountpoint);
        fuse_opt_free_args(&args);
        return;
    }

    if (fuse_set_signal_handlers(session_) != 0) {
        LogRegistry::fuse()->error("[FUSE] Failed to set signal handlers");
        fuse_session_destroy(session_);
        free(opts.mountpoint);
        fuse_opt_free_args(&args);
        return;
    }

    if (fuse_session_mount(session_, opts.mountpoint) != 0) {
        LogRegistry::fuse()->error("[FUSE] Failed to mount FUSE filesystem at {}", opts.mountpoint);
        fuse_remove_signal_handlers(session_);
        fuse_session_destroy(session_);
        free(opts.mountpoint);
        fuse_opt_free_args(&args);
        return;
    }

    LogRegistry::fuse()->info("[FUSE] Mounted FUSE filesystem at {}", opts.mountpoint);

    while (!fuse_session_exited(session_) && running_) {
        fuse_buf buf{};
        const int res = fuse_session_receive_buf(session_, &buf);
        if (res == -EINTR) continue;
        if (res <= 0) break;

        ThreadPoolManager::instance().fusePool()->submit(std::make_shared<FUSERequestTask>(session_, buf));
    }

    LogRegistry::fuse()->info("[FUSE] FUSE service loop exiting");

    fuse_session_unmount(session_);
    fuse_remove_signal_handlers(session_);
    fuse_session_destroy(session_);
    session_ = nullptr;

    free(opts.mountpoint);
    fuse_opt_free_args(&args);

    LogRegistry::fuse()->info("[FUSE] FUSE service stopped successfully");
}
