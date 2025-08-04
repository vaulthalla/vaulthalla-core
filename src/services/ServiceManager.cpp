#include "services/ServiceManager.hpp"

using namespace vh::logging;
using namespace vh::services;

ServiceManager& ServiceManager::instance() {
    static ServiceManager instance;
    return instance;
}

ServiceManager::ServiceManager()
    : syncController(std::make_shared<SyncController>()),
      fuseService(std::make_shared<FUSE>()),
      vaulthallaService(std::make_shared<Vaulthalla>())
{
    services_["SyncController"] = syncController;
    services_["FUSE"] = fuseService;
    services_["Vaulthalla"] = vaulthallaService;
}

void ServiceManager::startAll() {
    LogRegistry::vaulthalla()->debug("[ServiceManager] Starting all services...");
    std::lock_guard lock(mutex_);
    tryStart("Vaulthalla", vaulthallaService);
    tryStart("FUSE", fuseService);
    tryStart("SyncController", syncController);
    LogRegistry::vaulthalla()->debug("[ServiceManager] All services started.");

    startWatchdog();
}

void ServiceManager::stopAll(int signal) {
    LogRegistry::vaulthalla()->debug("[ServiceManager] Stopping all services...");
    {
        std::lock_guard lock(mutex_);
        stopService("SyncController", syncController, signal);
        stopService("FUSE", fuseService, signal);
        stopService("Vaulthalla", vaulthallaService, signal);
    }

    stopWatchdog();

    LogRegistry::vaulthalla()->debug("[ServiceManager] All services stopped.");
}

void ServiceManager::restartService(const std::string& name) {
    std::lock_guard lock(mutex_);

    auto svc = services_.at(name);
    if (!svc) return;

    LogRegistry::vaulthalla()->warn("[ServiceManager] Restarting service: {}", name);
    stopService(name, svc, SIGTERM);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    tryStart(name, svc);
}

bool ServiceManager::allRunning() const {
    return vaulthallaService->isRunning() &&
           fuseService->isRunning() &&
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
