#include "share/ShareLink.hpp"

#include <utility>

namespace vh::share {

    ShareLink::ShareLink(std::string ownerUsername,
                         std::string mountName,
                         std::string path,
                         std::string permissionType,
                         std::optional<std::chrono::system_clock::time_point> expiresAt)
            : ownerUsername_(std::move(ownerUsername)),
              mountName_(std::move(mountName)),
              path_(std::move(path)),
              permissionType_(std::move(permissionType)),
              expiresAt_(expiresAt) {}

    ShareLink::ShareLink(std::string linkId,
                         std::string ownerUsername,
                         std::string mountName,
                         std::string path,
                         std::string permissionType,
                         std::optional<std::chrono::system_clock::time_point> expiresAt)
             : linkId_(std::move(linkId)),
              ownerUsername_(std::move(ownerUsername)),
              mountName_(std::move(mountName)),
              path_(std::move(path)),
              permissionType_(std::move(permissionType)),
              expiresAt_(expiresAt) {};

    void ShareLink::setLinkId(const std::string& linkId) {
        linkId_ = linkId;
    }

    const std::string& ShareLink::getLinkId() const {
        return linkId_;
    }

    const std::string& ShareLink::getOwnerUsername() const {
        return ownerUsername_;
    }

    const std::string& ShareLink::getMountName() const {
        return mountName_;
    }

    const std::string& ShareLink::getPath() const {
        return path_;
    }

    const std::string& ShareLink::getPermissionType() const {
        return permissionType_;
    }

    const std::optional<std::chrono::system_clock::time_point>& ShareLink::getExpiresAt() const {
        return expiresAt_;
    }

    bool ShareLink::isExpired() const {
        if (!expiresAt_) return false;
        return std::chrono::system_clock::now() > *expiresAt_;
    }

    nlohmann::json ShareLink::toJson() const {
        nlohmann::json j;
        j["linkId"] = linkId_;
        j["ownerUsername"] = ownerUsername_;
        j["mountName"] = mountName_;
        j["path"] = path_;
        j["permissionType"] = permissionType_;

        if (expiresAt_) {
            j["expiresAt"] = std::chrono::duration_cast<std::chrono::seconds>(
                    expiresAt_->time_since_epoch()).count();
        } else {
            j["expiresAt"] = nullptr;
        }

        return j;
    }

    ShareLink ShareLink::fromJson(const nlohmann::json& j) {
        std::optional<std::chrono::system_clock::time_point> expiresAt;
        if (!j["expiresAt"].is_null()) {
            auto seconds = j["expiresAt"].get<int64_t>();
            expiresAt = std::chrono::system_clock::time_point{std::chrono::seconds{seconds}};
        }

        return ShareLink(
                j["linkId"].get<std::string>(),
                j["ownerUsername"].get<std::string>(),
                j["mountName"].get<std::string>(),
                j["path"].get<std::string>(),
                j["permissionType"].get<std::string>(),
                expiresAt
        );
    }

} // namespace vh::share
