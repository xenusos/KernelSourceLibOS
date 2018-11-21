/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <libos.hpp>

#include <ITypes/IFileOperations.hpp>
#include "OPseudoFile.hpp"

#define LOG_MOD "LibOS"
#include <Logging\Logging.hpp>

mutex_k pfns_mutex;
chain_p pseudo_file_handles;
class_k psudo_file_class;

void FreeFileHandle(OPseudoFileImpl * out);
void FreeCharDev(chardev_ref chardev);

OPseudoFileImpl::OPseudoFileImpl(PsudoFileInformation_t & info)
{
    _chardev   = { 0 };
    _info      = info;
    write_cb   = 0;
    read_cb    = 0;
    release_cb = 0;
    open_cb    = 0;
    seek       = 0;
    snprintf(_path, sizeof(_path), "/dev/" CHARFS_PREFIX "%lli", info.pub.devfs.char_dev_id);
}

error_t OPseudoFileImpl::GetIdentifierBlob(const void ** buf, size_t & len)
{
    uint64_t length;
    *buf = &_info;
    len = sizeof(_info.pub);
    return kStatusOkay;
}

error_t OPseudoFileImpl::GetPath(const char ** path)
{
    *path = _path;
    return kStatusOkay;
}

error_t OPseudoFileImpl::FileOkay(bool & status)
{
    status = !_dead;
    return kStatusOkay;
}

void OPseudoFileImpl::OnOpen(PseudofileOpen_t cb)
{
    open_cb = cb;
}

void OPseudoFileImpl::OnRelease(PseudofileRelease_t cb)
{
    release_cb = cb;
}

void OPseudoFileImpl::OnUserRead(PseudofileUserRead_t cb)
{
    read_cb = cb;
}

void OPseudoFileImpl::OnUserWrite(PseudofileUserWrite_t cb)
{
    write_cb = cb;
}

PsudoFileInformation_p OPseudoFileImpl::GetInfo()
{
    return &_info;
}

uint64_t OPseudoFileImpl::GetCharDevId()
{
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
        SYSV_FUNCTON_RETURN(0);

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
        SYSV_FUNCTON_RETURN(0);

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
        SYSV_FUNCTON_RETURN(PSEUDOFILE_ERROR_NO_HANDLER)

        of = off ? *off : PSEUDOFILE_IMPL_THIS->seek;

    buf = malloc(len);

    if (!buf)
        SYSV_FUNCTON_RETURN(PSEUDOFILE_ERROR_MEM_ERROR)

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
        SYSV_FUNCTON_RETURN(PSEUDOFILE_ERROR_NO_HANDLER)

        buf = malloc(len);

    if (!buf)
        SYSV_FUNCTON_RETURN(PSEUDOFILE_ERROR_MEM_ERROR)

        _copy_from_user(buf, buffer, len);

    failed = cb(PSEUDOFILE_IMPL_THIS, buf, len, off ? *off : PSEUDOFILE_IMPL_THIS->seek, &read);

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
loff_t	offset,
int whence,
void *pad,
DEFINE_SYSV_FUNCTON_END_DEF(fop_seek, loff_t)
{
    OPseudoFileImpl * impl;
    loff_t newpos;

    impl = PSEUDOFILE_IMPL_THIS;


    switch (whence) {
    case 0: /* SEEK_SET */
        newpos = offset;
        break;

    case 1: /* SEEK_CUR */
        newpos = impl->seek + offset;
        break;

    case 2: /* SEEK_END */
        LogPrint(kLogWarning, "A pseudodevice issued a size relative seek request - we are never aware of the file size");
        newpos = -1;
        break;

    default: /* can't happen */
        return -1;
    }

    impl->seek = newpos;
    file_set_f_pos_int64(file, newpos);
    SYSV_FUNCTON_RETURN(offset)
}
DEFINE_SYSV_END


error_t AllocateNewFileHandle(OPseudoFileImpl ** out)
{
    struct FileIterCtx_s
    {
        uint64_t i;
        bool found;
    };

    error_t er;
    uint64_t id;
    OPseudoFileImpl * file;
    OPseudoFileImpl ** handle_ref;
    PsudoFileInformation_t info;

    mutex_lock(pfns_mutex);

    FileIterCtx_s cur;
    for (uint64_t i = 0; i < UINT64_MAX; i++)
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
    {
        mutex_unlock(pfns_mutex);
        return kErrorOutOfMemory;
    }

    info.pub.type = PsuedoFileType_e::ksLinuxCharDev;
    info.pub.devfs.char_dev_id = cur.i;

    if (ERROR(er = chain_allocate_link(pseudo_file_handles, cur.i, sizeof(size_t), NULL, &info.priv.file_handle, (void **)&handle_ref)))
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

void FreeFileHandle(OPseudoFileImpl * out)
{
    mutex_lock(pfns_mutex);

    chain_deallocate_handle(out->GetInfo()->priv.file_handle);

    mutex_unlock(pfns_mutex);
}

void InitPseudoFiles()
{
    lock_class_key temp;

    psudo_file_class = __class_create(0/* fuck it. lets impersonate the linux kernel*/, "xenus", (lock_class_key_k)&temp);
    if (LINUX_ERROR(psudo_file_class))
    {
        panic("couldn't register pseudofile class");
        return;
    }

    error_t er;
    if (ERROR(er = chain_allocate(&pseudo_file_handles)))
        panicf("couldn't allocate file handle chain: error code %lli", er);

    pfns_mutex = mutex_allocate();
    ASSERT(pfns_mutex, "couldn't allocate file tracker mutex");
}

void FreeCharDev(chardev_ref chardev)
{
    __unregister_chrdev(chardev->major, chardev->minor, 256, chardev->name);
    device_destroy(psudo_file_class, chardev->dev);
    free(chardev->ops);
    dyncb_free_stub(chardev->handle_fops_open);
    dyncb_free_stub(chardev->handle_fops_release);
    dyncb_free_stub(chardev->handle_fops_write);
    dyncb_free_stub(chardev->handle_fops_read);
    dyncb_free_stub(chardev->handle_fops_seek);
}

error_t CreateCharDev(OPseudoFileImpl * file)
{
    l_int major;
    error_t ret;
    file_operations_k k;
    chardev_p chardev;

    chardev = file->GetCharDev();
    chardev->ops = k = file_operations_allocate();
    chardev->id = file->GetCharDevId();

    snprintf((char *)chardev->name, MAX_CHARFS_NAME, CHARFS_PREFIX "%lli", uint64_t(chardev->id));

    if (ERROR(ret = dyncb_allocate_stub(SYSV_FN(fop_open), 4, (void *)file, &chardev->sysv_fops_open, &chardev->handle_fops_open)))
        return ret;

    if (ERROR(ret = dyncb_allocate_stub(SYSV_FN(fop_release), 4, (void *)file, &chardev->sysv_fops_release, &chardev->handle_fops_release)))
        return ret;

    if (ERROR(ret = dyncb_allocate_stub(SYSV_FN(fop_write), 4, (void *)file, &chardev->sysv_fops_write, &chardev->handle_fops_write)))
        return ret;

    if (ERROR(ret = dyncb_allocate_stub(SYSV_FN(fop_read), 4, (void *)file, &chardev->sysv_fops_read, &chardev->handle_fops_read)))
        return ret;

    if (ERROR(ret = dyncb_allocate_stub(SYSV_FN(fop_seek), 4, (void *)file, &chardev->sysv_fops_seek, &chardev->handle_fops_seek)))
        return ret;

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
        printf("Failed to register linux file - register chrdev: %i\n", major);
        return kErrorGenericFailure;
    }

    if (LINUX_ERROR(chardev->device = device_create(psudo_file_class, NULL, chardev->dev, nullptr, chardev->name)))
    {
        printf("Internal Linux Error - couldn't register device");
        return kErrorGenericFailure;
    }

    return kStatusOkay;
}

error_t CreateTempKernFile(const OOutlivableRef<OPseudoFile> & out)
{
    OPseudoFileImpl * file;
    error_t er;

    if (ERROR(er = AllocateNewFileHandle(&file)))
        return er;

    if (ERROR(er = CreateCharDev(file)))
    {
        file->Destory();
        return er;
    }

    out.PassOwnership(file);
    return kStatusOkay;
}