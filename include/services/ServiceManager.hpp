#pragma once

#include "services/SyncController.hpp"
#include "services/FUSE.hpp"
#include "services/Vaulthalla.hpp"

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <chrono>

namespace vh::services {

class ServiceManager : public std::enable_shared_from_this<ServiceManager> {
public:
    static ServiceManager& instance() {
        static ServiceManager instance; // Meyers singleton
        return instance;
    }

    void startAll() {
        std::lock_guard lock(mutex_);

        tryStart("Vaulthalla", vaulthallaService);
        tryStart("FUSE", fuseService);
        tryStart("SyncController", syncController);
    }

    void stopAll(int signal = SIGTERM) {
        std::lock_guard lock(mutex_);
        stopService("SyncController", syncController, signal);
        stopService("FUSE", fuseService, signal);
        stopService("Vaulthalla", vaulthallaService, signal);
    }

    void restartService(const std::string& name) {
        std::lock_guard lock(mutex_);

        auto svc = services_.at(name);
        if (!svc) return;

        std::cout << "[ServiceManager] Restarting " << name << "...\n";
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
        std::cout << "[ServiceManager] Starting " << name << "...\n";
        try {
            svc->start();
        } catch (const std::exception& e) {
            std::cerr << "[-] " << name << " failed to start: " << e.what() << "\n";
            hardFail();
        }
    }

    void stopService(const std::string& name, const std::shared_ptr<AsyncService>& svc, int signal) {
        if (!svc || !svc->isRunning()) return;

        std::cout << "[ServiceManager] Stopping " << name << "...\n";
        try {
            svc->stop();
        } catch (...) {
            std::cerr << "[-] " << name << " ignored SIGTERM, escalating to SIGKILL...\n";
            std::raise(SIGKILL); // Kill whole process group
        }
    }

    [[noreturn]] void hardFail() {
        std::cerr << "[ServiceManager] Fatal service failure. Sending SIGTERM...\n";
        stopAll(SIGTERM);
        std::cerr << "[ServiceManager] Escalating to kill -9.\n";
        std::_Exit(EXIT_FAILURE);
    }

    // prevent accidental copies
    ServiceManager(const ServiceManager&) = delete;
    ServiceManager& operator=(const ServiceManager&) = delete;
};

}
