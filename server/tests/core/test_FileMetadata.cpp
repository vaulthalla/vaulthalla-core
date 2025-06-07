#include <gtest/gtest.h>
#include <core/FileMetadata.hpp>
#include <nlohmann/json.hpp>
#include <thread>

using namespace vh::core;
using namespace std::chrono;

TEST(FileMetadataTest, ConstructorSetsFieldsCorrectly) {
    std::string id = "testfile";
    std::string testPath = "/tmp/testfile.txt";
    uintmax_t size = 1337;

    FileMetadata meta(id, testPath, size);

    EXPECT_EQ(meta.id, id);
    EXPECT_EQ(meta.path, testPath);
    EXPECT_EQ(meta.size_bytes, size);
    EXPECT_LE(std::abs(duration_cast<seconds>(system_clock::now() - meta.modified_at).count()), 2);
}

TEST(FileMetadataTest, JsonSerializationRoundTrip) {
    FileMetadata original("sample", "/foo/bar", 42);
    original.permissions = std::filesystem::perms::owner_read | std::filesystem::perms::owner_write;
    original.owner = "root";
    original.group = "wheel";

    auto json = original.to_json();
    auto deserialized = FileMetadata::from_json(json);

    EXPECT_EQ(original.id, deserialized.id);
    EXPECT_EQ(original.path, deserialized.path);
    EXPECT_EQ(original.size_bytes, deserialized.size_bytes);
    EXPECT_EQ(original.permissions, deserialized.permissions);
    EXPECT_EQ(original.owner, deserialized.owner);
    EXPECT_EQ(original.group, deserialized.group);
}

TEST(FileMetadataTest, HandlesZeroSizeAndEmptyPath) {
    FileMetadata meta("zero", "", 0);
    EXPECT_EQ(meta.size_bytes, static_cast<uintmax_t>(0));
    EXPECT_TRUE(meta.path.empty());
}
