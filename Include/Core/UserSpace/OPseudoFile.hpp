/*
    Purpose: creates a file which the usermode library can open 
             this is as generic as possible to allow support for NTOS and Linux
             
             linux - devfs will be used to create a socket file - crt fopen, fread/fwrite will be passed through
             win32 - IoCreateDevice, IRP_MJ_READ/WRITE, IoCreateSymLink, etc
            
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/  

class OPseudoFile : public OObject
{
public:
    typedef bool(*PseudofileUserRead_t) (OPtr<OPseudoFile> file, void * buffer,       size_t length, size_t off, size_t *bytesCopied);
    typedef bool(*PseudofileUserWrite_t)(OPtr<OPseudoFile> file, const void * buffer, size_t length, size_t off, size_t *bytesRead);
    typedef bool(*PseudofileOpen_t)     (OPtr<OPseudoFile> file);
    typedef void(*PseudofileRelease_t)  (OPtr<OPseudoFile> file);

    virtual error_t GetIdentifierBlob(const void **, size_t &) = 0; // recommended
    virtual error_t GetPath(const char **)                     = 0; // unsafe
    
    virtual error_t FileOkay(bool &)                           = 0; 

    virtual void OnOpen     (PseudofileOpen_t)                 = 0;
    virtual void OnRelease  (PseudofileRelease_t)              = 0;
    virtual void OnUserRead (PseudofileUserRead_t)             = 0;
    virtual void OnUserWrite(PseudofileUserWrite_t)            = 0;

    virtual error_t Delete() = 0;
};


LIBLINUX_SYM error_t CreateTempKernFile(const OOutlivableRef<OPseudoFile> & out);