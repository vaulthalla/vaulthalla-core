#include "crypto/PasswordHash.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <password_to_hash>\n";
        return 1;
    }

    const std::string plainTextPassword = argv[1];

    try {
        std::string hashed = vh::crypto::hashPassword(plainTextPassword);
        std::cout << hashed << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error hashing password: " << e.what() << std::endl;
        return 2;
    }
}
