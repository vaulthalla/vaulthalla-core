#pragma once

#define FUSE_USE_VERSION 35

#include <memory>
#include <fuse3/fuse_lowlevel.h>

namespace vh::storage { class Manager; }
namespace vh::fs::cache { class Registry; }
namespace vh::vault { class APIKeyManager; }
namespace vh::auth { class Manager; }
namespace vh::protocols::shell { class UsageManager; }
namespace vh::stats::model { struct CacheStats; }
namespace vh::sync { class Controller; }

namespace vh::runtime {

struct Deps {
    std::shared_ptr<storage::Manager> storageManager;
    std::shared_ptr<vault::APIKeyManager> apiKeyManager;
    std::shared_ptr<auth::Manager> authManager;
    std::shared_ptr<sync::Controller> syncController;
    std::shared_ptr<fs::cache::Registry> fsCache;
    std::shared_ptr<protocols::shell::UsageManager> shellUsageManager;
    fuse_session* fuseSession = nullptr;
    std::shared_ptr<stats::model::CacheStats> httpCacheStats;

    void setFuseSession(fuse_session* fuseSession) { this->fuseSession = fuseSession; }

    Deps(const Deps&) = delete;
    Deps& operator=(const Deps&) = delete;

    static Deps& get();

    static void init();

    static void setSyncController(const std::shared_ptr<sync::Controller>& syncController);

private:
    Deps() = default;  // private ctor
};

}
