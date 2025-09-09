#include "CLITestRunner.hpp"
#include "TestUsageManager.hpp"
#include "logging/LogRegistry.hpp"
#include "CLITestContext.hpp"
#include "Validator.hpp"
#include "EntityRegistrar.hpp"

#include <iostream>
#include <algorithm>

using namespace vh::shell;
using namespace vh::args;

namespace vh::test::cli {

CLITestRunner::CLITestRunner(CLITestConfig&& config)
    : config_(config),
      usage_(std::make_shared<TestUsageManager>()),
      ctx_(std::make_shared<CLITestContext>()) {}

void CLITestRunner::registerStdoutContains(const std::string& path, std::string s) {
    per_path_stdout_contains_[path].push_back(std::move(s));
}
void CLITestRunner::registerStdoutNotContains(const std::string& path, std::string s) {
    per_path_stdout_not_contains_[path].push_back(std::move(s));
}

int CLITestRunner::operator()(std::ostream& out) {
    int failures = 0;
    size_t idx = 0;
    for (const auto& t : tests_) {
        out << "[" << (++idx) << "/" << tests_.size() << "] " << t.name << "\n";
        out << "  $ " << t.command << "\n";
        CommandResult r = exec_(t.command);

        auto fail = [&](const std::string& msg){
            ++failures;
            out << "  ✗ " << msg << "\n";
        };

        if (r.exit_code != t.expect_exit) {
            fail("exit_code: expected " + std::to_string(t.expect_exit) +
                 " got " + std::to_string(r.exit_code));
            out << "  stdout: " << r.stdout_text << "\n";
            out << "  stderr: " << r.stderr_text << "\n";
            continue;
        }

        for (const auto& needle : t.must_contain) {
            if (r.stdout_text.find(needle) == std::string::npos &&
                r.stderr_text.find(needle) == std::string::npos) {
                fail("missing expected text: \"" + needle + "\"");
                }
        }

        for (const auto& needle : t.must_not_contain) {
            if (r.stdout_text.find(needle) != std::string::npos ||
                r.stderr_text.find(needle) != std::string::npos) {
                fail("found forbidden text: \"" + needle + "\"");
                }
        }

        for (const auto& v : t.validators) {
            auto ar = v(t.command, r, t.ctx);
            if (!ar.ok) fail("validator: " + ar.message);
        }

        if (failures == 0 || idx == tests_.size()) {
            out << "  ✓ ok\n";
        }
    }

    registrar_->teardown();

    out << "\nSummary: " << (tests_.size() - failures) << " passed, "
        << failures << " failed, total " << tests_.size() << "\n";
    return failures;
}

void CLITestRunner::validateTestObjects() const {
    Validator<EntityType::USER, User>::assert_all_exist(ctx_->users);
    Validator<EntityType::VAULT, Vault>::assert_all_exist(ctx_->vaults);
    Validator<EntityType::GROUP, Group>::assert_all_exist(ctx_->groups);
    Validator<EntityType::USER_ROLE, UserRole>::assert_all_exist(ctx_->userRoles);
    Validator<EntityType::VAULT_ROLE, VaultRole>::assert_all_exist(ctx_->vaultRoles);
}

}
