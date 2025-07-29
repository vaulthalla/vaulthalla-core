#pragma once

#include <memory>
#include <vector>

namespace vh::types {
struct Operation;
}

using namespace vh::types;

namespace vh::database {

struct OperationQueries {
    static void addOperation(const std::shared_ptr<Operation>& op);
    static void deleteOperation(unsigned int id);
    static std::vector<std::shared_ptr<Operation>> listOperationsByVault(unsigned int vaultId);
    static void markOperationCompleted(const std::shared_ptr<Operation>& op);
};

}
