#pragma once

#include "CLITestConfig.hpp"

#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

namespace vh::test::cli {

class CommandRouter;
struct TestUsageManager;
class EntityRegistrar;
class ListInfoHandler;
class UpdateHandler;
struct TestCase;

struct TestStage {
    std::string name;
    std::vector<std::shared_ptr<TestCase>> tests;
};

class CLITestRunner {
public:
    explicit CLITestRunner(CLITestConfig&& config = CLITestConfig::Default());

    void registerStdoutContains(const std::string& path, std::string needle);
    void registerStdoutNotContains(const std::string& path, std::string needle);
    void registerStdoutContains(const std::string& path, std::vector<std::string> needles);
    void registerStdoutNotContains(const std::string& path, std::vector<std::string> needles);

    int operator()();

private:
    CLITestConfig config_;
    std::shared_ptr<CLITestContext> ctx_;
    std::shared_ptr<TestUsageManager> usage_;
    std::shared_ptr<CommandRouter> router_;

    std::unordered_map<std::string, std::vector<std::string>> per_path_stdout_contains_;
    std::unordered_map<std::string, std::vector<std::string>> per_path_stdout_not_contains_;
    std::array<TestStage, 5> kDefaultTestStages;

    void validateResponse(unsigned int stage) const;

    void seedRoles();
    void seed();
    void update();
    void read();
    void teardown();
    int printResults() const;

    void validateTestObjects() const;
};

}
