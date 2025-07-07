#include "services/Vaulthalla.hpp"
#include <iostream>

int main() {
    try {
        vh::services::Vaulthalla().start();
    } catch (const std::exception& e) {
        std::cerr << "Vaulthalla failed to start: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
