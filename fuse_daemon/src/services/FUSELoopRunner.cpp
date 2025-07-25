#include "services/FUSELoopRunner.hpp"
#include "FUSEBridge.hpp"
#include "config/ConfigRegistry.hpp"
#include "concurrency/ThreadPool.hpp"
#include "services/ThreadPoolRegistry.hpp"
#include "FUSERequestTask.hpp"

#include <iostream>

using namespace vh::concurrency;
using namespace vh::services;

namespace vh::fuse {

FUSELoopRunner::FUSELoopRunner(const std::shared_ptr<storage::StorageManager>& storageManager)
    : bridge_(std::make_shared<FUSEBridge>(storageManager)) {}

bool FUSELoopRunner::run() {
    const auto& mountpoint = config::ConfigRegistry::get().fuse.root_mount_path;

    std::vector<std::string> argsStr = {
        "vaulthalla-fuse",
        "-f",
        "-o",
        "big_writes,max_readahead=1048576,auto_cache,attr_timeout=60,entry_timeout=60,negative_timeout=10",
        mountpoint
    };

    std::vector<char*> argsCStr;
    for (auto& s : argsStr) argsCStr.push_back(s.data());

    fuse_args args = FUSE_ARGS_INIT(static_cast<int>(argsCStr.size()), argsCStr.data());

    const fuse_lowlevel_ops ops = bridge_->getOperations();
    session_ = fuse_session_new(&args, &ops, sizeof(ops), bridge_.get());
    if (!session_) {
        std::cerr << "[-] fuse_session_new failed.\n";
        return false;
    }

    if (fuse_session_mount(session_, mountpoint.c_str()) != 0) {
        std::cerr << "[-] fuse_session_mount failed.\n";
        fuse_session_destroy(session_);
        return false;
    }

    std::cout << "[+] FUSE mounted at " << mountpoint << "\n";

    while (!fuse_session_exited(session_)) {
        fuse_buf buf{};
        const int res = fuse_session_receive_buf(session_, &buf);
        if (res == -EINTR) continue;
        if (res <= 0) break;

        ThreadPoolRegistry::instance().fusePool()->submit(std::make_shared<FUSERequestTask>(session_, buf));
    }

    fuse_session_unmount(session_);
    fuse_session_destroy(session_);
    return true;
}

}
