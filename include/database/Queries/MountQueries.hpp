#pragma once

#include <string>

namespace vh::database {
    class MountQueries {
    public:
        MountQueries() = default;

        void createMount(const std::string& mountName, const std::string& mountType);
        void deleteMount(const std::string& mountName);
        bool mountExists(const std::string& mountName);
    };
}
