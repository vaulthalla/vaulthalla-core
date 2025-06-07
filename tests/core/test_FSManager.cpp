#include <gtest/gtest.h>
#include <core/FSManager.hpp>
#include <core/LocalDiskStorageEngine.hpp>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class FSManagerIntegrationTest : public ::testing::Test {
protected:
    std::shared_ptr<vh::core::StorageEngine> storage;
    std::unique_ptr<vh::core::FSManager> manager;
    fs::path test_dir;

    void SetUp() override {
        test_dir = fs::temp_directory_path() / "vaulthalla_test_dir";
        fs::create_directory(test_dir);
        storage = std::make_shared<vh::core::LocalDiskStorageEngine>(test_dir);
        manager = std::make_unique<vh::core::FSManager>(storage);
    }

    void TearDown() override {
        fs::remove_all(test_dir);
    }

    static void writeTextFile(const fs::path& path, const std::string& content) {
        std::ofstream out(path);
        out << content;
    }
};

// === EXISTING TESTS ===

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

// === ENHANCED TESTS ===

TEST_F(FSManagerIntegrationTest, FileExistsAndMetadata) {
    std::string fileId = "exists";
    std::vector<uint8_t> data = {99, 100, 101};

    ASSERT_TRUE(manager->saveFile(fileId, data));

    EXPECT_TRUE(manager->fileExists(fileId));

    auto metadataOpt = manager->getMetadata(fileId);
    ASSERT_TRUE(metadataOpt.has_value());
    EXPECT_EQ(metadataOpt->id, fileId);
    EXPECT_EQ(metadataOpt->size_bytes, data.size());
}

TEST_F(FSManagerIntegrationTest, RebuildIndexAndSearch) {
    // Create some text files directly on disk
    writeTextFile(test_dir / "alpha.txt", "odin thor loki valhalla");
    writeTextFile(test_dir / "beta.txt", "norse gods and valkyries");
    writeTextFile(test_dir / "gamma.txt", "this is an empty test file");

    manager->rebuildIndex();

    auto results = manager->search("valhalla");
    EXPECT_EQ(results.size(), 1); // Should find only alpha.txt

    auto godsResults = manager->search("gods");
    EXPECT_EQ(godsResults.size(), 1); // Should find beta.txt

    auto missingResults = manager->search("midgard");
    EXPECT_TRUE(missingResults.empty());
}

TEST_F(FSManagerIntegrationTest, ListFilesInDir) {
    writeTextFile(test_dir / "file1.txt", "hello");
    writeTextFile(test_dir / "file2.txt", "world");

    auto files = manager->listFilesInDir(test_dir, false);

    // Expect exactly 2 files found
    EXPECT_EQ(files.size(), 2);
    EXPECT_TRUE(std::find(files.begin(), files.end(), test_dir / "file1.txt") != files.end());
    EXPECT_TRUE(std::find(files.begin(), files.end(), test_dir / "file2.txt") != files.end());
}

TEST_F(FSManagerIntegrationTest, ScanFileAddsToIndex) {
    fs::path filePath = test_dir / "scanme.txt";
    writeTextFile(filePath, "mimir wisdom runes");

    manager->scanFile(filePath);

    auto results = manager->search("mimir");
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0], filePath);
}
