#include "util/Magic.hpp"
#include <stdexcept>

using namespace vh::util;

Magic::Magic() {
    cookie = magic_open(MAGIC_MIME_TYPE);
    if (!cookie) throw std::runtime_error("Failed to create magic cookie");
    if (magic_load(cookie, "/usr/share/misc/magic.mgc") != 0) {
        const std::string err = magic_error(cookie);
        magic_close(cookie);
        throw std::runtime_error("Failed to load magic database: " + err);
    }
}

Magic::~Magic() { if (cookie) magic_close(cookie); }

std::string Magic::mime_type(const std::string& path) const {
    const char* result = magic_file(cookie, path.c_str());
    if (!result) throw std::runtime_error("magic_file failed: " + std::string(magic_error(cookie)));
    return {result};
}

std::string Magic::get_mime_type(const std::string& path) {
    static Magic instance;
    return instance.mime_type(path);
}

