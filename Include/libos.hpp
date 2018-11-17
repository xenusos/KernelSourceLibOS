/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  
#pragma once

#define LIB_XENUS_PRESENT

#ifdef LIBLINUX_EXPORTS
#define LIBLINUX_SYM __declspec(dllexport) 
#define LIBLINUX_ASM
#else 
#define LIBLINUX_SYM __declspec(dllimport) 
#define LIBLINUX_ASM __declspec(dllimport) 
#endif

#include "Core\Base\Objects.hpp"