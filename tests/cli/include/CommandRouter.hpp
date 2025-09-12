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

class CommandRouter {
public:
    explicit CommandRouter(const std::shared_ptr<CLITestContext>& ctx);

    void registerRoute(const std::string& path, const std::function<EntityResult(const std::shared_ptr<void>& entity)>& handler);

    std::shared_ptr<TestCase> route(const std::shared_ptr<TestCase>& test) const;
    std::vector<std::shared_ptr<TestCase>> route(const std::vector<std::shared_ptr<TestCase>>& tests) const;

private:
    std::shared_ptr<EntityRegistrar> registrar_;
    std::pmr::unordered_map<std::string, std::function<EntityResult(const std::shared_ptr<void>& entity)>> routes_{};

    void registerAll();
};

}
