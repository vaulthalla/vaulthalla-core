#include "share/LinkResolver.hpp"
#include <sodium.h>

namespace vh::share {

    std::string LinkResolver::createShareLink(ShareLink& link) {
        std::lock_guard<std::mutex> lock(linksMutex_);

        std::string linkId = generateRandomLinkId();
        link.setLinkId(linkId);

        links_[linkId] = link;

        return linkId;
    }

    void LinkResolver::removeShareLink(const std::string& linkId) {
        std::lock_guard<std::mutex> lock(linksMutex_);
        links_.erase(linkId);
    }

    std::optional<ShareLink> LinkResolver::resolveLink(const std::string& linkId) {
        std::lock_guard<std::mutex> lock(linksMutex_);
        auto it = links_.find(linkId);
        if (it != links_.end()) {
            if (!it->second.isExpired()) {
                return it->second;
            }
        }
        return std::nullopt;
    }

    std::vector<ShareLink> LinkResolver::listAllShares() {
        std::lock_guard<std::mutex> lock(linksMutex_);
        std::vector<ShareLink> result;
        for (const auto& pair : links_) result.push_back(pair.second);
        return result;
    }

    std::string LinkResolver::generateRandomLinkId() {
        unsigned char buf[8]; // 64-bit random → more than enough
        randombytes_buf(buf, sizeof buf);

        std::ostringstream oss;
        for (unsigned char i : buf) oss << std::hex << std::setw(2) << std::setfill('0') << (int)i;

        return oss.str();
    }


} // namespace vh::share
