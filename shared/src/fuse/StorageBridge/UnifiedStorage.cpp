#include "fuse/StorageBridge/UnifiedStorage.hpp"
#include "types/File.hpp"
#include <cstring>
#include <ctime>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

using namespace vh::shared::bridge;

namespace {

struct FileNode {
    bool is_dir{};
    std::vector<char> data;
    mode_t mode{};
    time_t created_at{};
    time_t updated_at{};
    uid_t uid{};
    gid_t gid{};
};

std::unordered_map<std::string, FileNode> mock_fs;
std::string sanitizePath(const std::string& path) {
    return path == "/" ? "/" : path.back() == '/' ? path.substr(0, path.size() - 1) : path;
}

FileNode makeDirNode(mode_t mode) {
    return FileNode{true, {}, mode | S_IFDIR, std::time(nullptr), std::time(nullptr), getuid(), getgid()};
}

FileNode makeFileNode(mode_t mode) {
    return FileNode{false, {}, mode | S_IFREG, std::time(nullptr), std::time(nullptr), getuid(), getgid()};
}

} // namespace

UnifiedStorage::UnifiedStorage() {
    mock_fs["/"] = makeDirNode(0755);
}

bool UnifiedStorage::exists(const std::string& path) const {
    return mock_fs.contains(sanitizePath(path));
}

vh::types::File UnifiedStorage::getMetadata(const std::string& path) const {
    const auto it = mock_fs.find(sanitizePath(path));
    if (it == mock_fs.end()) throw std::runtime_error("Path not found");

    const FileNode& node = it->second;
    types::File f;
    f.created_at = node.created_at;
    f.updated_at = node.updated_at;
    f.size_bytes = node.data.size();
    return f;
}

std::vector<vh::types::File> UnifiedStorage::listDirectory(const std::string& path) const {
    const std::string base = sanitizePath(path);
    if (!mock_fs.contains(base) || !mock_fs.at(base).is_dir) return {};

    std::vector<types::File> entries;
    for (const auto& [p, node] : mock_fs) {
        if (p == base || p.find(base + "/") != 0) continue;

        auto relative = p.substr(base.size() + 1);
        if (relative.empty() || relative.find('/') != std::string::npos) continue;

        types::File f;
        f.name = relative;
        f.created_at = node.created_at;
        f.updated_at = node.updated_at;
        f.size_bytes = node.data.size();
        entries.push_back(f);
    }

    return entries;
}

std::vector<char> UnifiedStorage::readFile(const std::string& path, const size_t offset, const size_t size) {
    const auto it = mock_fs.find(sanitizePath(path));
    if (it == mock_fs.end() || it->second.is_dir) return {};

    FileNode& node = it->second;
    size_t len = std::min(size, node.data.size() - offset);
    return std::vector<char>(node.data.begin() + offset, node.data.begin() + offset + len);
}

ssize_t UnifiedStorage::writeFile(const std::string& path, const char* buf, const size_t size, const size_t offset) {
    const auto it = mock_fs.find(sanitizePath(path));
    if (it == mock_fs.end() || it->second.is_dir) return -1;

    FileNode& node = it->second;
    if (node.data.size() < offset + size) {
        node.data.resize(offset + size);
    }

    std::memcpy(node.data.data() + offset, buf, size);
    node.updated_at = std::time(nullptr);
    return static_cast<ssize_t>(size);
}

bool UnifiedStorage::createFile(const std::string& path, const mode_t mode) {
    const std::string clean = sanitizePath(path);
    if (mock_fs.contains(clean)) return false;

    mock_fs[clean] = makeFileNode(mode);
    return true;
}

bool UnifiedStorage::resizeFile(const std::string& path, const size_t newSize) {
    const auto it = mock_fs.find(sanitizePath(path));
    if (it == mock_fs.end() || it->second.is_dir) return false;

    it->second.data.resize(newSize);
    it->second.updated_at = std::time(nullptr);
    return true;
}

bool UnifiedStorage::removeFile(const std::string& path) {
    const std::string clean = sanitizePath(path);
    if (!mock_fs.contains(clean) || mock_fs[clean].is_dir) return false;

    mock_fs.erase(clean);
    return true;
}

bool UnifiedStorage::moveFile(const std::string& oldPath, const std::string& newPath) {
    const std::string src = sanitizePath(oldPath);
    const std::string dst = sanitizePath(newPath);
    if (!mock_fs.contains(src)) return false;

    mock_fs[dst] = mock_fs[src];
    mock_fs.erase(src);
    return true;
}

bool UnifiedStorage::makeDirectory(const std::string& path, const mode_t mode) {
    const std::string clean = sanitizePath(path);
    if (mock_fs.contains(clean)) return false;

    mock_fs[clean] = makeDirNode(mode);
    return true;
}

bool UnifiedStorage::removeDirectory(const std::string& path) {
    const std::string clean = sanitizePath(path);
    if (!mock_fs.contains(clean) || !mock_fs.at(clean).is_dir) return false;

    for (const auto& [p, _] : mock_fs) {
        if (p != clean && p.find(clean + "/") == 0) return false; // not empty
    }

    mock_fs.erase(clean);
    return true;
}

bool UnifiedStorage::updateTimestamps(const std::string& path, time_t atime, const time_t mtime) {
    const auto it = mock_fs.find(sanitizePath(path));
    if (it == mock_fs.end()) return false;

    it->second.updated_at = mtime;
    return true;
}

bool UnifiedStorage::chmod(const std::string& path, const mode_t mode) {
    const auto it = mock_fs.find(sanitizePath(path));
    if (it == mock_fs.end()) return false;

    it->second.mode = mode;
    return true;
}

bool UnifiedStorage::chown(const std::string& path, const uid_t uid, const gid_t gid) {
    const auto it = mock_fs.find(sanitizePath(path));
    if (it == mock_fs.end()) return false;

    it->second.uid = uid;
    it->second.gid = gid;
    return true;
}

bool UnifiedStorage::sync(const std::string& path) {
    const auto it = mock_fs.find(sanitizePath(path));
    return it != mock_fs.end();
}

bool UnifiedStorage::flush(const std::string& path) {
    // In this mock implementation, flush does nothing
    return true;
}

size_t UnifiedStorage::getTotalBlocks() const {
    return 1024 * 1024; // 4 GB
}

size_t UnifiedStorage::getFreeBlocks() const {
    return 512 * 1024; // 2 GB
}
