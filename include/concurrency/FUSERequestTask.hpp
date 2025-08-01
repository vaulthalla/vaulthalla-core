#pragma once

#include "concurrency/Task.hpp"
#include <fuse3/fuse_lowlevel.h>

namespace vh::fuse {

class FUSERequestTask : public Task {
public:
    FUSERequestTask(fuse_session* session, const fuse_buf& buf) : session_(session), buf_(buf) {}

    void operator()() override { fuse_session_process_buf(session_, &buf_); }

private:
    fuse_session* session_;
    fuse_buf buf_;
};

}
