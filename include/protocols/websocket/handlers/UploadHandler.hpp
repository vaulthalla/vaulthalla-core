#pragma once

#include <string>
#include <fstream>
#include <optional>
#include <nlohmann/json.hpp>
#include <boost/beast/core/flat_buffer.hpp>

namespace vh::types {
    struct User;
}

namespace vh::websocket {

class WebSocketSession; // Forward declare to avoid circular dependency

struct UploadContext {
    std::string uploadId;
    std::string tmpPath;
    std::string finalPath;
    uint64_t expectedSize = 0;
    uint64_t bytesReceived = 0;
    std::ofstream file;
};

class UploadHandler {
public:
    UploadHandler(WebSocketSession& session);

    void startUpload(const std::string& uploadId,
                     const std::filesystem::path& tmpPath,
                     const std::filesystem::path& finalPath,
                     uint64_t expectedSize);

    void handleBinaryFrame(boost::beast::flat_buffer& buffer);

    void finishUpload(unsigned int vaultId);

    bool uploadInProgress() const { return currentUpload_.has_value(); }

private:
    WebSocketSession& session_;
    std::optional<UploadContext> currentUpload_;

    void fail(const std::string& command, const std::string& error) const;
};

} // namespace vh::websocket