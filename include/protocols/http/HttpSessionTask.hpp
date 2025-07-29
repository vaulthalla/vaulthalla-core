#pragma once

#include "concurrency/Task.hpp"
#include "protocols/http/HttpSession.hpp"

namespace vh::concurrency {

struct HttpSessionTask : Task {
    std::shared_ptr<http::HttpSession> session;

    explicit HttpSessionTask(std::shared_ptr<http::HttpSession> s) : session(std::move(s)) {}

    void operator()() override {
        session->run();
    }
};


}
