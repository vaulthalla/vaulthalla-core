#include "FUSELoopRunner.hpp"
#include "FUSEOperations.hpp"
#include "config/ConfigRegistry.hpp"
#include "concurrency/ThreadPoolRegistry.hpp"
#include "concurrency/ThreadPool.hpp"
#include "FUSERequestTask.hpp"

#include <fuse3/fuse_lowlevel.h>
#include <iostream>

using namespace vh::concurrency;

namespace vh::fuse {

bool FUSELoopRunner::run() {
    fuse_args args = FUSE_ARGS_INIT(0, nullptr);

    fuse_lowlevel_ops ops = getOperations();
    session_ = fuse_session_new(&args, &ops, sizeof(ops), nullptr);
    if (!session_) {
        std::cerr << "[-] fuse_session_new failed.\n";
        return false;
    }

    const auto& mountpoint = config::ConfigRegistry::get().fuse.root_mount_path;

    if (fuse_session_mount(session_, mountpoint.c_str()) != 0) {
        std::cerr << "[-] fuse_session_mount failed.\n";
        fuse_session_destroy(session_);
        return false;
    }

    std::cout << "[+] FUSE mounted at " << mountpoint << "\n";

    while (!fuse_session_exited(session_)) {
        fuse_buf buf;
        int res = fuse_session_receive_buf(session_, &buf);

        if (res == -EINTR) continue;
        if (res <= 0) break;

        threadPool_->submit(std::make_shared<FUSERequestTask>(session_, buf));
    }

    fuse_session_unmount(session_);
    fuse_session_destroy(session_);
    return true;
}

}
