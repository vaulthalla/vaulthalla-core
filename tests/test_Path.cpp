#include <gtest/gtest.h>
#include "types/Path.hpp"
#include "config/ConfigRegistry.hpp"

#include <filesystem>

namespace fs = std::filesystem;
using namespace vh::types;
using namespace vh::config;

class PathTest : public ::testing::Test {
protected:
    void SetUp() override {
        ConfigRegistry::init({"/etc/vaulthalla/config.yaml"});
    }
};

TEST_F(PathTest, AbsPath_VaultRoot) {
    Path p("users/admin");
    fs::path abs = p.absPath("Invoice.pdf", PathType::VAULT_ROOT);
    EXPECT_EQ(abs, "/mnt/vaulthalla/users/admin/Invoice.pdf");
}

TEST_F(PathTest, RelPath_VaultRoot) {
    Path p("users/admin");
    fs::path rel = p.relPath("/mnt/vaulthalla/users/admin/Invoice.pdf", PathType::VAULT_ROOT);
    EXPECT_EQ(rel, "Invoice.pdf");
}

TEST_F(PathTest, AbsRelToRoot_StripsMountPrefix) {
    Path p("users/admin");
    fs::path out = p.absRelToRoot("/mnt/vaulthalla/users/admin/Invoice.pdf", PathType::VAULT_ROOT);
    EXPECT_EQ(out, "/Invoice.pdf");  // normalized under vault root
}

TEST_F(PathTest, AbsRelToRoot_NestedPath) {
    Path p("users/admin");
    fs::path out = p.absRelToRoot("/mnt/vaulthalla/users/admin/projects/test.txt", PathType::VAULT_ROOT);
    EXPECT_EQ(out, "/projects/test.txt");
}

TEST_F(PathTest, AbsRelToRoot_NotUnderRoot) {
    Path p("users/admin");
    fs::path out = p.absRelToRoot("/etc/passwd", PathType::VAULT_ROOT);
    // Should just fall back to filename
    EXPECT_EQ(out.filename(), "passwd");
}

TEST_F(PathTest, CacheRootPath) {
    Path p("users/admin");
    fs::path abs = p.absPath("file.tmp", PathType::CACHE_ROOT);
    EXPECT_EQ(abs.string(), "/var/lib/vaulthalla/.cache/users/admin/file.tmp");
}

TEST_F(PathTest, RelPath_VaultRoot_Subdir) {
    Path p("users/admin");
    fs::path rel = p.relPath("/mnt/vaulthalla/users/admin/docs/report.txt", PathType::VAULT_ROOT);
    EXPECT_EQ(rel, "docs/report.txt");
}

TEST_F(PathTest, RelPath_FuseRoot) {
    Path p("users/admin");
    fs::path rel = p.relPath("/mnt/vaulthalla/users/admin/docs/report.txt", PathType::FUSE_ROOT);
    EXPECT_EQ(rel, "users/admin/docs/report.txt");
}

TEST_F(PathTest, AbsRelToRoot_SimpleFile) {
    Path p("users/admin");
    fs::path out = p.absRelToRoot("/mnt/vaulthalla/users/admin/note.txt", PathType::VAULT_ROOT);
    EXPECT_EQ(out, "/note.txt");
}

TEST_F(PathTest, AbsRelToRoot_NormalizesDotDot) {
    Path p("users/admin");
    // Simulate a user path with extra traversal
    fs::path out = p.absRelToRoot("/mnt/vaulthalla/users/admin/../admin/Invoice.pdf", PathType::VAULT_ROOT);
    EXPECT_EQ(out.string(), "/Invoice.pdf");  // should normalize away the ".."
}

TEST_F(PathTest, AbsRelToRoot_DeepSubdir) {
    Path p("users/admin");
    fs::path out = p.absRelToRoot("/mnt/vaulthalla/users/admin/projects/2025/report/final.docx", PathType::VAULT_ROOT);
    EXPECT_EQ(out, "/projects/2025/report/final.docx");
}

TEST_F(PathTest, AbsPath_BackupRoot) {
    Path p("users/admin");
    fs::path abs = p.absPath("shadow.db", PathType::BACKING_VAULT_ROOT);
    EXPECT_EQ(abs, "/var/lib/vaulthalla/users/admin/shadow.db");
}

TEST_F(PathTest, AbsRelToRoot_ResolvePath) {
    Path p("users/admin");
    fs::path abs = p.absRelToRoot("/mnt/vaulthalla/users/admin/test-assets/sample.jpg", PathType::VAULT_ROOT);
    EXPECT_EQ(abs, "/test-assets/sample.jpg");
}

TEST_F(PathTest, AbsRelToAbsOther_ConvertPaths) {
    Path p("users/admin");
    fs::path abs = p.absRelToAbsRel("/users/admin/test.txt", PathType::FUSE_ROOT, PathType::VAULT_ROOT);
    EXPECT_EQ(abs, "/test.txt");
}

TEST_F(PathTest, AbsRelToAbsOther_OutsideRootFallsBack) {
    Path p("users/admin");
    fs::path abs = p.absRelToAbsRel("/etc/passwd", PathType::VAULT_ROOT, PathType::CACHE_ROOT);
    EXPECT_EQ(abs.filename(), "passwd"); // Should collapse to filename
}

TEST_F(PathTest, AbsRelToAbsOther_BackupRootToVault) {
    Path p("users/admin");
    fs::path abs = p.absRelToAbsRel("/var/lib/vaulthalla/users/admin/shadow.db", PathType::BACKING_VAULT_ROOT, PathType::VAULT_ROOT);
    EXPECT_EQ(abs, "/shadow.db");
}

TEST_F(PathTest, AbsRelToRoot_EmptyString) {
    Path p("users/admin");
    fs::path out = p.absRelToRoot("", PathType::VAULT_ROOT);
    EXPECT_TRUE(out == fs::path("/"));  // should return root if empty
}

TEST_F(PathTest, AbsRelToAbsOther_ReduceFuseToVault) {
    Path p("users/admin");
    fs::path out = p.absRelToAbsRel("/users/admin/sample_data/Invoice-102-Cooper-Larson.pdf", PathType::FUSE_ROOT, PathType::VAULT_ROOT);
    EXPECT_EQ(out, "/sample_data/Invoice-102-Cooper-Larson.pdf");  // should treat as absolute under vault root
}
