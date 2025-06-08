#pragma once

#include <string>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>

namespace vh::share {

    class ShareLink {
    public:
        ShareLink(const std::string& ownerUsername,
                  const std::string& mountName,
                  const std::string& path,
                  const std::string& permissionType,
                  std::optional<std::chrono::system_clock::time_point> expiresAt = std::nullopt);

        void setLinkId(const std::string& linkId);

        const std::string& getLinkId() const;
        const std::string& getOwnerUsername() const;
        const std::string& getMountName() const;
        const std::string& getPath() const;
        const std::string& getPermissionType() const;
        const std::optional<std::chrono::system_clock::time_point>& getExpiresAt() const;

        bool isExpired() const;

        // Serialization
        nlohmann::json toJson() const;
        static ShareLink fromJson(const nlohmann::json& j);

    private:
        std::string linkId_;
        std::string ownerUsername_;
        std::string mountName_;
        std::string path_;
        std::string permissionType_;
        std::optional<std::chrono::system_clock::time_point> expiresAt_;
    };

} // namespace vh::share
