#pragma once

#include "auth/AuthManager.hpp"
#include "storage/StorageManager.hpp"
#include "services/SyncController.hpp"
#include "services/FUSE.hpp"
#include "services/Vaulthalla.hpp"
#include <memory>

namespace vh::services {

class SyncController;

struct ServiceManager : public std::enable_shared_from_this<ServiceManager> {
    std::shared_ptr<storage::StorageManager> storageManager;
    std::shared_ptr<auth::AuthManager> authManager;
    std::shared_ptr<SyncController> syncController;
    std::shared_ptr<FUSE> fuseService;
    std::shared_ptr<Vaulthalla> vaulthallaService;

    ServiceManager()
    : storageManager(std::make_shared<storage::StorageManager>()),
      authManager(std::make_shared<auth::AuthManager>(storageManager)) {}

    void initServices() {
        syncController = std::make_shared<SyncController>(shared_from_this());
        fuseService = std::make_shared<FUSE>(shared_from_this());
        vaulthallaService = std::make_shared<Vaulthalla>(shared_from_this());
        vaulthallaService->start();
        fuseService->start();
        syncController->start();
    }
};

} // namespace vh::services
