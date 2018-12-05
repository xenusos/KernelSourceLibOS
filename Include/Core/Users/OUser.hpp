#pragma once

enum UserPermissionLevel
{                            // NT     | LINUX | XENUS
    kUserMicrokernel = 0,    // SYSTEM | N/A   | N/A
    kUserServer,             // N/A    | N/A   | root
    kUserAdministrator,      // Admin  | root  | Admin
    kUserUser                // User   | User  | User
};

struct UserID
{
    uint64_t part_a;
    uint64_t part_b;
    uint64_t part_c;
    uint64_t part_d;
};