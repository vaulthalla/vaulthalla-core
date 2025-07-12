#include "protocols/websocket/handlers/UploadHandler.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "database/Queries/FileQueries.hpp"
#include "types/User.hpp"
#include "types/Directory.hpp"

#include <filesystem>
#include <iostream>

namespace vh::websocket {

UploadHandler::UploadHandler(WebSocketSession& session) : session_(session) {}

void UploadHandler::startUpload(const std::string& uploadId,
                                const std::filesystem::path& tmpPath,
                                const std::filesystem::path& finalPath,
                                const uint64_t expectedSize) {
    if (currentUpload_) throw std::runtime_error("Upload already in progress");

    if (std::filesystem::is_directory(finalPath))
        throw std::runtime_error("Upload final path is a directory — filename must be provided");

    std::filesystem::create_directories(finalPath.parent_path());

    std::cout << "absPath: " << finalPath << ", tmpPath: " << tmpPath << std::endl;

    std::ofstream file(tmpPath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) throw std::runtime_error("Cannot open temp file");

    currentUpload_ = UploadContext {
        .uploadId = uploadId,
        .tmpPath = tmpPath,
        .finalPath = finalPath,
        .expectedSize = expectedSize,
        .bytesReceived = 0,
        .file = std::move(file)
    };
}

void UploadHandler::ensureDirectoriesInDb(const unsigned int vaultId,
                                          const std::filesystem::path& relPath,
                                          const std::shared_ptr<types::User>& user) {
    std::filesystem::path current;
    std::optional<unsigned int> parentId = database::FileQueries::getRootDirectoryId(vaultId);

    for (const auto& part : relPath.parent_path()) {
        current /= part;

        if (auto dirId = database::FileQueries::getDirectoryIdByPath(vaultId, current)) {
            parentId = dirId;
            continue;  // directory already exists in DB
        }

        types::Directory dir;
        dir.vault_id = vaultId;
        dir.name = part.string();
        dir.created_by = user->id;
        dir.last_modified_by = user->id;
        dir.path = current.string();
        dir.parent_id = parentId;

        dir.stats = std::make_shared<types::DirectoryStats>();
        dir.stats->size_bytes = 0;
        dir.stats->file_count = 0;
        dir.stats->subdirectory_count = 0;

        parentId = database::FileQueries::addDirectory(dir);
    }
}

void UploadHandler::handleBinaryFrame(beast::flat_buffer& buffer) {
    if (!currentUpload_) throw std::runtime_error("No upload in progress");

    auto& upload = *currentUpload_;
    const auto data = static_cast<const char*>(buffer.data().data());
    const auto size = buffer.size();

    upload.file.write(data, size);
    if (!upload.file) throw std::runtime_error("Write error during upload");

    upload.bytesReceived += size;
    buffer.consume(size);
}

void UploadHandler::finishUpload() {
    if (!currentUpload_) throw std::runtime_error("No upload in progress");

    auto& upload = *currentUpload_;
    upload.file.close();

    if (upload.bytesReceived != upload.expectedSize) {
        std::filesystem::remove(upload.tmpPath);
        throw std::runtime_error("Upload size mismatch");
    }

    std::filesystem::rename(upload.tmpPath, upload.finalPath);
    currentUpload_.reset();
}

void UploadHandler::fail(const std::string& command, const std::string& error) const {
    session_.send({
        {"command", command},
        {"status", "error"},
        {"error", error}
    });
    std::cerr << "[UploadHandler] " << error << std::endl;
}

} // namespace vh::websocket
