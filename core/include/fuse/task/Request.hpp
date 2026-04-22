#pragma once

#include "concurrency/Task.hpp"
#include "log/Registry.hpp"

#include <fuse3/fuse_lowlevel.h>
#include <cstdlib>
#include <unistd.h>

namespace vh::fuse::task {

class Request final : public concurrency::Task {
public:
    Request(fuse_session* session, const fuse_buf& buf) : session_(session), buf_(buf) {}
    ~Request() override { releaseBuf(); }

    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    Request(Request&&) = delete;
    Request& operator=(Request&&) = delete;

    void operator()() override {
        try {
            fuse_session_process_buf(session_, &buf_);
        } catch (const std::exception& ex) {
            log::Registry::fuse()->critical("[fuse::task::Request] Unhandled exception: {}", ex.what());
        } catch (...) {
            log::Registry::fuse()->critical("[fuse::task::Request] Unhandled unknown exception");
        }

        releaseBuf();
    }

private:
    void releaseBuf() {
        if (released_) return;

        if ((buf_.flags & FUSE_BUF_IS_FD) != 0) {
            if (buf_.fd >= 0) ::close(buf_.fd);
        } else if (buf_.mem) {
            std::free(buf_.mem);
        }

        buf_.mem = nullptr;
        buf_.size = 0;
        buf_.fd = -1;
        buf_.pos = 0;
        buf_.flags = static_cast<fuse_buf_flags>(0);
        released_ = true;
    }

    fuse_session* session_;
    fuse_buf buf_;
    bool released_ = false;
};

}
