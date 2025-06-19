#include "../include/FUSEOperations.hpp"
#include "StorageBridge/RemoteFSProxy.hpp"
#include "StorageBridge/UnifiedStorage.hpp"

int main(int argc, char* argv[]) {
    auto storage = std::make_shared<vh::shared::bridge::UnifiedStorage>(/* vault config */);
    auto* fs = new vh::shared::bridge::RemoteFSProxy(storage);

    vh::fuse::bind(fs);
    struct fuse_operations ops = vh::fuse::getOperations();

    return fuse_main(argc, argv, &ops, nullptr);
}
