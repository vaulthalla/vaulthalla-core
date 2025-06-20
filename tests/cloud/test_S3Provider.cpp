#include "../../shared/include/types/db/APIKey.hpp"
#include "cloud/S3Provider.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

class S3ProviderIntegrationTest : public ::testing::Test {
  protected:
    std::shared_ptr<vh::types::api::S3APIKey> apiKey_;
    std::string bucket_;
    std::shared_ptr<vh::cloud::s3::S3Provider> s3Provider_;
    std::filesystem::path test_dir;

    void SetUp() override {
        test_dir = fs::temp_directory_path() / "vaulthalla_test_dir";
        fs::create_directory(test_dir);

        apiKey_ = std::make_shared<vh::types::api::S3APIKey>("Test S3 Key",
                                                             1, // user_id
                                                             vh::types::api::S3Provider::CloudflareR2,
                                                             std::getenv("VAULTHALLA_TEST_R2_ACCESS_KEY"),
                                                             std::getenv("VAULTHALLA_TEST_R2_SECRET_ACCESS_KEY"),
                                                             std::getenv("VAULTHALLA_TEST_R2_REGION"),
                                                             std::getenv("VAULTHALLA_TEST_R2_ENDPOINT"));

        bucket_ = std::getenv("VAULTHALLA_TEST_R2_BUCKET");

        s3Provider_ = std::make_shared<vh::cloud::s3::S3Provider>(apiKey_);
    }

    void TearDown() override { fs::remove_all(test_dir); }

    static void writeTextFile(const fs::path& path, const std::string& content) {
        std::ofstream out(path);
        out << content;
    }
};

TEST_F(S3ProviderIntegrationTest, test_S3SimpleUploadRoundTrip) {
    const std::string key = "simple-test.txt";
    const auto filePath = test_dir / key;

    // Write some test content to the file
    writeTextFile(filePath, "This is a test file for S3 upload.");

    ASSERT_TRUE(fs::exists(filePath));

    // Upload the file
    bool uploadSuccess = s3Provider_->uploadObject(bucket_, key, filePath.string());
    EXPECT_TRUE(uploadSuccess);

    // Download the file for verification
    const auto downloadedPath = test_dir / "downloaded.txt";
    bool downloadSuccess = s3Provider_->downloadObject(bucket_, key, downloadedPath.string());
    EXPECT_TRUE(downloadSuccess);

    // Compare original and downloaded files
    std::ifstream original(filePath);
    std::ifstream downloaded(downloadedPath);
    std::ostringstream originalContent, downloadedContent;
    originalContent << original.rdbuf();
    downloadedContent << downloaded.rdbuf();
    EXPECT_EQ(originalContent.str(), downloadedContent.str());

    // Cleanup
    EXPECT_TRUE(s3Provider_->deleteObject(bucket_, key));
}

TEST_F(S3ProviderIntegrationTest, test_S3MultipartUploadRoundtrip) {
    const std::string key = "multipart-test-2.txt";

    // Generate a temporary file with ~15MB of data
    const auto filePath = test_dir / key;
    const std::string part = std::string(5 * 1024 * 1024, 'x'); // 5MB
    std::ofstream out(filePath, std::ios::binary);
    out << part << part << part; // total 15MB
    out.close();

    ASSERT_TRUE(fs::exists(filePath));

    // Upload the file using multipart logic
    bool uploadSuccess = s3Provider_->uploadLargeObject(bucket_, key, filePath.string(), 5 * 1024 * 1024);
    EXPECT_TRUE(uploadSuccess);

    // Download for verification
    const auto downloadedPath = test_dir / "downloaded.txt";
    bool downloadSuccess = s3Provider_->downloadObject(bucket_, key, downloadedPath.string());
    EXPECT_TRUE(downloadSuccess);

    // Compare original and downloaded files
    std::ifstream original(filePath, std::ios::binary);
    std::ifstream downloaded(downloadedPath, std::ios::binary);
    std::ostringstream originalContent, downloadedContent;
    originalContent << original.rdbuf();
    downloadedContent << downloaded.rdbuf();
    EXPECT_TRUE(downloadedContent.str().contains("xxxxx")) << downloadedContent.str();
    EXPECT_EQ(originalContent.str(), downloadedContent.str());

    // Cleanup
    EXPECT_TRUE(s3Provider_->deleteObject(bucket_, key));
}

TEST_F(S3ProviderIntegrationTest, test_S3MultipartAbortOnFailure) {
    const std::string key = "abort-test.txt";

    std::string uploadId = s3Provider_->initiateMultipartUpload(bucket_, key);
    ASSERT_FALSE(uploadId.empty());

    std::string bogus = std::string(5 * 1024 * 1024, 'Z');
    std::string etag;

    // Simulate partial upload then abort
    bool part1 = s3Provider_->uploadPart(bucket_, key, uploadId, 1, bogus, etag);
    EXPECT_TRUE(part1);

    bool abortSuccess = s3Provider_->abortMultipartUpload(bucket_, key, uploadId);
    EXPECT_TRUE(abortSuccess);
}
