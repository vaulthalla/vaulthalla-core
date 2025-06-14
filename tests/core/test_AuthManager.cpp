//#include <gtest/gtest.h>
//#include "auth/AuthManager.hpp"
//#include "database/queries/UserQueries.hpp"
//#include "database/Transactions.hpp"
//#include "crypto/PasswordUtils.hpp"
//#include <memory>
//
//class AuthManagerTest : public ::testing::Test {
//protected:
//    std::unique_ptr<vh::auth::AuthManager> authManager_;
//
//    void SetUp() override {
//        authManager_ = std::make_unique<vh::auth::AuthManager>();
//        vh::database::Transactions::init();
//        vh::auth::PasswordUtils::loadCommonWeakPasswordsFromURLs({
//            "https://raw.githubusercontent.com/danielmiessler/SecLists/refs/heads/master/Passwords/Common-Credentials/100k-most-used-passwords-NCSC.txt",
//            "https://raw.githubusercontent.com/danielmiessler/SecLists/refs/heads/master/Passwords/Common-Credentials/probable-v2_top-12000.txt"
//        });
//
//        vh::auth::PasswordUtils::loadDictionaryFromURL(
//                "https://raw.githubusercontent.com/dolph/dictionary/refs/heads/master/popular.txt"
//        );
//
//        vh::database::Transactions::exec("AuthManagerTest::SetUp", [&](pqxx::work& txn) {
//            txn.exec("DELETE FROM users;");
//        });
//    }
//
//    void TearDown() override {
//        vh::database::Transactions::exec("AuthManagerTest::TearDown", [&](pqxx::work& txn) {
//            // Ensure the users table is empty after each test
//            txn.exec("DELETE FROM users;");
//        });
//    }
//};
//
//TEST_F(AuthManagerTest, RegisterUser_Success) {
//    auto client = authManager_->registerUser("Cooper Test", "cooper.test@example.com", "fjeljws@1884");
//    auto user = client->getUser();
//
//    ASSERT_NE(user, nullptr);
//    EXPECT_EQ(user->name, "Cooper Test");
//    EXPECT_EQ(user->email, "cooper.test@example.com");
//    // NOTE: Do not test raw password — assume it's hashed in DB
//
//    auto fetched = vh::database::UserQueries::getUserByEmail("cooper.test@example.com");
//    ASSERT_NE(fetched, nullptr);
//    EXPECT_EQ(fetched->name, "Cooper Test");
//    EXPECT_EQ(fetched->email, "cooper.test@example.com");
//}
//
//TEST_F(AuthManagerTest, RegisterUser_DuplicateEmail_Fails) {
//    authManager_->registerUser("First User", "duplicate@example.com", "fjeljws@1884");
//    EXPECT_THROW({ authManager_->registerUser("Second User", "duplicate@example.com", "dwfe23$3212"); }, std::exception);
//}
//
//TEST_F(AuthManagerTest, RegisterUser_InvalidEmail_Fails) {
//    EXPECT_THROW({ authManager_->registerUser("Bad User", "", "fjeljws@1884"); }, std::exception);
//}
//
//TEST_F(AuthManagerTest, RegisterUser_InvalidPassword_Fails) {
//    EXPECT_THROW({ authManager_->registerUser("Bad User", "baduser@example.com", ""); }, std::exception);
//}
