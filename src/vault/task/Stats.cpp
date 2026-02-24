#include "vault/task/Stats.hpp"
#include "vault/model/Stat.hpp"

using namespace vh::vault::task;
using namespace vh::vault::model;

Stats::Stats(const unsigned int vaultId) : vaultId(vaultId) {}

void Stats::operator()() {
    try { promise.set_value(std::make_shared<Stat>(vaultId)); }
    catch (std::exception& e) { promise.set_exception(std::make_exception_ptr(e)); }
}
