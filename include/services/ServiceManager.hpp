#pragma once

#include "services/SyncController.hpp"
#include "services/FUSE.hpp"
#include "services/Vaulthalla.hpp"
#include "logging/LogRegistry.hpp"

#include <csignal>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <chrono>

using namespace vh::logging;

namespace vh::services {

class ServiceManager : public std::enable_shared_from_this<ServiceManager> {
public:
    static ServiceManager& instance() {
        static ServiceManager instance; // Meyers singleton
        return instance;
    }

    void startAll() {
        LogRegistry::vaulthalla()->debug("[ServiceManager] Starting all services...");
        std::lock_guard lock(mutex_);
        tryStart("Vaulthalla", vaulthallaService);
        tryStart("FUSE", fuseService);
        tryStart("SyncController", syncController);
        LogRegistry::vaulthalla()->debug("[ServiceManager] All services started.");
    }

    void stopAll(int signal = SIGTERM) {
        LogRegistry::vaulthalla()->debug("[ServiceManager] Stopping all services...");
        std::lock_guard lock(mutex_);
        stopService("SyncController", syncController, signal);
        stopService("FUSE", fuseService, signal);
        stopService("Vaulthalla", vaulthallaService, signal);
        LogRegistry::vaulthalla()->debug("[ServiceManager] All services stopped.");
    }

    void restartService(const std::string& name) {
        std::lock_guard lock(mutex_);

        auto svc = services_.at(name);
        if (!svc) return;

        LogRegistry::vaulthalla()->debug("[ServiceManager] Restarting service: {}", name);
        stopService(name, svc, SIGTERM);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        tryStart(name, svc);
    }

    [[nodiscard]] bool allRunning() const {
        return vaulthallaService->isRunning() &&
               fuseService->isRunning() &&
               syncController->isRunning();
    }

    std::shared_ptr<SyncController> getSyncController() const { return syncController; }

    // prevent accidental copies
    ServiceManager(const ServiceManager&) = delete;
    ServiceManager& operator=(const ServiceManager&) = delete;

private:
    std::shared_ptr<SyncController> syncController;
    std::shared_ptr<FUSE> fuseService;
    std::shared_ptr<Vaulthalla> vaulthallaService;

    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<AsyncService>> services_;

    ServiceManager()
        : syncController(std::make_shared<SyncController>()),
          fuseService(std::make_shared<FUSE>()),
          vaulthallaService(std::make_shared<Vaulthalla>())
    {
        services_["SyncController"] = syncController;
        services_["FUSE"] = fuseService;
        services_["Vaulthalla"] = vaulthallaService;
    }

    void tryStart(const std::string& name, const std::shared_ptr<AsyncService>& svc) {
        if (!svc) return;
        LogRegistry::vaulthalla()->debug("[ServiceManager] Starting service: {}", name);
        try {
            svc->start();
        } catch (const std::exception& e) {
            LogRegistry::vaulthalla()->error("[ServiceManager] Failed to start {}: {}", name, e.what());
            hardFail();
        }
    }

    void stopService(const std::string& name, const std::shared_ptr<AsyncService>& svc, int signal) {
        if (!svc || !svc->isRunning()) return;

        LogRegistry::vaulthalla()->debug("[ServiceManager] Stopping service: {}", name);
        try {
            svc->stop();
        } catch (...) {
            LogRegistry::vaulthalla()->error("[ServiceManager] Failed to stop {} gracefully, sending signal {}", name, signal);
            std::raise(SIGKILL); // Kill whole process group
        }
    }

    [[noreturn]] void hardFail() {
        LogRegistry::vaulthalla()->error("[ServiceManager] Critical failure, cannot continue.");
        stopAll(SIGTERM);
        LogRegistry::vaulthalla()->error("[ServiceManager] Exiting with failure status.");
        std::_Exit(EXIT_FAILURE);
    }
};

}
