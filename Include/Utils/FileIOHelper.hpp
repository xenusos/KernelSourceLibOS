/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  
#pragma once

namespace IOHelpers
{
    LIBLINUX_SYM bool UpDir(const char * path, char * dest);
    LIBLINUX_SYM error_t DeprecatedNuke(const  char * fileordir_path, char * path = nullptr);
    LIBLINUX_SYM error_t WriteAllToFile(char * path, void * buffer, size_t length);
    LIBLINUX_SYM error_t WriteAllToFile(char * path, char * string);
}