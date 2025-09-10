#pragma once

#include "CLITestContext.hpp"
#include "EntityType.hpp"
#include "CommandBuilder.hpp"
#include "database/Queries/UserQueries.hpp"
#include "protocols/shell/Router.hpp"
#include "protocols/shell/SocketIO.hpp"

#include <memory>
#include <string>

namespace vh::test::cli {

class ListInfoHandler {
public:
    explicit ListInfoHandler(const std::shared_ptr<CLITestContext>& ctx,
                             const std::shared_ptr<shell::Router>& router) : ctx_(ctx), router_(router) {}

    EntityResult list(const EntityType& type) const {
        const auto cmd = ctx_->getCommand(type, "list");
        if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for listing");
        const auto command = CommandBuilder(cmd).build();
        const auto admin = database::UserQueries::getUserByName("admin");
        int fd;
        const auto io = std::make_unique<shell::SocketIO>(fd);
        return {router_->executeLine(command, admin, io.get())};
    }

    template <typename T>
    EntityResult info(const EntityType& type, const std::shared_ptr<T>& entity) const {
        const auto cmd = ctx_->getCommand(type, "info");
        if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for info");

        const auto command = CommandBuilder(cmd).withEntity(*entity).build();
        const auto admin = database::UserQueries::getUserByName("admin");
        int fd;
        const auto io = std::make_unique<shell::SocketIO>(fd);
        return {router_->executeLine(command, admin, io.get())};
    }

private:
    std::shared_ptr<CLITestContext> ctx_;
    std::shared_ptr<shell::Router> router_;
};

}
