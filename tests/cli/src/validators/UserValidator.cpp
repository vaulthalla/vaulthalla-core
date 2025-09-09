#include "validators/UserValidator.hpp"
#include "types/User.hpp"
#include "database/Queries/UserQueries.hpp"

using namespace vh::test::cli;
using namespace vh::types;
using namespace vh::database;

AssertionResult UserValidator::assert_user_exists(const std::shared_ptr<types::User>& user) {
    if (!UserQueries::userExists(user->name)) return AssertionResult::Fail("User '" + user->name + "' not found in DB");
    return AssertionResult::Pass();
}

AssertionResult UserValidator::assert_user_not_exists(const std::shared_ptr<types::User>& user) {
    if (UserQueries::userExists(user->name)) return AssertionResult::Fail("User '" + user->name + "' unexpectedly found in DB");
    return AssertionResult::Pass();
}

AssertionResult UserValidator::assert_user_count_at_least(const unsigned int count) {
    const auto actual = UserQueries::listUsers().size();
    if (actual < count) return AssertionResult::Fail("Expected at least " + std::to_string(count) + " users, found " + std::to_string(actual));
    return AssertionResult::Pass();
}

AssertionResult UserValidator::assert_user_equality(const std::shared_ptr<types::User>& expected) {
    const auto actual = UserQueries::getUserByName(expected->name);
    if (!actual) return AssertionResult::Fail("User '" + expected->name + "' not found in DB for equality check");
    if (*actual != *expected) return AssertionResult::Fail("User '" + expected->name + "' does not match expected values");
    return AssertionResult::Pass();
}
