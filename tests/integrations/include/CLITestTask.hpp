#pragma once

#include "TestCase.hpp"
#include "TestTask.hpp"
#include "CommandRouter.hpp"

namespace vh::test::cli {

class CLITestTask final : public PromisedTestTask {
public:
    explicit CLITestTask(const std::shared_ptr<CommandRouter>& router,
                         const std::vector<std::shared_ptr<TestCase>>& tests)
        : PromisedTestTask(), router_(router), tests_(tests) {
        if (!router_) throw std::runtime_error("CLITestTask: router is null");
    }
    ~CLITestTask() override = default;

    void operator()() override {
        try { promise.set_value(router_->route(tests_)); }
        catch (const std::exception& e) { promise.set_exception(std::current_exception()); }
    }

private:
    std::shared_ptr<CommandRouter> router_;
    std::vector<std::shared_ptr<TestCase>> tests_;
};

}
