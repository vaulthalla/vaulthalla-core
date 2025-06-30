#include "websocket/handlers/UploadHandler.hpp"
#include "websocket/WebSocketSession.hpp"
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
