#pragma once

#include "CLITestContext.hpp"
#include "EntityType.hpp"

#include <memory>
#include <string>

namespace vh::test::cli {

class ListInfoHandler {
public:
    explicit ListInfoHandler(const std::shared_ptr<CLITestContext>& ctx) : ctx_(ctx) {}

    void list(const EntityType& type) const;

    void info(const EntityType& type, const unsigned int id) const;

private:
    std::shared_ptr<CLITestContext> ctx_;
};

}
