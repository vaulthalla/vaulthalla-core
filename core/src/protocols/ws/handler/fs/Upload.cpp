#include "protocols/ws/handler/fs/Upload.hpp"
#include "protocols/ws/Session.hpp"
#include "fs/model/File.hpp"
#include "identities/User.hpp"
#include "log/Registry.hpp"
#include "fs/Filesystem.hpp"

#include <filesystem>
#include <stdexcept>

using namespace vh::storage;
using namespace vh::identities;
using namespace vh::fs;

namespace vh::protocols::ws::handler::fs {

    Upload::Upload(const std::shared_ptr<Session>& session) : session_(session) {}

    void Upload::startUpload(const UploadArgs& args) {
        log::Registry::ws()->debug("[UploadHandler] Starting upload (uploadId: {}, tmpPath: {}, finalPath: {}, "
                                "fuseFrom: {}, fuseTo: {}, expectedSize: {})",
                                 args.uploadId, args.tmpPath.string(), args.finalPath.string(),
                                 args.fuseFrom.string(), args.fuseTo.string(), args.expectedSize);

        if (uploadInProgress()) throw std::runtime_error("Upload already in progress");
        if (!session_ || !session_->user) throw std::runtime_error("Authenticated user upload requires a human session");

        if (std::filesystem::is_directory(args.finalPath))
            throw std::runtime_error("Upload final path is a directory — filename must be provided");

        if (const auto err = Filesystem::mkdir(args.fuseFrom.parent_path(), 0755, session_->user->id, args.engine); err)
            throw std::runtime_error(std::string("Failed to create directory ") + args.fuseFrom.parent_path().string());

        currentUpload_.emplace(args);
    }

    void Upload::startShareUpload(const ShareUploadArgs& args) {
        log::Registry::ws()->debug("[UploadHandler] Starting share upload (uploadId: {}, shareSessionId: {}, expectedSize: {})",
                                  args.uploadId, args.shareSessionId, args.expectedSize);

        if (uploadInProgress()) throw std::runtime_error("Upload already in progress");
        if (args.uploadId.empty()) throw std::invalid_argument("Share upload id is required");
        if (args.shareId.empty()) throw std::invalid_argument("Share upload share id is required");
        if (args.shareSessionId.empty()) throw std::invalid_argument("Share upload session id is required");
        if (!args.onChunk || !args.onFinish || !args.onCancel || !args.onFail)
            throw std::invalid_argument("Share upload callbacks are required");

        currentShareUpload_.emplace(args);
    }

    void Upload::handleBinaryFrame(beast::flat_buffer& buffer) {
        if (currentShareUpload_) {
            auto& upload = *currentShareUpload_;
            const auto size = static_cast<uint64_t>(buffer.size());
            try {
                if (size == 0) throw std::runtime_error("Empty upload chunk");
                if (size > upload.maxChunkSize) throw std::runtime_error("Upload chunk exceeds maximum size");
                if (upload.bytesReceived > upload.expectedSize ||
                    size > upload.expectedSize - upload.bytesReceived)
                    throw std::runtime_error("Upload chunk exceeds expected size");

                const auto data = static_cast<const uint8_t*>(buffer.data().data());
                upload.buffer.insert(upload.buffer.end(), data, data + static_cast<std::ptrdiff_t>(size));
                upload.onChunk(size);
                upload.bytesReceived += size;
                buffer.consume(size);
                return;
            } catch (const std::exception& e) {
                const auto onFail = upload.onFail;
                currentShareUpload_.reset();
                if (onFail) onFail(e.what());
                throw;
            }
        }

        if (!currentUpload_) throw std::runtime_error("No upload in progress");

        auto& upload = *currentUpload_;
        const auto data = static_cast<const char*>(buffer.data().data());
        const auto size = buffer.size();

        upload.file.write(data, static_cast<long>(size));
        if (!upload.file) throw std::runtime_error("Write error during upload");

        upload.bytesReceived += size;
        buffer.consume(size);
    }

    void Upload::finishUpload() {
        if (!currentUpload_) throw std::runtime_error("No upload in progress");

        auto& upload = *currentUpload_;
        upload.file.close();

        if (upload.bytesReceived != upload.expectedSize) {
            std::filesystem::remove(upload.tmpPath);
            throw std::runtime_error("Upload size mismatch");
        }

        log::Registry::ws()->debug("[UploadHandler] Finishing upload (uploadId: {}, fuseFrom: {}, fuseTo: {}, userId: {})",
                                 upload.uploadId, upload.fuseFrom.string(), upload.fuseTo.string(), session_->user->id);

        if (const auto err = Filesystem::rename(upload.fuseFrom, upload.fuseTo, session_->user->id, upload.engine); err) {
            std::filesystem::remove(upload.tmpPath);
            throw std::runtime_error(std::string("Failed to move uploaded file to final location: ") + std::strerror(err));
        }

        currentUpload_.reset();
    }

    std::shared_ptr<vh::fs::model::File> Upload::finishShareUpload(
        const std::string& uploadId,
        const std::string& shareSessionId
    ) {
        if (!currentShareUpload_) throw std::runtime_error("No share upload in progress");

        auto& upload = *currentShareUpload_;
        if (upload.uploadId != uploadId) throw std::runtime_error("Share upload id mismatch");
        if (upload.shareSessionId != shareSessionId) throw std::runtime_error("Share upload session mismatch");
        if (upload.bytesReceived != upload.expectedSize) throw std::runtime_error("Upload size mismatch");

        auto onFinish = upload.onFinish;
        auto buffer = std::move(upload.buffer);
        currentShareUpload_.reset();
        return onFinish(buffer);
    }

    void Upload::cancelShareUpload(const std::string& uploadId, const std::string& shareSessionId) {
        if (!currentShareUpload_) throw std::runtime_error("No share upload in progress");

        auto& upload = *currentShareUpload_;
        if (upload.uploadId != uploadId) throw std::runtime_error("Share upload id mismatch");
        if (upload.shareSessionId != shareSessionId) throw std::runtime_error("Share upload session mismatch");

        auto onCancel = upload.onCancel;
        currentShareUpload_.reset();
        if (onCancel) onCancel();
    }

    std::optional<std::string> Upload::activeShareUploadId() const {
        if (!currentShareUpload_) return std::nullopt;
        return currentShareUpload_->uploadId;
    }

}
