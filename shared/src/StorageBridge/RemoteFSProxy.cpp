#include "StorageBridge/RemoteFSProxy.hpp"
#include "types/db/File.hpp"
#include "StorageBridge/UnifiedStorage.hpp"
#include <cstring>
#include <stdexcept>

using namespace vh::shared::bridge;

RemoteFSProxy::RemoteFSProxy(std::shared_ptr<UnifiedStorage> backend)
    : backend_(std::move(backend)) {
    if (!backend_) throw std::invalid_argument("Backend cannot be null");
}

bool RemoteFSProxy::fileExists(const std::string& path) const {
    return backend_->exists(path);
}

vh::types::File RemoteFSProxy::stat(const std::string& path) const {
    return backend_->getMetadata(path);
}

std::vector<vh::types::File> RemoteFSProxy::listDir(const std::string& path) const {
    return backend_->listDirectory(path);
}

ssize_t RemoteFSProxy::readFile(const std::string& path, char* buf, const size_t size, const off_t offset) {
    auto data = backend_->readFile(path, offset, size);
    std::memcpy(buf, data.data(), data.size());
    return static_cast<ssize_t>(data.size());
}

ssize_t RemoteFSProxy::writeFile(const std::string& path, const char* buf, const size_t size, const off_t offset) {
    return backend_->writeFile(path, buf, size, offset);
}

bool RemoteFSProxy::createFile(const std::string& path, const mode_t mode) {
    return backend_->createFile(path, mode);
}

bool RemoteFSProxy::resizeFile(const std::string& path, const size_t newSize) {
    return backend_->resizeFile(path, newSize);
}

bool RemoteFSProxy::deleteFile(const std::string& path) {
    return backend_->removeFile(path);
}

bool RemoteFSProxy::mkdir(const std::string& path, const mode_t mode) {
    return backend_->makeDirectory(path, mode);
}

bool RemoteFSProxy::deleteDirectory(const std::string& path) {
    return backend_->removeDirectory(path);
}

bool RemoteFSProxy::rename(const std::string& oldPath, const std::string& newPath) {
    return backend_->moveFile(oldPath, newPath);
}

bool RemoteFSProxy::updateTimestamps(const std::string& path, const time_t atime, const time_t mtime) {
    return backend_->updateTimestamps(path, atime, mtime);
}

bool RemoteFSProxy::setPermissions(const std::string& path, const mode_t mode) {
    return backend_->chmod(path, mode);
}

bool RemoteFSProxy::setOwnership(const std::string& path, const uid_t uid, const gid_t gid) {
    return backend_->chown(path, uid, gid);
}

size_t RemoteFSProxy::getTotalBlocks() const {
    return backend_->getTotalBlocks();
}

size_t RemoteFSProxy::getFreeBlocks() const {
    return backend_->getFreeBlocks();
}

bool RemoteFSProxy::sync(const std::string& path) {
    return backend_->sync(path);
}

bool RemoteFSProxy::flush(const std::string& path) {
    return backend_->flush(path);
}
