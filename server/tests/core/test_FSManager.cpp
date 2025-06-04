#include <gtest/gtest.h>
#include <core/FSManager.hpp>
#include <core/LocalDiskStorageEngine.hpp>
#include <filesystem>

namespace fs = std::filesystem;

class FSManagerIntegrationTest : public ::testing::Test {
protected:
    std::shared_ptr<core::StorageEngine> storage;
    std::unique_ptr<core::FSManager> manager;
    fs::path test_dir;

    void SetUp() override {
        test_dir = fs::temp_directory_path() / "vaulthalla_test_dir";
        fs::create_directory(test_dir);
        storage = std::make_shared<core::LocalDiskStorageEngine>(test_dir);
        manager = std::make_unique<core::FSManager>(storage);
    }

    void TearDown() override {
        fs::remove_all(test_dir);
    }
};

TEST_F(FSManagerIntegrationTest, SaveAndReadFile) {
    std::string fileId = "testfile";
    std::vector<uint8_t> content = {1, 2, 3, 4};

    ASSERT_TRUE(manager->saveFile(fileId, content));

    auto result = manager->loadFile(fileId);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), content);
}

TEST_F(FSManagerIntegrationTest, DeleteFileActuallyDeletes) {
    std::string fileId = "todelete";
    std::vector<uint8_t> data = {42};

    ASSERT_TRUE(manager->saveFile(fileId, data));
    ASSERT_TRUE(manager->deleteFile(fileId));

    auto reloaded = manager->loadFile(fileId);
    ASSERT_FALSE(reloaded.has_value());
}
