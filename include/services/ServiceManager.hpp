#pragma once

#include "auth/AuthManager.hpp"
#include "core/FSManager.hpp"
#include "index/SearchIndex.hpp"
#include "storage/StorageManager.hpp"
#include "security/AccessControl.hpp"
#include "share/LinkResolver.hpp"


#include <memory>
#include <filesystem>

namespace vh::services {
    class ServiceManager {
    public:
        ServiceManager();

        [[nodiscard]] std::shared_ptr<vh::auth::AuthManager> authManager() const;
        [[nodiscard]] std::shared_ptr<vh::core::FSManager> fsManager() const;
        [[nodiscard]] std::shared_ptr<vh::index::SearchIndex> searchIndex() const;
        [[nodiscard]] std::shared_ptr<vh::storage::StorageManager> storageManager() const;
        [[nodiscard]] std::shared_ptr<vh::security::AccessControl> accessControl() const;
        [[nodiscard]] std::shared_ptr<vh::share::LinkResolver> linkResolver() const;

    private:
        std::shared_ptr<vh::auth::AuthManager> authManager_;
        std::shared_ptr<vh::core::FSManager> fsManager_;
        std::shared_ptr<vh::index::SearchIndex> searchIndex_;
        std::shared_ptr<vh::storage::StorageManager> storageManager_;
        std::shared_ptr<vh::security::AccessControl> accessControl_;
        std::shared_ptr<vh::share::LinkResolver> linkResolver_;
    };
}
