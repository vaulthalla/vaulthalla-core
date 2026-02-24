#pragma once

#include <memory>
#include <vector>

namespace vh::sync::model { struct Operation; }

namespace vh::database {

struct OperationQueries {
    static void addOperation(const std::shared_ptr<sync::model::Operation>& op);
    static void deleteOperation(unsigned int id);
    static std::vector<std::shared_ptr<sync::model::Operation>> listOperationsByVault(unsigned int vaultId);
    static void markOperationCompleted(const std::shared_ptr<sync::model::Operation>& op);
};

}
