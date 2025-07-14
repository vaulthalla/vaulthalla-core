#include "crypto/PasswordHash.hpp"
#include "crypto/PasswordUtils.hpp"

#include <iostream>
#include <string>
#include <sstream>
#include <ranges>
#include <algorithm>

void validatePassword(const std::string& password) {
    vh::auth::PasswordUtils::loadCommonWeakPasswordsFromURLs(
        {"https://raw.githubusercontent.com/danielmiessler/SecLists/refs/heads/master/Passwords/Common-Credentials/"
         "100k-most-used-passwords-NCSC.txt",
         "https://raw.githubusercontent.com/danielmiessler/SecLists/refs/heads/master/Passwords/Common-Credentials/"
         "probable-v2_top-12000.txt"});

    vh::auth::PasswordUtils::loadDictionaryFromURL(
        "https://raw.githubusercontent.com/dolph/dictionary/refs/heads/master/popular.txt");

    std::vector<std::string> errors;

    if (password.empty())
        errors.emplace_back("password is empty");

    if (password.size() < 12 || password.size() > 128)
        errors.emplace_back("Password must be between 12 and 128 characters long.");

    if (std::ranges::any_of(password.begin(), password.end(), ::isdigit)) // At least one digit
        errors.emplace_back("Password must contain at least one digit.");

    if (std::ranges::any_of(password.begin(), password.end(), ::isalpha)) // At least one letter
        errors.emplace_back("Password must contain at least one letter.");

    if (const auto strength = vh::auth::PasswordUtils::passwordStrengthCheck(password) < 50)
        errors.emplace_back("Password is too weak (strength " + std::to_string(strength) +
                            "/100). Use at least 12 characters, mix upper/lowercase, digits, and symbols.");

    if (vh::auth::PasswordUtils::containsDictionaryWord(password))
        errors.emplace_back("Password contains dictionary word — this is forbidden.");

    if (vh::auth::PasswordUtils::isCommonWeakPassword(password))
        errors.emplace_back("Password matches known weak pattern — this is forbidden.");

    if (vh::auth::PasswordUtils::isPwnedPassword(password))
        errors.emplace_back("Password has been found in public breaches — choose a different one.");

    if (!errors.empty()) {
        std::ostringstream oss;
        oss << "Registration failed due to the following issues:\n";
        for (const auto& err : errors) oss << "- " << err << std::endl;
        throw std::runtime_error(oss.str());
    }
}

int main(int argc, char* argv[]) {
    bool validate = false;
    std::string password;

    if (argc == 3 && std::string(argv[1]) == "--validate") {
        validate = true;
        password = argv[2];
    } else if (argc == 2) {
        password = argv[1];
    } else {
        std::cerr << "Usage:\n"
                  << "  " << argv[0] << " <password_to_hash>\n"
                  << "  " << argv[0] << " --validate <password_to_hash>\n";
        return 1;
    }

    try {
        if (validate) validatePassword(password);
        const auto hashed = vh::crypto::hashPassword(password);
        std::cout << hashed << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 3;
    }
}
