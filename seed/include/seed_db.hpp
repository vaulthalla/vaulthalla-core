#pragma once

namespace vh::seed {

void seed_database();
void initPermissions();
void initRoles();
void initAdmin();
void initAdminGroup();
void initAdminDefaultVault();
void initRoot();

// dev
void initDevCloudVault();

}
