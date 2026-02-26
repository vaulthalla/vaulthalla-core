#pragma once

#include <memory>
#include <vector>

namespace vh::sync::model { struct Waiver; }

namespace vh::db::query::vault {

class Waiver {
    using W = vh::sync::model::Waiver;
    using WaiverPtr = std::shared_ptr<W>;

public:
    static void addWaiver(const WaiverPtr& waiver);

    static std::vector<WaiverPtr> getAllWaivers();

    static std::vector<WaiverPtr> getWaiversByUser(unsigned int userId);

    static std::vector<WaiverPtr> getWaiversByVault(unsigned int vaultId);
};

}
