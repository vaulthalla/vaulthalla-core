#include "ShellUsage.hpp"
#include "CommandUsage.hpp"

#include <iostream>
#include <fstream>

using namespace vh::shell;

int main(const int argc, char** argv) {
    std::string out;
    if (argc == 3 && std::string(argv[1]) == "--out") out = argv[2];

    const auto md = ShellUsage::all().markdown();
    if (out.empty()) std::cout << md;
    else {
        std::ofstream f(out);
        f << md;
    }
    return 0;
}
