#include "stats/VaultStatTask.hpp"
#include "types/stats/VaultStat.hpp"

using namespace vh::stats;
using namespace vh::types;

VaultStatTask::VaultStatTask(const unsigned int vaultId) : vaultId(vaultId) {}

void VaultStatTask::operator()() {
    try {
        stat = std::make_shared<VaultStat>(vaultId);
        promise.set_value(stat);
    } catch (std::exception& e) {
        promise.set_exception(std::make_exception_ptr(e));
    }
}
