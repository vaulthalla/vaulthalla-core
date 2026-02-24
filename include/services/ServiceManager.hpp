#pragma once

#include "logging/LogRegistry.hpp"

#include <csignal>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>

namespace vh::shell { class Router; }
namespace vh::concurrency { class AsyncService; }
namespace vh::fuse { class Service; }

namespace vh::services {

class SyncController;
class Vaulthalla;
class CtlServerService;
class ConnectionLifecycleManager;
class LogRotationService;
class DBSweeper;

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

    void startTestServices();

private:
    ServiceManager();

    void tryStart(const std::string& name, const std::shared_ptr<concurrency::AsyncService>& svc);
    static void stopService(const std::string& name, const std::shared_ptr<concurrency::AsyncService>& svc, int signal);

    void startWatchdog();
    void stopWatchdog();
    [[noreturn]] void hardFail();

    std::shared_ptr<SyncController> syncController;
    std::shared_ptr<fuse::Service> fuseService;
    std::shared_ptr<Vaulthalla> vaulthallaService;
    std::shared_ptr<CtlServerService> ctlServerService;
    std::shared_ptr<ConnectionLifecycleManager> connectionLifecycleManager;
    std::shared_ptr<LogRotationService> logRotationService;
    std::shared_ptr<DBSweeper> dbSweeperService;

    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<concurrency::AsyncService>> services_;

    // Watchdog state
    std::thread watchdogThread;
    std::atomic<bool> watchdogRunning{false};
};

}
