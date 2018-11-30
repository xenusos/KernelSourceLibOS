/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  
#include <libos.hpp>

namespace IOHelpers
{
    bool UpDir(const char * path, char * dest)
    {
        size_t i;
        size_t length;

        length = strlen(path);
        memcpy(dest, path, length + 1);

        i = length - 1;
        if (path[i] == '/')
            i--; 

        for (; i > 0; i--)
        {
            if (path[i] == '/')
            {
                dest[i + 1] = 0x00;
                break;
            }
        }

        return i != 0;
    }
}