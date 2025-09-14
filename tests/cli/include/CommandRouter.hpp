#pragma once

#include "EntityType.hpp"

#include <unordered_map>
#include <string>
#include <functional>
#include <memory>

namespace vh::test::cli {

struct TestCase;
class EntityRegistrar;
class ListInfoHandler;
class UpdateHandler;
struct CLITestContext;

using singleArgFunc = std::function<EntityResult(const std::shared_ptr<void>& entity)>;
using dualArgFunc = std::function<EntityResult(const std::shared_ptr<void>& entity, const std::shared_ptr<void>& target)>;
using tripleArgFunc = std::function<EntityResult(const std::shared_ptr<void>& entity, const std::shared_ptr<void>& target, const std::shared_ptr<void>& subject)>;
using tripleArgWithSubjTypeFunc = std::function<EntityResult(const std::shared_ptr<void>& entity, const std::shared_ptr<void>& target, const EntityType& subjectType, const std::shared_ptr<void>& subject)>;

using CallType = std::variant<singleArgFunc, dualArgFunc, tripleArgFunc, tripleArgWithSubjTypeFunc>;

class CommandRouter {
public:
    explicit CommandRouter(const std::shared_ptr<CLITestContext>& ctx);

    void registerRoute(const std::string& path, const CallType& handler);

    std::shared_ptr<TestCase> route(const std::shared_ptr<TestCase>& test) const;
    std::vector<std::shared_ptr<TestCase>> route(const std::vector<std::shared_ptr<TestCase>>& tests) const;

private:
    std::shared_ptr<EntityRegistrar> registrar_;
    std::pmr::unordered_map<std::string, CallType> routes_{};

    void registerAll();
};

}
