#pragma once

#include <magic.h>
#include <string>

namespace vh::util {

class Magic {
public:
    Magic();
    ~Magic();

    std::string mime_type(const std::string& path) const;

    static std::string get_mime_type(const std::string& path);

private:
    magic_t cookie;
};

}
