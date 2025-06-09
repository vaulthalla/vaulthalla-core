#pragma once

#include <string>

namespace vh::database {
    class ShareQueries {
    public:
        ShareQueries() = default;

        void addShared(const std::string& shareId, const std::string& mountName, const std::string& path,
                         const std::string& owner, const std::string& permissions);
        void deleteShare(const std::string& shareId);
        bool shareExists(const std::string& shareId) const;
        std::string getShareDetails(const std::string& shareId) const;
    };
}
