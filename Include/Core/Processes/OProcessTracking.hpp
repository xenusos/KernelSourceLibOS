/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once
#include <Core/Processes/OProcesses.hpp>

typedef void(*ProcessStartNtfy_cb)(OPtr<OProcess> thread);
typedef void(*ProcessExitNtfy_cb)(OPtr<OProcess> thread);

LIBLINUX_SYM error_t ProcessesAddExitHook(ProcessExitNtfy_cb cb);
LIBLINUX_SYM error_t ProcessesAddStartHook(ProcessStartNtfy_cb cb);

LIBLINUX_SYM error_t ProcessesRemoveExitHook(ProcessExitNtfy_cb cb);
LIBLINUX_SYM error_t ProcessesRemoveStartHook(ProcessStartNtfy_cb cb);
