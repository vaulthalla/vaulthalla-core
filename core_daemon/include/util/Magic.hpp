#pragma once

#include <string>
#include <magic.h>

namespace vh::util {

class Magic {
public:
    Magic();
    ~Magic();

    std::string mime_type(const std::string& path) const;
    std::string mime_type_buffer(const std::string& buffer) const;

    static std::string get_mime_type(const std::string& path);
    static std::string get_mime_type_from_buffer(const std::string& buffer);

private:
    magic_t cookie;
};

}
