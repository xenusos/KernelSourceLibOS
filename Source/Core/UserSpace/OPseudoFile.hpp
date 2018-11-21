/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <Core\UserSpace\OPseudoFile.hpp>

#define CHARFS_PREFIX "XenusPsuedoFile_"
#define MAX_CHARFS_NAME sizeof(CHARFS_PREFIX "18,446,744,073,709,551,615")

#define PSEUDOFILE_ERROR_CB_ERROR -1
#define PSEUDOFILE_ERROR_NO_HANDLER -2
#define PSEUDOFILE_ERROR_MEM_ERROR -3

enum PsuedoFileType_e
{
    ksSyscallLongPollingGeneric,
    ksIOCTL,
    ksWin32IRPWriteFileReadFile,
    ksLinuxCharDev
};

typedef struct PsudoFileInformation_s
{
    struct
    {
        enum PsuedoFileType_e type;
        union
        {
            struct
            {
                uint64_t char_dev_id;
            } devfs;
        };
    } pub;
    struct
    {
        link_p file_handle;
    } priv;
} PsudoFileInformation_t, *PsudoFileInformation_ref, *PsudoFileInformation_p;


typedef struct chardev_s
{
    char name[MAX_CHARFS_NAME];
    size_t id;
    sysv_fptr_t sysv_fops_open;
    sysv_fptr_t sysv_fops_release;
    sysv_fptr_t sysv_fops_write;
    sysv_fptr_t sysv_fops_read;
    sysv_fptr_t sysv_fops_seek;
    void * handle_fops_open;
    void * handle_fops_release;
    void * handle_fops_write;
    void * handle_fops_read;
    void * handle_fops_seek;
    file_operations_k ops;
    device_k device;
    l_unsigned_int major;
    l_unsigned_int minor;
    dev_t dev;
} chardev_t, *chardev_ref, *chardev_p;

class OPseudoFileImpl : public OPseudoFile
{
public:
    OPseudoFileImpl(PsudoFileInformation_t & info);

    error_t GetIdentifierBlob(const void **, size_t &) override;
    error_t GetPath(const char **)                     override;

    error_t FileOkay(bool & status)                    override;
                                                       
    void OnOpen(PseudofileOpen_t)                      override;
    void OnRelease(PseudofileRelease_t)                override;
    void OnUserRead(PseudofileUserRead_t)              override;
    void OnUserWrite(PseudofileUserWrite_t)            override;
                                                       
    error_t Delete()                                   override;
                                                       
    PsudoFileInformation_p GetInfo();
    uint64_t GetCharDevId();

    chardev_t * GetCharDev();

    PseudofileUserWrite_t write_cb;
    PseudofileUserRead_t  read_cb;
    PseudofileRelease_t   release_cb;
    PseudofileOpen_t	  open_cb;
    loff_t seek;

protected:
    void InvalidateImp()                               override;

private:
    bool _dead;
    PsudoFileInformation_t _info;
    chardev_t _chardev;
    char _path[256];
};


void InitPseudoFiles();