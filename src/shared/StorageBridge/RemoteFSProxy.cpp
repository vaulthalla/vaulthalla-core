#include "shared/StorageBridge/RemoteFSProxy.hpp"
#include "shared/StorageBridge/UnifiedStorage.hpp"
#include "types/FileMetadata.hpp"

#include <stdexcept>
#include <cstring>

using namespace vh::shared::bridge;

RemoteFSProxy::RemoteFSProxy(std::shared_ptr<UnifiedStorage> backend)
        : backend_(std::move(backend)) {}

bool RemoteFSProxy::fileExists(const std::string& path) const {
    return backend_->exists(path);
}

vh::types::FileMetadata RemoteFSProxy::stat(const std::string& path) const {
    return backend_->getMetadata(path);
}

std::vector<vh::types::FileMetadata> RemoteFSProxy::listDirectory(const std::string& path) const {
    return backend_->listDirectory(path);
}

ssize_t RemoteFSProxy::readFile(const std::string& path, char* buf, size_t size, off_t offset) {
    auto data = backend_->readFile(path, offset, size);
    std::memcpy(buf, data.data(), data.size());
    return static_cast<ssize_t>(data.size());
}

ssize_t RemoteFSProxy::writeFile(const std::string& path, const char* buf, size_t size, off_t offset) {
    return backend_->writeFile(path, buf, size, offset);
}

bool RemoteFSProxy::mkdir(const std::string& path) {
    return backend_->makeDirectory(path);
}

bool RemoteFSProxy::unlink(const std::string& path) {
    return backend_->removeFile(path);
}

bool RemoteFSProxy::rename(const std::string& oldPath, const std::string& newPath) {
    return backend_->moveFile(oldPath, newPath);
}

