#pragma once

namespace vh::test::cli {

struct CLITestConfig {
    unsigned int numUsers = 10,
           numVaults = 15,
           numGroups = 5,
           numUserRoles = 7,
           numVaultRoles = 7;

    static CLITestConfig Default() { return CLITestConfig{}; }

    static CLITestConfig Minimal() {
        return CLITestConfig{
            .numUsers = 2,
            .numVaults = 2,
            .numGroups = 1,
            .numUserRoles = 1,
            .numVaultRoles = 1
        };
    }

    static CLITestConfig Large() {
        return CLITestConfig{
            .numUsers = 50,
            .numVaults = 75,
            .numGroups = 20,
            .numUserRoles = 15,
            .numVaultRoles = 15
        };
    }

    static CLITestConfig ExtraLarge() {
        return CLITestConfig{
            .numUsers = 100,
            .numVaults = 150,
            .numGroups = 50,
            .numUserRoles = 25,
            .numVaultRoles = 25
        };
    }

    static CLITestConfig NG_STRESS() {
        return CLITestConfig{
            .numUsers = 500,
            .numVaults = 750,
            .numGroups = 100,
            .numUserRoles = 50,
            .numVaultRoles = 50
        };
    }
};

}
