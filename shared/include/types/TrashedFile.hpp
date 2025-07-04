#pragma once

#include "File.hpp"

#include <optional>

namespace pqxx {
    class row;
}

namespace vh::types {

struct TrashedFile : File {
    bool is_trashed{};
    std::optional<std::time_t> trashed_at;
    unsigned int trashed_by{};

    TrashedFile() = default;
    explicit TrashedFile(const pqxx::row& row);
};

}
