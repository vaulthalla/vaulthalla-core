#pragma once

#include "auth/AuthManager.hpp"
#include "index/SearchIndex.hpp"
#include "share/LinkResolver.hpp"
#include "storage/StorageManager.hpp"
#include <filesystem>
#include <memory>

namespace vh::services {
class ServiceManager {
  public:
    ServiceManager();

    [[nodiscard]] std::shared_ptr<vh::auth::AuthManager> authManager() const;

    [[nodiscard]] std::shared_ptr<vh::index::SearchIndex> searchIndex() const;

    [[nodiscard]] std::shared_ptr<vh::storage::StorageManager> storageManager() const;

    [[nodiscard]] std::shared_ptr<vh::share::LinkResolver> linkResolver() const;

  private:
    std::shared_ptr<vh::storage::StorageManager> storageManager_;
    std::shared_ptr<vh::auth::AuthManager> authManager_;
    std::shared_ptr<vh::index::SearchIndex> searchIndex_;
    std::shared_ptr<vh::share::LinkResolver> linkResolver_;
};
} // namespace vh::services