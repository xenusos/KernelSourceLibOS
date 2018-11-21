/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  
#include <libos.hpp>

#include <ITypes\IPath.hpp>
#include <ITypes\IFile.hpp>


namespace IOHelpers
{
    bool UpDir(const  char * path, char * dest)
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
                *(char **)(&(dest[i + 1])) = (char *)0x00;
                break;
            }
        }

        return i != 0;
    }

    error_t DeprecatedNuke(const  char * dirorfile_path, const  char * opt_dir_path)
    {
        file_k path;
        file_k target;
        char dir_path[256];
        inode_k delegated_inode;
        bool is_dir;

        is_dir = dirorfile_path[strlen(dirorfile_path) - 1] == '/';

        if (opt_dir_path)
        {
            memcpy(dir_path, opt_dir_path, strlen(opt_dir_path) + 1);
        }
        else
        {
            if (!UpDir((char *)file_path, dir_path))
                return kErrorGenericFailure;
        }

        if (!(target = filp_open(dirorfile_path, O_RDONLY | (is_dir ? O_DIRECTORY : 0), 0600)))
        {
            /* nothing to close */
            return kErrorFileNullHandle;
        }

        if (!(path = filp_open(dir_path, O_DIRECTORY, 0600)))
        {
            filp_close(target, 0);
            return kErrorFileNullHandle;
        }

        if (is_dir)
        {
            vfs_rmdir(IFile(path).GetINode(), IPath(IFile(target).GetPath()).GetDEntry());
        }
        else
        {
            vfs_unlink(IFile(path).GetINode(),
                IPath(IFile(target).GetPath()).GetDEntry(),
                &delegated_inode);
        }

        filp_close(target, 0);
        filp_close(path, 0);

        return kStatusOkay;
    }

    error_t WriteAllToFile(char * path, void * buffer, size_t length)
    {
        error_t ret;
        loff_t pos;
        file_k file;

        ret = kStatusOkay;

        if (!(file = filp_open(path, O_CREAT | O_RDWR, 0777)))
            return kErrorFileNullHandle;

        if (kernel_write(file, buffer, length, &pos) != length)
            ret = kErrorGenericFailure;

        if (!filp_close(file, 0))
            ret = kErrorGenericFailure;

        return ret;
    }

    error_t WriteAllToFile(char * path, char * string)
    {
        return WriteAllToFile(path, string, strlen(string));
    }
}