#include <cstring>

extern "C" char *strchrnul(const char *s, int c) {
    const char *p = strchr(s, c);
    return const_cast<char*>(p ? p : s + strlen(s));
}
