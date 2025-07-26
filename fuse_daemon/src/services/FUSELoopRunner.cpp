#include "services/FUSELoopRunner.hpp"
#include "FUSEBridge.hpp"
#include "config/ConfigRegistry.hpp"
#include "concurrency/ThreadPool.hpp"
#include "services/ThreadPoolRegistry.hpp"
#include "FUSERequestTask.hpp"

#include <iostream>
#include <cstring>

using namespace vh::concurrency;
using namespace vh::services;

namespace vh::fuse {

FUSELoopRunner::FUSELoopRunner(const std::shared_ptr<storage::StorageManager>& storageManager)
    : bridge_(std::make_shared<FUSEBridge>(storageManager)) {}

void fuse_ll_init(void* userdata, fuse_conn_info* conn) {
    std::cout << "[+] Initializing FUSE connection\n";

    constexpr uintmax_t MB = 1024 * 1024;

    // Optional flags you want to request
    conn->want |= FUSE_CAP_ASYNC_READ;
    conn->want |= FUSE_CAP_WRITEBACK_CACHE;
    conn->max_readahead = MB;
    conn->max_write = MB;

    std::cout << "    max_readahead: " << conn->max_readahead << "\n";
    std::cout << "    max_write:     " << conn->max_write << "\n";
}

bool FUSELoopRunner::run() {
    std::cout << "Starting Vaulthalla FUSE daemon..." << std::endl;

    std::vector<std::string> argsStr = {
        "vaulthalla-fuse",
        "-f",
        config::ConfigRegistry::get().fuse.root_mount_path.string()
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
        std::cerr << "[-] fuse_opt_parse failed\n";
        return false;
    }

    fuse_cmdline_opts opts;
    if (fuse_parse_cmdline(&args, &opts) != 0) {
        std::cerr << "[-] Failed to parse FUSE options\n";
        return false;
    }

    fuse_lowlevel_ops ops = bridge_->getOperations();
    ops.init = fuse_ll_init;

    session_ = fuse_session_new(&args, &ops, sizeof(ops), bridge_.get());
    if (!session_) {
        std::cerr << "[-] fuse_session_new failed\n";
        free(opts.mountpoint);
        fuse_opt_free_args(&args);
        return false;
    }

    if (fuse_set_signal_handlers(session_) != 0) {
        std::cerr << "[-] fuse_set_signal_handlers failed\n";
        fuse_session_destroy(session_);
        free(opts.mountpoint);
        fuse_opt_free_args(&args);
        return false;
    }

    if (fuse_session_mount(session_, opts.mountpoint) != 0) {
        std::cerr << "[-] fuse_session_mount failed\n";
        fuse_remove_signal_handlers(session_);
        fuse_session_destroy(session_);
        free(opts.mountpoint);
        fuse_opt_free_args(&args);
        return false;
    }

    std::cout << "[+] FUSE mounted at " << opts.mountpoint << "\n";

    while (!fuse_session_exited(session_)) {
        fuse_buf buf{};
        int res = fuse_session_receive_buf(session_, &buf);
        if (res == -EINTR) continue;
        if (res <= 0) break;

        ThreadPoolRegistry::instance().fusePool()->submit(std::make_shared<FUSERequestTask>(session_, buf));
    }

    fuse_session_unmount(session_);
    fuse_remove_signal_handlers(session_);
    fuse_session_destroy(session_);
    free(opts.mountpoint);
    fuse_opt_free_args(&args);
    return true;
}

}
