#include "stats/VaultStatTask.hpp"
#include "types/stats/VaultStat.hpp"

using namespace vh::stats;
using namespace vh::types;

VaultStatTask::VaultStatTask(const unsigned int vaultId) : vaultId(vaultId) {}

void VaultStatTask::operator()() {
    try { promise.set_value(std::make_shared<VaultStat>(vaultId)); }
    catch (std::exception& e) { promise.set_exception(std::make_exception_ptr(e)); }
}
