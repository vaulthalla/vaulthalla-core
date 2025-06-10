#include <gtest/gtest.h>
#include "auth/AuthManager.hpp"
#include "database//DBConnection.hpp"
#include "database/queries/UserQueries.hpp"
#include "database/Transactions.hpp"
#include "types/User.hpp"
#include <memory>

class AuthManagerTest : public ::testing::Test {
protected:
    std::shared_ptr<vh::database::DBConnection> db_;
    std::unique_ptr<vh::auth::AuthManager> authManager_;

    void SetUp() override {
        db_ = std::make_shared<vh::database::DBConnection>();
        authManager_ = std::make_unique<vh::auth::AuthManager>();

        vh::database::runTransaction("AuthManagerTest::SetUp", [&](pqxx::work& txn) {
            txn.exec("DELETE FROM users;");
        });
    }

    void TearDown() override {
        vh::database::runTransaction("AuthManagerTest::TearDown", [&](pqxx::work& txn) {
            // Ensure the users table is empty after each test
            txn.exec("DELETE FROM users;");
        });
    }
};

TEST_F(AuthManagerTest, RegisterUser_Success) {
    auto user = authManager_->registerUser("Cooper Test", "cooper.test@example.com", "password123");

    ASSERT_NE(user, nullptr);
    EXPECT_EQ(user->name, "Cooper Test");
    EXPECT_EQ(user->email, "cooper.test@example.com");
    // NOTE: Do not test raw password — assume it's hashed in DB

    auto fetched = std::make_unique<vh::types::User>(vh::database::UserQueries::getUserByEmail("cooper.test@example.com"));
    ASSERT_NE(fetched, nullptr);
    EXPECT_EQ(fetched->name, "Cooper Test");
    EXPECT_EQ(fetched->email, "cooper.test@example.com");
}

TEST_F(AuthManagerTest, RegisterUser_DuplicateEmail_Fails) {
    authManager_->registerUser("First User", "duplicate@example.com", "password123");
    EXPECT_THROW({ authManager_->registerUser("Second User", "duplicate@example.com", "diffpassword"); }, std::exception);
}

TEST_F(AuthManagerTest, RegisterUser_InvalidEmail_Fails) {
    EXPECT_THROW({ authManager_->registerUser("Bad User", "", "password123"); }, std::exception);
}

TEST_F(AuthManagerTest, RegisterUser_InvalidPassword_Fails) {
    EXPECT_THROW({ authManager_->registerUser("Bad User", "baduser@example.com", ""); }, std::exception);
}
