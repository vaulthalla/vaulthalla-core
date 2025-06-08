#pragma once

#include "share/ShareLink.hpp"

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <optional>

namespace vh::share {

    class ShareLink;

    class LinkResolver {
    public:
        [[nodiscard]]std::string createShareLink(ShareLink& link);
        void removeShareLink(const std::string& linkId);

        std::optional<ShareLink> resolveLink(const std::string& linkId);

        std::vector<ShareLink> listAllShares();

        static std::string generateRandomLinkId();

    private:
        std::unordered_map<std::string, ShareLink> links_{};
        std::mutex linksMutex_{};
    };

} // namespace vh::share
