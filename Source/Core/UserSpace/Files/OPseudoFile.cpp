/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>

#include <ITypes/IFileOperations.hpp>
#include "OPseudoFile.hpp"

static mutex_k pfns_mutex;
static chain_p pseudo_file_handles;
static class_k psudo_file_class;

static void FreeFileHandle(OPseudoFileImpl * out);
static void FreeCharDev(chardev_ref chardev);

OPseudoFileImpl::OPseudoFileImpl(PsudoFileInformation_t & info)
{
    _chardev   = { 0 };
    _info      = info;
    write_cb   = 0;
    read_cb    = 0;
    release_cb = 0;
    open_cb    = 0;
    snprintf(_path, sizeof(_path), "/dev/" CHARFS_PREFIX "%lli", info.pub.devfs.char_dev_id);
}

error_t OPseudoFileImpl::GetIdentifierBlob(const void ** buf, size_t & len)
{
    CHK_DEAD;
    uint64_t length;
    *buf = &_info;
    len = sizeof(_info.pub);
    return kStatusOkay;
}

error_t OPseudoFileImpl::GetPath(const char ** path)
{
    CHK_DEAD;
    *path = _path;
    return kStatusOkay;
}

error_t OPseudoFileImpl::FileOkay(bool & status)
{
    CHK_DEAD;
    status = !_dead;
    return kStatusOkay;
}

void OPseudoFileImpl::OnOpen(PseudofileOpen_t cb)
{
    CHK_DEAD_RET_VOID;
    open_cb = cb;
}

void OPseudoFileImpl::OnRelease(PseudofileRelease_t cb)
{
    CHK_DEAD_RET_VOID;
    release_cb = cb;
}

void OPseudoFileImpl::OnUserRead(PseudofileUserRead_t cb)
{
    CHK_DEAD_RET_VOID;
    read_cb = cb;
}

void OPseudoFileImpl::OnUserWrite(PseudofileUserWrite_t cb)
{
    CHK_DEAD_RET_VOID;
    write_cb = cb;
}

PsudoFileInformation_p OPseudoFileImpl::GetInfo()
{
    CHK_DEAD_RET_NULL;
    return &_info;
}

uint64_t OPseudoFileImpl::GetCharDevId()
{
    CHK_DEAD_RET_ZERO;
    return _info.pub.devfs.char_dev_id;
}

chardev_t * OPseudoFileImpl::GetCharDev()
{
    CHK_DEAD_RET_NULL;
    return &_chardev;
}

error_t OPseudoFileImpl::Delete()
{
    CHK_DEAD;
    Invalidate();
    return kStatusOkay;
}

void OPseudoFileImpl::InvalidateImp()
{
    FreeCharDev(GetCharDev());
    FreeFileHandle(this);
}

#define PSEUDOFILE_OPTR_THIS OPtr<OPseudoFile>(((OPseudoFile *)(SYSV_GET_DATA))))
#define PSEUDOFILE_IMPL_THIS ((OPseudoFileImpl *)(SYSV_GET_DATA))

DEFINE_SYSV_FUNCTON_START(fop_open, l_int)
inode_k node,
file_k file,
uint64_t arg_padding_1,
uint64_t arg_padding_2, // theres a 4 argument prerequisite
DEFINE_SYSV_FUNCTON_END_DEF(fop_open, l_int)
{
    l_int ret;
    OPseudoFileImpl::PseudofileOpen_t cb;

    cb = PSEUDOFILE_IMPL_THIS->open_cb;

    if (!cb)
    {
        SYSV_FUNCTON_RETURN(0);
    }

    ret = cb(PSEUDOFILE_IMPL_THIS) ? 0 : PSEUDOFILE_ERROR_CB_ERROR;
    SYSV_FUNCTON_RETURN(ret)
}
DEFINE_SYSV_END

DEFINE_SYSV_FUNCTON_START(fop_release, l_int)
inode_k node,
file_k file,
uint64_t arg_padding_1,
uint64_t arg_padding_2, // theres a 4 argument prerequisite for dynamic callbacks to work [i <3 msfts abi]
DEFINE_SYSV_FUNCTON_END_DEF(fop_release, l_int)
{
    OPseudoFileImpl::PseudofileRelease_t cb;

    cb = PSEUDOFILE_IMPL_THIS->release_cb;

    if (!cb)
    {
        SYSV_FUNCTON_RETURN(0);
    }

    cb(PSEUDOFILE_IMPL_THIS);
    SYSV_FUNCTON_RETURN(0)
}
DEFINE_SYSV_END

DEFINE_SYSV_FUNCTON_START(fop_read, ssize_t)
file_k file,
user_addr_t buffer,
size_t len,
loff_t * off,
DEFINE_SYSV_FUNCTON_END_DEF(fop_read, ssize_t)
{
    size_t written;
    bool failed;
    void * buf;
    size_t of;
    OPseudoFileImpl::PseudofileUserRead_t cb;

    cb = PSEUDOFILE_IMPL_THIS->read_cb;

    if (!cb)
    {
        SYSV_FUNCTON_RETURN(PSEUDOFILE_ERROR_NO_HANDLER)
    }
    of = off ? *off : file_get_f_pos_int64(file);

    buf = malloc(len);

    if (!buf)
    {
        SYSV_FUNCTON_RETURN(PSEUDOFILE_ERROR_MEM_ERROR)
    }

    failed = cb(PSEUDOFILE_IMPL_THIS, buf, len, of, &written);

    if (!failed)
    {
        free(buf);
        SYSV_FUNCTON_RETURN(PSEUDOFILE_ERROR_CB_ERROR)
    }

    if (off)
        *off = written + of;

    _copy_to_user(buffer, buf, written);
    free(buf);
    SYSV_FUNCTON_RETURN(written)
}
DEFINE_SYSV_END

DEFINE_SYSV_FUNCTON_START(fop_write, ssize_t)
file_k file,
user_addr_t buffer,
size_t len,
loff_t *off,
DEFINE_SYSV_FUNCTON_END_DEF(fop_write, ssize_t)
{
    size_t read;
    bool failed;
    void * buf;
    OPseudoFileImpl::PseudofileUserWrite_t cb;

    cb = PSEUDOFILE_IMPL_THIS->write_cb;

    if (!cb)
    {
        SYSV_FUNCTON_RETURN(PSEUDOFILE_ERROR_NO_HANDLER)
    }

    buf = malloc(len);

    if (!buf)
    {
        SYSV_FUNCTON_RETURN(PSEUDOFILE_ERROR_MEM_ERROR)
    }

    _copy_from_user(buf, buffer, len);

    failed = cb(PSEUDOFILE_IMPL_THIS, buf, len, off ? *off : file_get_f_pos_int64(file), &read);

    if (!failed)
    {
        free(buf);
        SYSV_FUNCTON_RETURN(PSEUDOFILE_ERROR_CB_ERROR)
    }

    free(buf);
    SYSV_FUNCTON_RETURN(read)
}
DEFINE_SYSV_END

DEFINE_SYSV_FUNCTON_START(fop_seek, loff_t)
file_k file,
loff_t offset,
int whence,
void *pad,
DEFINE_SYSV_FUNCTON_END_DEF(fop_seek, loff_t)
{
    loff_t newpos;

    switch (whence) {
    case 0: /* SEEK_SET */
        newpos = offset;
        break;

    case 1: /* SEEK_CUR */
        newpos = file_get_f_pos_int64(file) + offset;
        break;

    case 2: /* SEEK_END */
        LogPrint(kLogWarning, "A pseudodevice issued a size relative seek request - we are never aware of the file size");
        newpos = -1;
        break;

    default: /* can't happen */
        return -1;
    }

    file_set_f_pos_int64(file, newpos);
    SYSV_FUNCTON_RETURN(offset)
}
DEFINE_SYSV_END

static error_t GetNextFileId(size_t & id)
{
    struct FileIterCtx_s
    {
        uint64_t i;
        bool found;
    } cur;

    for (size_t i = 0; i < SIZE_T_MAX; i++)
    {
        cur.i = i;
        cur.found = false;
        chain_iterator(pseudo_file_handles, [](uint64_t hash, void * buffer, void * context)
        {
            FileIterCtx_s * ctx = (FileIterCtx_s *)context;
            if (hash == ctx->i)
                ctx->found = true;
        }, &cur);

        if (!cur.found)
            break;
    }

    if (cur.found)
        return kErrorOutOfUIDs;

    id = cur.i;

    return kStatusOkay;
}

static error_t AllocateNewFileHandle(OPseudoFileImpl ** out)
{
    error_t er;
    size_t id;
    OPseudoFileImpl * file;
    OPseudoFileImpl ** handle_ref;
    PsudoFileInformation_t info;

    mutex_lock(pfns_mutex);

    if (ERROR(er = GetNextFileId(id)))
    {
        mutex_unlock(pfns_mutex);
        return er;
    }

    info.pub.devfs.char_dev_id = id;
    info.pub.type = PsuedoFileType_e::ksLinuxCharDev;

    if (ERROR(er = chain_allocate_link(pseudo_file_handles, id, sizeof(size_t), NULL, &info.priv.file_handle, (void **)&handle_ref)))
    {
        mutex_unlock(pfns_mutex);
        return er;
    }

    file = new OPseudoFileImpl(info);
    if (!file)
    {
        chain_deallocate_handle(info.priv.file_handle);
        mutex_unlock(pfns_mutex);
        return kErrorOutOfMemory;
    }

    *handle_ref = file;
    *out = file;

    mutex_unlock(pfns_mutex);
    return kStatusOkay;
}

static void FreeFileHandle(OPseudoFileImpl * out)
{
    mutex_lock(pfns_mutex);

    chain_deallocate_handle(out->GetInfo()->priv.file_handle);

    mutex_unlock(pfns_mutex);
}

void InitPseudoFiles()
{
    error_t er;
    lock_class_key temp;

    psudo_file_class = __class_create(0/* Lets just impersonate the linux kernel*/, "xenus", (lock_class_key_k)&temp);
    if (LINUX_PTR_ERROR(psudo_file_class))
    {
        panic("couldn't register pseudofile class");
        return;
    }

    er = chain_allocate(&pseudo_file_handles);
    if (ERROR(er))
        panicf("couldn't allocate file handle chain: error code " PRINTF_ERROR, er);

    pfns_mutex = mutex_allocate();
    ASSERT(pfns_mutex, "couldn't allocate file tracker mutex");
}

static void FreeCharDev(chardev_ref chardev)
{
    __unregister_chrdev(chardev->major, chardev->minor, 256, chardev->name);
    device_destroy(psudo_file_class, chardev->dev);
    if (chardev->ops)
        free(chardev->ops);
    if (chardev->handle_fops_open)
        dyncb_free_stub(chardev->handle_fops_open);
    if (chardev->handle_fops_release)
        dyncb_free_stub(chardev->handle_fops_release);
    if (chardev->handle_fops_write)
        dyncb_free_stub(chardev->handle_fops_write);
    if (chardev->handle_fops_read)
        dyncb_free_stub(chardev->handle_fops_read);
    if (chardev->handle_fops_seek)
        dyncb_free_stub(chardev->handle_fops_seek);
}

static error_t CreateCharDev(OPseudoFileImpl * file)
{
    l_int major;
    error_t ret;
    file_operations_k k;
    chardev_p chardev;

    chardev = file->GetCharDev();
    chardev->ops = k = file_operations_allocate();
    chardev->id = file->GetCharDevId();

    snprintf((char *)chardev->name, MAX_CHARFS_NAME, CHARFS_PREFIX "%lli", uint64_t(chardev->id));

#define ALLOCATE_DYNCB(function, args, sysv_out, handle_out)                                      \
    ret = dyncb_allocate_stub(SYSV_FN(function), args, (void *)file, sysv_out, handle_out);       \
    if (ERROR(ret))                                                                               \
        return ret;

    ALLOCATE_DYNCB(fop_open, 4, &chardev->sysv_fops_open, &chardev->handle_fops_open);
    ALLOCATE_DYNCB(fop_release, 4, &chardev->sysv_fops_release, &chardev->handle_fops_release);
    ALLOCATE_DYNCB(fop_write, 4, &chardev->sysv_fops_write, &chardev->handle_fops_write);
    ALLOCATE_DYNCB(fop_read, 4, &chardev->sysv_fops_read, &chardev->handle_fops_read);
    ALLOCATE_DYNCB(fop_seek, 4, &chardev->sysv_fops_seek, &chardev->handle_fops_seek);

    file_operations_set_open_size_t(k, size_t(chardev->sysv_fops_open));
    file_operations_set_release_size_t(k, size_t(chardev->sysv_fops_release));
    file_operations_set_write_size_t(k, size_t(chardev->sysv_fops_write));
    file_operations_set_read_size_t(k, size_t(chardev->sysv_fops_read));
    file_operations_set_llseek_size_t(k, size_t(chardev->sysv_fops_seek));

    major = __register_chrdev(0, 0, 256, chardev->name, k);

    chardev->major = (l_unsigned_int)major;
    chardev->minor = 0;
    chardev->dev = MKDEV(major, 0);

    if (major < 0)
    {
        LogPrint(kLogError, "Failed to register linux file - register chrdev failed, major = %i (whatever that means... linux is awful)\n", major);
        return kErrorGenericFailure;
    }

    if (LINUX_PTR_ERROR(chardev->device = device_create(psudo_file_class, NULL, chardev->dev, nullptr, chardev->name)))
    {
        LogPrint(kLogError, "Internal Linux Error - couldn't register device (%i, whatever that means... linux is awful)", chardev->device);
        return kErrorGenericFailure;
    }

    return kStatusOkay;
}

error_t CreateTempKernFile(const OOutlivableRef<OPseudoFile> & out)
{
    OPseudoFileImpl * file;
    error_t er;

    er = AllocateNewFileHandle(&file);
    if (ERROR(er))
        return er;

    er = CreateCharDev(file);
    if (ERROR(er))
    {
        file->Destroy();
        return er;
    }

    out.PassOwnership(file);
    return kStatusOkay;
}
