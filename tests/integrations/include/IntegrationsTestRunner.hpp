#pragma once

#include "CLITestConfig.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <optional>

namespace vh::protocols::shell { class UsageManager; }
namespace vh::identities::model { struct User; }
namespace vh::rbac::model { struct PermissionOverride; }

namespace vh::test::cli {

class TestThreadPool;
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

class IntegrationsTestRunner {
public:
    explicit IntegrationsTestRunner(CLITestConfig&& cfg = CLITestConfig::Default());

    void registerStdoutContains(const std::string& path, std::string needle);
    void registerStdoutNotContains(const std::string& path, std::string needle);
    void registerStdoutContains(const std::string& path, std::vector<std::string> needles);
    void registerStdoutNotContains(const std::string& path, std::vector<std::string> needles);

    int operator()();

    static std::optional<unsigned int> extractId(std::string_view output, std::string_view idPrefix);

private:
    CLITestConfig config_;
    std::shared_ptr<CLITestContext> ctx_;
    std::shared_ptr<protocols::shell::UsageManager> usage_;
    std::shared_ptr<CommandRouter> router_;
    std::shared_ptr<std::atomic<bool>> interruptFlag;
    std::shared_ptr<TestThreadPool> threadPool_;

    // Expectations are keyed by command path
    std::unordered_map<std::string, Expectations> expectations_by_path_;

    // Pipeline stages executed in order
    std::vector<TestStage> stages_;

    // Open Linux users
    std::vector<unsigned int> linux_uids_;
    std::vector<unsigned int> linux_gids_;

    void registerAllContainsAssertions();

    // Pipeline steps
    void seed();
    void assign();
    void readStage();
    void updateStage();
    void teardownStage();

    // FUSE steps
    std::shared_ptr<identities::model::User> createUser(unsigned int vaultId, const std::optional<uint16_t>& vaultPerms = std::nullopt, const std::vector<std::shared_ptr<rbac::model::PermissionOverride>>& overrides = {});
    void runFUSETests();
    void testFUSECRUD();
    void testFUSEAllow();
    void testFUSEDeny();
    void testVaultPermOverridesAllow();
    void testVaultPermOverridesDeny();
    void testFUSEGroupPermissions();
    void testGroupPermOverrides();
    void testFUSEUserOverridesGroupOverride();

    // Helpers
    void validateStage(const TestStage& stage) const;
    void validateAllTestObjects() const;
    int  printResults() const;

    template <EntityType E>
    void finish_seed(const std::vector<std::shared_ptr<TestCase>>& res);
};

}
