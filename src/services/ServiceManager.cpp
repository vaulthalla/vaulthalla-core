#include "services/ServiceManager.hpp"
#include "services/SyncController.hpp"
#include "services/FUSE.hpp"
#include "services/Vaulthalla.hpp"
#include "services/CtlServerService.hpp"
#include "services/ConnectionLifecycleManager.hpp"
#include "services/LogRotationService.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "services/DBSweeper.hpp"

#include <paths.h>

using namespace vh::logging;
using namespace vh::services;

ServiceManager& ServiceManager::instance() {
    static ServiceManager instance;
    return instance;
}

ServiceManager::ServiceManager()
    : syncController(std::make_shared<SyncController>()),
      fuseService(std::make_shared<FUSE>()),
      vaulthallaService(std::make_shared<Vaulthalla>()),
      connectionLifecycleManager(std::make_shared<ConnectionLifecycleManager>()),
      logRotationService(std::make_shared<LogRotationService>()),
      dbSweeperService(std::make_shared<DBSweeper>())
{
    services_["SyncController"] = syncController;
    services_["FUSE"] = fuseService;
    services_["Vaulthalla"] = vaulthallaService;
    services_["ConnectionLifecycleManager"] = connectionLifecycleManager;
    services_["LogRotationService"] = logRotationService;
    services_["DBSweeper"] = dbSweeperService;

    if (!paths::testMode) {
        ctlServerService = std::make_shared<CtlServerService>();
        services_["CtlServer"] = ctlServerService;
    }
}

void ServiceManager::startAll() {
    LogRegistry::vaulthalla()->debug("[ServiceManager] Starting all services...");
    std::lock_guard lock(mutex_);
    tryStart("Vaulthalla", vaulthallaService);
    tryStart("FUSE", fuseService);
    ServiceDepsRegistry::instance().setFuseSession(fuseService->session());
    tryStart("SyncController", syncController);
    tryStart("CtlServer", ctlServerService);
    tryStart("ConnectionLifecycleManager", connectionLifecycleManager);
    tryStart("LogRotationService", logRotationService);
    tryStart("DBSweeper", dbSweeperService);
    LogRegistry::vaulthalla()->debug("[ServiceManager] All services started.");

    startWatchdog();
}

void ServiceManager::startTestServices() {
    std::scoped_lock lock(mutex_);
    tryStart("FUSE", fuseService);
    tryStart("CtlServer", ctlServerService);
}


void ServiceManager::stopAll(const int signal) {
    LogRegistry::vaulthalla()->debug("[ServiceManager] Stopping all services...");
    {
        std::lock_guard lock(mutex_);
        stopService("SyncController", syncController, signal);
        stopService("FUSE", fuseService, signal);
        stopService("Vaulthalla", vaulthallaService, signal);
        stopService("CtlServer", ctlServerService, signal);
        stopService("ConnectionLifecycleManager", connectionLifecycleManager, signal);
        stopService("LogRotationService", logRotationService, signal);
        stopService("DBSweeper", dbSweeperService, signal);
    }

    stopWatchdog();

    LogRegistry::vaulthalla()->debug("[ServiceManager] All services stopped.");
}

void ServiceManager::restartService(const std::string& name) {
    std::lock_guard lock(mutex_);

    const auto svc = services_.at(name);
    if (!svc) return;

    LogRegistry::vaulthalla()->warn("[ServiceManager] Restarting service: {}", name);
    stopService(name, svc, SIGTERM);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    tryStart(name, svc);
}

bool ServiceManager::allRunning() const {
    return vaulthallaService->isRunning() && fuseService->isRunning() &&
        syncController->isRunning();
}

void ServiceManager::tryStart(const std::string& name, const std::shared_ptr<AsyncService>& svc) {
    if (!svc) return;
    LogRegistry::vaulthalla()->debug("[ServiceManager] Starting service: {}", name);
    try {
        svc->start();
    } catch (const std::exception& e) {
        LogRegistry::vaulthalla()->error("[ServiceManager] Failed to start {}: {}", name, e.what());
        hardFail();
    }
}

void ServiceManager::stopService(const std::string& name, const std::shared_ptr<AsyncService>& svc, int signal) {
    if (!svc || !svc->isRunning()) return;

    LogRegistry::vaulthalla()->debug("[ServiceManager] Stopping service: {}", name);
    try {
        svc->stop();
    } catch (...) {
        LogRegistry::vaulthalla()->error("[ServiceManager] Failed to stop {} gracefully, sending signal {}", name, signal);
        std::raise(SIGKILL); // Kill whole process group
    }
}

void ServiceManager::startWatchdog() {
    if (watchdogRunning.exchange(true)) return; // already running
    watchdogThread = std::thread([this]() {
        LogRegistry::vaulthalla()->info("[ServiceManager] Watchdog started.");
        while (watchdogRunning) {
            {
                std::lock_guard lock(mutex_);
                for (auto& [name, svc] : services_) {
                    if (svc && !svc->isRunning()) {
                        LogRegistry::vaulthalla()->warn("[Watchdog] {} is down, restarting...", name);
                        if (name == "FUSE") system(fmt::format("fusermount3 -u {} > /dev/null 2>&1 || fusermount -u {} > /dev/null 2>&1",
                   paths::getMountPath().string(), paths::getMountPath().string()).c_str());

                        restartService(name);
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        LogRegistry::vaulthalla()->info("[ServiceManager] Watchdog stopped.");
    });
}

void ServiceManager::stopWatchdog() {
    if (!watchdogRunning.exchange(false)) return;
    if (watchdogThread.joinable())
        watchdogThread.join();
}

[[noreturn]] void ServiceManager::hardFail() {
    LogRegistry::vaulthalla()->error("[ServiceManager] Critical failure, cannot continue.");
    stopAll(SIGTERM);
    LogRegistry::vaulthalla()->error("[ServiceManager] Exiting with failure status.");
    std::_Exit(EXIT_FAILURE);
}
