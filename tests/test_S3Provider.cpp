#include "types/APIKey.hpp"
#include "storage/cloud/S3Provider.hpp"
#include "util/imageUtil.hpp"
#include "types/FSEntry.hpp"
#include "util/u8.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <algorithm>

#include "storage/CloudStorageEngine.hpp"

namespace fs = std::filesystem;

class S3ProviderIntegrationTest : public ::testing::Test {
  protected:
    std::shared_ptr<vh::types::api::APIKey> apiKey_;
    std::string bucket_;
    std::shared_ptr<vh::cloud::S3Provider> s3Provider_;
    std::filesystem::path test_dir;

    void SetUp() override {
        test_dir = fs::temp_directory_path() / "vaulthalla_test_dir";
        fs::create_directory(test_dir);

        apiKey_ = std::make_shared<vh::types::api::APIKey>(1,
                                                             "Test S3 Key",
                                                             vh::types::api::S3Provider::CloudflareR2,
                                                             std::getenv("VAULTHALLA_TEST_R2_ACCESS_KEY"),
                                                             std::getenv("VAULTHALLA_TEST_R2_SECRET_ACCESS_KEY"),
                                                             std::getenv("VAULTHALLA_TEST_R2_REGION"),
                                                             std::getenv("VAULTHALLA_TEST_R2_ENDPOINT"));

        bucket_ = std::getenv("VAULTHALLA_TEST_R2_BUCKET");

        s3Provider_ = std::make_shared<vh::cloud::S3Provider>(apiKey_, bucket_);
    }

    void TearDown() override { fs::remove_all(test_dir); }

    static void writeTextFile(const fs::path& path, const std::string& content) {
        std::ofstream out(path);
        out << content;
    }
};

TEST_F(S3ProviderIntegrationTest, test_DeleteUnicodeFilename) {
    const std::filesystem::path key = u8"Screenshot 2025-06-26 at 3.29.35\u202FPM.png";
    ASSERT_TRUE(fs::exists(key));

    std::cout << "Uploading file: " << key << std::endl;
    ASSERT_TRUE(s3Provider_->uploadObject(key, key));

    std::cout << "Downloading file: " << key << std::endl;
    const auto downloadedPath = test_dir / "downloaded.png";
    ASSERT_TRUE(s3Provider_->downloadObject(key, downloadedPath));

    std::cout << "Deleting file: " << key << std::endl;
    EXPECT_TRUE(s3Provider_->deleteObject(key));
}

TEST_F(S3ProviderIntegrationTest, test_BulkUploadDownloadDeleteTestAssets) {
    const std::vector<std::filesystem::path> filenames = {
        "sample.jpg",
        "sample.pdf",
        u8"Screenshot 2025-06-26 at 3.29.35\u202FPM.png"
    };

    std::vector<std::filesystem::path> uploadedKeys;

    for (const auto& name : filenames) {
        const fs::path src = fs::path(name);
        ASSERT_TRUE(fs::exists(src)) << "Test asset missing: " << src;

        const fs::path relKey = fs::path("test-assets") / src.filename();
        const fs::path dest = test_dir / src.filename();
        fs::copy_file(src, dest, fs::copy_options::overwrite_existing);

        ASSERT_TRUE(fs::exists(dest)) << "Failed to copy file to: " << dest;

        std::cout << "Uploading file: " << relKey << std::endl;
        ASSERT_TRUE(s3Provider_->uploadObject(relKey, dest)) << "Upload failed for: " << relKey;

        uploadedKeys.push_back(relKey);

        std::vector<uint8_t> buffer;
        std::cout << "Downloading file to buffer: " << relKey << std::endl;
        EXPECT_TRUE(s3Provider_->downloadToBuffer(relKey, buffer)) << "Download failed for: " << relKey;
        EXPECT_GT(buffer.size(), 10) << "Buffer too small for: " << relKey;
    }

    for (const auto& key : uploadedKeys) {
        std::cout << "Deleting uploaded key: " << key << std::endl;
        EXPECT_TRUE(s3Provider_->deleteObject(key)) << "Failed to delete key: " << key;
    }
}

TEST_F(S3ProviderIntegrationTest, test_S3SimpleUploadRoundTrip) {
    const std::filesystem::path key = {"simple-test.txt"};
    const auto filePath = test_dir / key;

    // Write some test content to the file
    writeTextFile(filePath, "This is a test file for S3 upload.");

    ASSERT_TRUE(fs::exists(filePath)) << "File not created at: " << filePath;

    std::cout << "Uploading file: " << filePath << std::endl;
    // Upload the file
    bool uploadSuccess = s3Provider_->uploadObject(key, filePath);
    EXPECT_TRUE(uploadSuccess) << "Failed to upload file to S3: " << key;

    std::cout << "Downloading file: " << filePath << std::endl;
    // Download the file for verification
    const auto downloadedPath = test_dir / "downloaded.txt";
    bool downloadSuccess = s3Provider_->downloadObject(key, downloadedPath);
    EXPECT_TRUE(downloadSuccess);

    // Compare original and downloaded files
    std::ifstream original(filePath);
    std::ifstream downloaded(downloadedPath);
    std::ostringstream originalContent, downloadedContent;
    originalContent << original.rdbuf();
    downloadedContent << downloaded.rdbuf();
    EXPECT_TRUE(originalContent.str() == downloadedContent.str());

    // Cleanup
    EXPECT_TRUE(s3Provider_->deleteObject(key));
}

TEST_F(S3ProviderIntegrationTest, test_S3MultipartUploadRoundtrip) {
    const std::filesystem::path key = {"multipart-test-2.txt"};

    // Generate a temporary file with ~15MB of data
    const auto filePath = test_dir / key;
    const std::string part = std::string(5 * 1024 * 1024, 'x'); // 5MB
    std::ofstream out(filePath, std::ios::binary);
    out << part << part << part; // total 15MB
    out.close();

    ASSERT_TRUE(fs::exists(filePath));

    // Upload the file using multipart logic
    bool uploadSuccess = s3Provider_->uploadLargeObject(key, filePath.string(), 5 * 1024 * 1024);
    EXPECT_TRUE(uploadSuccess);

    // Download for verification
    const auto downloadedPath = test_dir / "downloaded.txt";
    bool downloadSuccess = s3Provider_->downloadObject(key, downloadedPath.string());
    EXPECT_TRUE(downloadSuccess);

    // Compare original and downloaded files
    std::ifstream original(filePath, std::ios::binary);
    std::ifstream downloaded(downloadedPath, std::ios::binary);
    std::ostringstream originalContent, downloadedContent;
    originalContent << original.rdbuf();
    downloadedContent << downloaded.rdbuf();
    EXPECT_TRUE(downloadedContent.str().contains("xxxxx")) << downloadedContent.str();
    // EXPECT_EQ(originalContent.str(), downloadedContent.str());

    // Cleanup
    EXPECT_TRUE(s3Provider_->deleteObject(key));
}

TEST_F(S3ProviderIntegrationTest, test_S3MultipartAbortOnFailure) {
    const std::filesystem::path key = {"abort-test.txt"};

    std::string uploadId = s3Provider_->initiateMultipartUpload(key);
    ASSERT_FALSE(uploadId.empty());

    std::string bogus = std::string(5 * 1024 * 1024, 'Z');
    std::string etag;

    // Simulate partial upload then abort
    bool part1 = s3Provider_->uploadPart(key, uploadId, 1, bogus, etag);
    EXPECT_TRUE(part1);

    bool abortSuccess = s3Provider_->abortMultipartUpload(key, uploadId);
    EXPECT_TRUE(abortSuccess);
}

TEST_F(S3ProviderIntegrationTest, test_S3ListObjectsAndDownloadToBuffer) {
    const std::filesystem::path key = {"list-download-test.txt"};
    const auto filePath = test_dir / key;
    writeTextFile(filePath, "This file should appear in listObjects and download into buffer.");
    ASSERT_TRUE(s3Provider_->uploadObject(key, filePath.string()));

    const auto xml = s3Provider_->listObjects();

    const auto entries = vh::types::fromS3XML(xml);
    EXPECT_FALSE(entries.empty()) << "fromS3XML should return at least one entry";
    auto match = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
        return !entry->isDirectory() && entry->path.filename() == key;
    });
    EXPECT_TRUE(match != entries.end()) << "Uploaded key not found in fromS3XML()";

    std::vector<uint8_t> buffer;
    EXPECT_TRUE(s3Provider_->downloadToBuffer(key, buffer));

    const std::string expected = "appear in listObjects";
    const auto it = std::search(buffer.begin(), buffer.end(),
                                expected.begin(), expected.end());

    EXPECT_TRUE(it != buffer.end());

    EXPECT_TRUE(s3Provider_->deleteObject(key));
}

TEST_F(S3ProviderIntegrationTest, test_ResizeAndCompressImageBuffer) {
    const std::filesystem::path key = {"test-image.jpg"};
    const auto srcPath = fs::path("sample.jpg");
    ASSERT_TRUE(fs::exists(srcPath));
    ASSERT_TRUE(s3Provider_->uploadObject(key, srcPath.string()));

    std::vector<uint8_t> buffer;
    ASSERT_TRUE(s3Provider_->downloadToBuffer(key, buffer));

    auto jpeg = vh::util::resize_and_compress_image_buffer(
        buffer.data(),
        buffer.size(),
        std::nullopt,
        std::make_optional("128")
    );

    EXPECT_GT(jpeg.size(), 100); // sanity check: JPEG data exists
    EXPECT_TRUE(s3Provider_->deleteObject(key));
}

TEST_F(S3ProviderIntegrationTest, test_ResizeAndCompressPdfBuffer) {
    const std::filesystem::path key = {"test-pdf.pdf"};
    const auto srcPath = fs::path("sample.pdf");
    ASSERT_TRUE(fs::exists(srcPath));
    ASSERT_TRUE(s3Provider_->uploadObject(key, srcPath.string()));

    std::vector<uint8_t> buffer;
    ASSERT_TRUE(s3Provider_->downloadToBuffer(key, buffer));

    auto jpeg = vh::util::resize_and_compress_pdf_buffer(
        buffer.data(),
        buffer.size(),
        std::nullopt,
        std::make_optional("128")
    );

    EXPECT_GT(jpeg.size(), 100);
    EXPECT_TRUE(s3Provider_->deleteObject(key));
}
