#include "protocols/websocket/handlers/UploadHandler.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "types/User.hpp"
#include "logging/LogRegistry.hpp"
#include "storage/Filesystem.hpp"

#include <filesystem>

using namespace vh::websocket;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;
using namespace vh::logging;

UploadHandler::UploadHandler(WebSocketSession& session) : session_(session) {}

void UploadHandler::startUpload(const UploadArgs& args) {
    LogRegistry::ws()->debug("[UploadHandler] Starting upload (uploadId: {}, tmpPath: {}, finalPath: {}, expectedSize: {})",
                             args.uploadId, args.tmpPath.string(), args.finalPath.string(), args.expectedSize);

    if (currentUpload_) throw std::runtime_error("Upload already in progress");

    if (std::filesystem::is_directory(args.finalPath))
        throw std::runtime_error("Upload final path is a directory — filename must be provided");

    std::optional<unsigned int> userId = std::nullopt;
    if (session_.getAuthenticatedUser()) userId = session_.getAuthenticatedUser()->id;

    Filesystem::mkdir(args.fuseFrom.parent_path(), 0755, userId, args.engine);

    currentUpload_.emplace(args);
}

void UploadHandler::handleBinaryFrame(beast::flat_buffer& buffer) {
    if (!currentUpload_) throw std::runtime_error("No upload in progress");

    auto& upload = *currentUpload_;
    const auto data = static_cast<const char*>(buffer.data().data());
    const auto size = buffer.size();

    upload.file.write(data, static_cast<long>(size));
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

    std::optional<unsigned int> userId = std::nullopt;
    if (session_.getAuthenticatedUser()) userId = session_.getAuthenticatedUser()->id;

    LogRegistry::ws()->debug("[UploadHandler] Finishing upload (uploadId: {}, fuseFrom: {}, fuseTo: {}, userId: {})",
                             upload.uploadId, upload.fuseFrom.string(), upload.fuseTo.string(), userId.value_or(0));

    Filesystem::rename(upload.fuseFrom, upload.fuseTo, userId, upload.engine);

    currentUpload_.reset();
}

void UploadHandler::fail(const std::string& command, const std::string& error) const {
    session_.send({
        {"command", command},
        {"status", "error"},
        {"error", error}
    });
    LogRegistry::ws()->error("[UploadHandler] {} failed: {}", command, error);
}
