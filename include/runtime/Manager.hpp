#pragma once

#include "log/Registry.hpp"

#include <csignal>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>

namespace vh::protocols {
class ProtocolService;
namespace shell { class Router; class Server; }
namespace ws { class ConnectionLifecycleManager; }
}

namespace vh::concurrency { class AsyncService; }
namespace vh::fuse { class Service; }
namespace vh::sync { class Controller; }
namespace vh::log { class RotationService; }
namespace vh::db { class Janitor; }

namespace vh::runtime {

class Manager : public std::enable_shared_from_this<Manager> {
public:
    static Manager& instance();

    void startAll();
    void stopAll(int signal = SIGTERM);
    void restartService(const std::string& name);

    [[nodiscard]] bool allRunning() const;

    std::shared_ptr<sync::Controller> getSyncController() const { return syncController; }

    // prevent accidental copies
    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;

    void startTestServices();

private:
    Manager();

    void tryStart(const std::string& name, const std::shared_ptr<concurrency::AsyncService>& svc);
    static void stopService(const std::string& name, const std::shared_ptr<concurrency::AsyncService>& svc, int signal);

    void startWatchdog();
    void stopWatchdog();
    [[noreturn]] void hardFail();

    std::shared_ptr<sync::Controller> syncController;
    std::shared_ptr<fuse::Service> fuseService;
    std::shared_ptr<protocols::ProtocolService> protocolService;
    std::shared_ptr<protocols::shell::Server> shellServer;
    std::shared_ptr<protocols::ws::ConnectionLifecycleManager> connectionLifecycleManager;
    std::shared_ptr<log::RotationService> logRotationService;
    std::shared_ptr<db::Janitor> dbSweeperService;

    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<concurrency::AsyncService>> services_;

    // Watchdog state
    std::thread watchdogThread;
    std::atomic<bool> watchdogRunning{false};
};

}
