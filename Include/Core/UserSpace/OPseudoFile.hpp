/*
    Purpose: creates a file which the usermode library can open 
	         this is as generic as possible to allow support for NTOS and Linux
			 
			 linux - devfs will be used to create a socket file - raw read/write stubs will be used in the usermode module
			 win32 - IoCreateDevice, IOCTL_???_METHOD_??, etc or IRP_MJ_READ/WRITE
			
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  



class OPseudoFile : public OObject
{
public:
    typedef bool(*PseudofileUserRead_t)(OPtr<OPseudoFile> file, void * buffer, size_t length, size_t off, size_t *bytesCopied);
    typedef bool(*PseudofileUserWrite_t)(OPtr<OPseudoFile> file, const void * buffer, size_t length, size_t off, size_t *bytesRead);
    typedef bool(*PseudofileOpen_t)(OPtr<OPseudoFile> file);
    typedef void(*PseudofileRelease_t)(OPtr<OPseudoFile> file);

	virtual error_t GetIdentifierBlob(const void **, size_t &) = 0; //to be used in LibIRC
	virtual error_t GetPath(const char **) = 0; // may return a nice looking path, may not, or may return null; only use the file apis provided by LibIRC given the ident blob
	
	virtual error_t FileOkay(bool &) = 0; 

    virtual void OnOpen(PseudofileOpen_t)             = 0;
    virtual void OnRelease(PseudofileRelease_t)       = 0;
    virtual void OnUserRead(PseudofileUserRead_t)     = 0;
    virtual void OnUserWrite(PseudofileUserWrite_t)   = 0;

	virtual error_t Delete() = 0;
};


LIBLINUX_SYM error_t CreateTempKernFile(const OOutlivableRef<OPseudoFile> & out);