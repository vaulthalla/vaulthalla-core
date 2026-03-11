#pragma once

#include "db/model/ListQueryParams.hpp"

#include <vector>
#include <string>
#include <memory>

namespace vh::rbac::model { struct Override; }

namespace vh::db::query::rbac::permission {

struct VPermOverrideQuery {
    unsigned int vault_id;
    std::string subject_type;
    unsigned int subject_id;
    unsigned int bit_position = 0;
};

class Override {
    using Override = vh::rbac::model::Override;
    using OverridePtr = std::shared_ptr<Override>;

public:
    static unsigned int addVPermOverride(const OverridePtr& override);
    static void updateVPermOverride(const OverridePtr& override);
    static void removeVPermOverride(unsigned int permOverrideId);
    static std::vector<OverridePtr> listVPermOverrides(unsigned int vaultId, model::ListQueryParams&& params = {});
    static std::vector<OverridePtr> listAssignedVRoleOverrides(const VPermOverrideQuery& query, model::ListQueryParams&& params = {});
    static OverridePtr getVPermOverride(const VPermOverrideQuery& query);
};

}
