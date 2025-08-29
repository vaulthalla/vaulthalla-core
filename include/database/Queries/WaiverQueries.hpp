#pragma once

#include <memory>
#include <vector>

namespace vh::types {
    struct Waiver;
}

namespace vh::database {

struct WaiverQueries {
    static void addWaiver(const std::shared_ptr<types::Waiver>& waiver);

    static std::vector<std::shared_ptr<types::Waiver>> getAllWaivers();

    static std::vector<std::shared_ptr<types::Waiver>> getWaiversByUser(unsigned int userId);

    static std::vector<std::shared_ptr<types::Waiver>> getWaiversByVault(unsigned int vaultId);
};

}
