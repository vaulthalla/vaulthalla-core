#pragma once

#include <string>
#include <fstream>
#include <optional>
#include <nlohmann/json.hpp>
#include <boost/beast/core/flat_buffer.hpp>

namespace vh::storage {
class StorageEngine;
}

namespace vh::types {
    struct User;
}

namespace vh::websocket {

class WebSocketSession; // Forward declare to avoid circular dependency

struct UploadArgs {
    std::string uploadId;
    uint64_t expectedSize = 0;
    std::shared_ptr<storage::StorageEngine> engine;
    std::filesystem::path tmpPath, finalPath, fuseFrom, fuseTo;
};

struct UploadContext : UploadArgs {
    uint64_t bytesReceived = 0;
    std::ofstream file;

    explicit UploadContext(const UploadArgs& args)
        : UploadArgs(args),
          file(args.tmpPath, std::ios::binary | std::ios::trunc) {
        if (!file.is_open()) throw std::runtime_error("Cannot open temp file");
    }
};

class UploadHandler {
public:
    UploadHandler(WebSocketSession& session);

    void startUpload(const UploadArgs& args);

    void handleBinaryFrame(boost::beast::flat_buffer& buffer);

    void finishUpload();

    bool uploadInProgress() const { return currentUpload_.has_value(); }

private:
    WebSocketSession& session_;
    std::optional<UploadContext> currentUpload_;

    void fail(const std::string& command, const std::string& error) const;
};

} // namespace vh::websocket