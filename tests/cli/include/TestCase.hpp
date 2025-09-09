#pragma once

#include "EntityType.hpp"

#include <string>
#include <vector>

namespace vh::test::cli {

struct TestCase {
    std::string name;
    std::string path;          // e.g. "user/create"
    int expect_exit{0};
    std::vector<std::string> must_contain;     // stdout contains
    std::vector<std::string> must_not_contain; // stdout must NOT contain

    static TestCase Default() { return TestCase{}; }
    static TestCase ExitOnly(const int code) { return TestCase{ .expect_exit = code }; }
    static TestCase Contains(const std::string& text) { return TestCase{ .must_contain = {text} }; }
    static TestCase NotContains(const std::string& text) { return TestCase{ .must_not_contain = {text} }; }
    static TestCase Full(const std::string& name,
                            const std::string& path,
                            const int expect_exit,
                            const std::vector<std::string>& must_contain,
                            const std::vector<std::string>& must_not_contain) {
        return TestCase{ name, path, expect_exit, must_contain, must_not_contain };
    }

    static TestCase Generate(const EntityType& type, const CommandType& action) {
        const auto typeStr = EntityTypeToString(type);
        const auto actionStr = CommandTypeToString(action);
        return TestCase{
            .name = actionStr + " " + typeStr,
            .path = typeStr + "/" + actionStr,
            .expect_exit = 0,
            .must_contain = {},
            .must_not_contain = {}
        };
    }
};

}
