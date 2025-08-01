#pragma once

#include <string>
#include <filesystem>

namespace vh::crypto {

class Hash {
public:
    static std::string blake2b(const std::filesystem::path& filepath);
};

}
