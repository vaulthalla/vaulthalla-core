#pragma once

#include "protocols/shell/types.hpp"

#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

namespace vh::test::cli {

struct TestUsageManager;
class EntityRegistrar;
class ListInfoHandler;
class UpdateHandler;
struct TestCase;

struct TestStage {
    std::string name;
    std::vector<TestCase> tests_;
};

struct CLITestConfig {
    size_t numUsers = 10,
           numVaults = 15,
           numGroups = 5,
           numUserRoles = 7,
           numVaultRoles = 7;

    static CLITestConfig Default() { return CLITestConfig{}; }

    static CLITestConfig Minimal() {
        return CLITestConfig{
            .numUsers = 2,
            .numVaults = 2,
            .numGroups = 1,
            .numUserRoles = 1,
            .numVaultRoles = 1
        };
    }

    static CLITestConfig Large() {
        return CLITestConfig{
            .numUsers = 50,
            .numVaults = 75,
            .numGroups = 20,
            .numUserRoles = 15,
            .numVaultRoles = 15
        };
    }

    static CLITestConfig ExtraLarge() {
        return CLITestConfig{
            .numUsers = 100,
            .numVaults = 150,
            .numGroups = 50,
            .numUserRoles = 25,
            .numVaultRoles = 25
        };
    }

    static CLITestConfig NG_STRESS() {
        return CLITestConfig{
            .numUsers = 500,
            .numVaults = 750,
            .numGroups = 100,
            .numUserRoles = 50,
            .numVaultRoles = 50
        };
    }
};

class CLITestRunner {
public:
    explicit CLITestRunner(CLITestConfig&& config = CLITestConfig::Default());

    void registerStdoutContains(const std::string& path, std::string needle);
    void registerStdoutNotContains(const std::string& path, std::string needle);

    int operator()(std::ostream& out);

private:
    CLITestConfig config_;
    std::shared_ptr<TestUsageManager> usage_;
    std::shared_ptr<CLITestContext> ctx_;

    std::shared_ptr<EntityRegistrar> registrar_;
    std::shared_ptr<ListInfoHandler> lister_;
    std::shared_ptr<UpdateHandler> updater_;

    std::unordered_map<std::string, std::vector<std::string>> per_path_stdout_contains_;
    std::unordered_map<std::string, std::vector<std::string>> per_path_stdout_not_contains_;
    std::array<TestStage, 6> kDefaultTestStages;

    void seedTests();

    void runTest(const TestCase& t, std::ostream& out, int& failures, int& total, int indent = 0) const;

    void validateTestObjects() const;
};

}
