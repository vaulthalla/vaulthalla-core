#pragma once

#include "services/SyncController.hpp"
#include "services/FUSE.hpp"
#include "services/Vaulthalla.hpp"
#include <memory>

namespace vh::services {

class SyncController;

struct ServiceManager : public std::enable_shared_from_this<ServiceManager> {
    std::shared_ptr<SyncController> syncController;
    std::shared_ptr<FUSE> fuseService;
    std::shared_ptr<Vaulthalla> vaulthallaService;

    ServiceManager()
        : syncController(std::make_shared<SyncController>()),
          fuseService(std::make_shared<FUSE>()),
          vaulthallaService(std::make_shared<Vaulthalla>()) {}

    void startServices() {
        vaulthallaService->start();
        fuseService->start();
        syncController->start();
    }
};

} // namespace vh::services
