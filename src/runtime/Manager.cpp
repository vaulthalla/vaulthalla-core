#include "runtime/Manager.hpp"
#include "concurrency/AsyncService.hpp"
#include "sync/Controller.hpp"
#include "fuse/Service.hpp"
#include "protocols/ProtocolService.hpp"
#include "protocols/shell/Server.hpp"
#include "protocols/ws/ConnectionLifecycleManager.hpp"
#include "log/RotationService.hpp"
#include "log/Registry.hpp"
#include "runtime/Deps.hpp"
#include "db/Janitor.hpp"

#include <paths.h>

namespace vh::runtime {

Manager& Manager::instance() {
    static Manager instance;
    return instance;
}

Manager::Manager()
    : syncController(std::make_shared<sync::Controller>()),
      fuseService(std::make_shared<fuse::Service>()),
      protocolService(std::make_shared<protocols::ProtocolService>()),
      connectionLifecycleManager(std::make_shared<protocols::ws::ConnectionLifecycleManager>()),
      logRotationService(std::make_shared<log::RotationService>()),
      dbSweeperService(std::make_shared<db::Janitor>())
{
    services_["SyncController"] = syncController;
    services_["FUSE"] = fuseService;
    services_["ProtocolService"] = protocolService;
    services_["ConnectionLifecycleManager"] = connectionLifecycleManager;
    services_["LogRotationService"] = logRotationService;
    services_["DBJanitor"] = dbSweeperService;

    if (!paths::testMode) {
        shellServer = std::make_shared<protocols::shell::Server>();
        services_["ShellServer"] = shellServer;
    }
}

void Manager::startAll() {
    log::Registry::vaulthalla()->debug("[ServiceManager] Starting all services...");
    std::lock_guard lock(mutex_);
    tryStart("ProtocolService", protocolService);
    tryStart("FUSE", fuseService);
    runtime::Deps::get().setFuseSession(fuseService->session());
    tryStart("SyncController", syncController);
    tryStart("ShellServer", shellServer);
    tryStart("ConnectionLifecycleManager", connectionLifecycleManager);
    tryStart("LogRotationService", logRotationService);
    tryStart("DBJanitor", dbSweeperService);
    log::Registry::vaulthalla()->debug("[ServiceManager] All services started.");

    startWatchdog();
}

void Manager::startTestServices() {
    std::scoped_lock lock(mutex_);
    tryStart("FUSE", fuseService);
    tryStart("ShellServer", shellServer);
}


void Manager::stopAll(const int signal) {
    log::Registry::vaulthalla()->debug("[ServiceManager] Stopping all services...");
    {
        std::lock_guard lock(mutex_);
        stopService("SyncController", syncController, signal);
        stopService("FUSE", fuseService, signal);
        stopService("ProtocolService", protocolService, signal);
        stopService("ShellServer", shellServer, signal);
        stopService("ConnectionLifecycleManager", connectionLifecycleManager, signal);
        stopService("LogRotationService", logRotationService, signal);
        stopService("DBJanitor", dbSweeperService, signal);
    }

    stopWatchdog();

    log::Registry::vaulthalla()->debug("[ServiceManager] All services stopped.");
}

void Manager::restartService(const std::string& name) {
    std::lock_guard lock(mutex_);

    const auto svc = services_.at(name);
    if (!svc) return;

    log::Registry::vaulthalla()->warn("[ServiceManager] Restarting service: {}", name);
    stopService(name, svc, SIGTERM);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    tryStart(name, svc);
}

bool Manager::allRunning() const {
    return protocolService->isRunning() && fuseService->isRunning() &&
        syncController->isRunning() && shellServer->isRunning() && connectionLifecycleManager->isRunning() &&
            logRotationService->isRunning() && dbSweeperService->isRunning();
}

void Manager::tryStart(const std::string& name, const std::shared_ptr<concurrency::AsyncService>& svc) {
    if (!svc) return;
    log::Registry::vaulthalla()->debug("[ServiceManager] Starting service: {}", name);
    try {
        svc->start();
    } catch (const std::exception& e) {
        log::Registry::vaulthalla()->error("[ServiceManager] Failed to start {}: {}", name, e.what());
        hardFail();
    }
}

void Manager::stopService(const std::string& name, const std::shared_ptr<concurrency::AsyncService>& svc, int signal) {
    if (!svc || !svc->isRunning()) return;

    log::Registry::vaulthalla()->debug("[ServiceManager] Stopping service: {}", name);
    try {
        svc->stop();
    } catch (...) {
        log::Registry::vaulthalla()->error("[ServiceManager] Failed to stop {} gracefully, sending signal {}", name, signal);
        std::raise(SIGKILL); // Kill whole process group
    }
}

void Manager::startWatchdog() {
    if (watchdogRunning.exchange(true)) return; // already running
    watchdogThread = std::thread([this]() {
        log::Registry::vaulthalla()->info("[ServiceManager] Watchdog started.");
        while (watchdogRunning) {
            {
                std::lock_guard lock(mutex_);
                for (auto& [name, svc] : services_) {
                    if (svc && !svc->isRunning()) {
                        log::Registry::vaulthalla()->warn("[Watchdog] {} is down, restarting...", name);
                        if (name == "FUSE") system(fmt::format("fusermount3 -u {} > /dev/null 2>&1 || fusermount -u {} > /dev/null 2>&1",
                   paths::getMountPath().string(), paths::getMountPath().string()).c_str());

                        restartService(name);
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        log::Registry::vaulthalla()->info("[ServiceManager] Watchdog stopped.");
    });
}

void Manager::stopWatchdog() {
    if (!watchdogRunning.exchange(false)) return;
    if (watchdogThread.joinable())
        watchdogThread.join();
}

[[noreturn]] void Manager::hardFail() {
    log::Registry::vaulthalla()->error("[ServiceManager] Critical failure, cannot continue.");
    stopAll(SIGTERM);
    log::Registry::vaulthalla()->error("[ServiceManager] Exiting with failure status.");
    std::_Exit(EXIT_FAILURE);
}

}
