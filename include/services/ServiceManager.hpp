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
#include <atomic>

namespace vh::services {

class ServiceManager : public std::enable_shared_from_this<ServiceManager> {
public:
    static ServiceManager& instance();

    void startAll();
    void stopAll(int signal = SIGTERM);
    void restartService(const std::string& name);

    [[nodiscard]] bool allRunning() const;

    std::shared_ptr<SyncController> getSyncController() const { return syncController; }

    // prevent accidental copies
    ServiceManager(const ServiceManager&) = delete;
    ServiceManager& operator=(const ServiceManager&) = delete;

private:
    ServiceManager();

    void tryStart(const std::string& name, const std::shared_ptr<AsyncService>& svc);
    void stopService(const std::string& name, const std::shared_ptr<AsyncService>& svc, int signal);

    void startWatchdog();
    void stopWatchdog();
    [[noreturn]] void hardFail();

    std::shared_ptr<SyncController> syncController;
    std::shared_ptr<FUSE> fuseService;
    std::shared_ptr<Vaulthalla> vaulthallaService;

    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<AsyncService>> services_;

    // Watchdog state
    std::thread watchdogThread;
    std::atomic<bool> watchdogRunning{false};
};

}
