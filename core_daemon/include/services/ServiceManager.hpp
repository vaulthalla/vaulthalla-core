#pragma once

#include "auth/AuthManager.hpp"
#include "index/SearchIndex.hpp"
#include "share/LinkResolver.hpp"
#include "storage/StorageManager.hpp"
#include <memory>

namespace vh::services {
class ServiceManager {
  public:
    ServiceManager();

    [[nodiscard]] std::shared_ptr<auth::AuthManager> authManager() const;

    [[nodiscard]] std::shared_ptr<index::SearchIndex> searchIndex() const;

    [[nodiscard]] std::shared_ptr<storage::StorageManager> storageManager() const;

    [[nodiscard]] std::shared_ptr<share::LinkResolver> linkResolver() const;

  private:
    std::shared_ptr<storage::StorageManager> storageManager_;
    std::shared_ptr<auth::AuthManager> authManager_;
    std::shared_ptr<index::SearchIndex> searchIndex_;
    std::shared_ptr<share::LinkResolver> linkResolver_;
};
} // namespace vh::services