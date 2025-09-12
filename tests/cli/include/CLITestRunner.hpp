#pragma once

#include "CLITestConfig.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace vh::shell {
class UsageManager;
}

namespace vh::test::cli {

struct CLITestContext;
class CommandRouter;
struct TestCase;
enum class EntityType;

struct TestStage {
    std::string name;
    std::vector<std::shared_ptr<TestCase>> tests;
};

struct Expectations {
    std::vector<std::string> must_have;
    std::vector<std::string> must_not_have;
};

class CLITestRunner {
public:
    explicit CLITestRunner(CLITestConfig&& cfg = CLITestConfig::Default());

    void registerStdoutContains(const std::string& path, std::string needle);
    void registerStdoutNotContains(const std::string& path, std::string needle);
    void registerStdoutContains(const std::string& path, std::vector<std::string> needles);
    void registerStdoutNotContains(const std::string& path, std::vector<std::string> needles);

    int operator()();

    static std::optional<unsigned int> extractId(std::string_view output, std::string_view idPrefix);

private:
    CLITestConfig config_;
    std::shared_ptr<CLITestContext>   ctx_;
    std::shared_ptr<shell::UsageManager> usage_;
    std::shared_ptr<CommandRouter>    router_;

    // Expectations are keyed by command path
    std::unordered_map<std::string, Expectations> expectations_by_path_;

    // Pipeline stages executed in order
    std::vector<TestStage> stages_;

    void registerAllContainsAssertions();

    // Pipeline steps
    void seed();
    void readStage();
    void updateStage();
    void teardownStage();

    // Helpers
    void validateStage(const TestStage& stage) const;
    void validateAllTestObjects() const;
    int  printResults() const;

    template <EntityType E>
    void seed(size_t count);
};

}
