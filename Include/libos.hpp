/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  
#pragma once

#define LIB_OS_PRESENT

#if defined(LIBLINUX_BUILDING)
    #define LIBLINUX_SYM __declspec(dllexport) 
    #define LIBLINUX_ASM
#else 
    #define LIBLINUX_SYM __declspec(dllimport) 
    #define LIBLINUX_ASM __declspec(dllimport) 
#endif

#if defined(LIBLINUX_BUILDING) && !defined(LIB_COMPILER_PRESENT)
    #include <xenus_lazy.h>
    #include <libtypes.hpp>
    #include <libcompiler.hpp>
    #define LOG_MOD "LibOS"
#endif

#include "Base\Objects\Objects.hpp"
#include "Base\Logging\Logging.hpp"