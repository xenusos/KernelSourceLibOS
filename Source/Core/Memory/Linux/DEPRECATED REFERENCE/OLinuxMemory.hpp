/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once

#include <Core/Memory/Linux/OLinuxMemory.hpp>

class OLKernelMappedBufferImpl : public OObject
{
public:
    OLKernelMappedBufferImpl();

    error_t GetVAStart(size_t&)                                          ;
    error_t GetVAEnd(size_t&)                                            ;
    error_t GetLength(size_t&)                                           ;
    error_t Unmap()                                                      ;



    error_t CreateAddress(size_t pages, size_t & out);
    error_t Remap(dyn_list_head_p pages, size_t count, OLPageEntryMeta prot);

protected:
    void InvalidateImp()                                                  ;

    void * _va;
    size_t _length;
    vm_struct_k _vm;
    bool _mapped;
};

class OLUserMappedBufferImpl : public OObject
{
public:
    OLUserMappedBufferImpl(task_k task);

    error_t GetVAStart(size_t&)                                           ;
    error_t GetVAEnd(size_t&)                                             ;
    error_t GetLength(size_t&)                                            ;
    error_t Unmap()                                                       ;

    void    DisableUnmapOnFree()                                          ;
                                                                 
    error_t CreateAddress(size_t pages, size_t & out);
    error_t Remap(dyn_list_head_p pages, size_t count, OLPageEntryMeta prot);
protected:
    void InvalidateImp()                                                  ;

    size_t _va;
    size_t _length;
    task_k _task;
    vm_area_struct_k _area;
    bool _mapped;
    bool _no_unmap;
};

class OLBufferDescriptionImpl : public OLBufferDescription
{
public:
    OLBufferDescriptionImpl(dyn_list_head_p page);

    bool    PageIsPresent(size_t idx)                                     override;
    error_t PageInsert(size_t idx, page_k page)                           override;
    error_t PagePhysAddr(size_t idx, phys_addr_t & addr)                  override;
    error_t PageMap(size_t idx, void * & addr)                            override;
    error_t PageCount(size_t &)                                           override;
    void    PageUnmap(void * addr)                                        override;

    // Map
    virtual error_t SetupKernelAddress(size_t & out)                      override;
    virtual error_t SetupUserAddress(task_k task, size_t & out)           override;
    
    virtual error_t MapKernel(const OUncontrollableRef<OLGenericMappedBuffer> kernel, OLPageEntryMeta prot) override; 
    virtual error_t MapUser  (const OUncontrollableRef<OLGenericMappedBuffer> kernel, OLPageEntryMeta prot) override;

    // Remap
    virtual error_t UpdateKernel(OLPageEntryMeta prot)                        override;
    virtual error_t UpdateUser  (OLPageEntryMeta prot)                        override;
    virtual error_t UpdateAll   (OLPageEntryMeta prot)                        override;
protected:
    void InvalidateImp()                                                  override;
    
    size_t _cnt;
    dyn_list_head_p _pages;
    OLUserMappedBufferImpl   * _mapped_user;
    OLKernelMappedBufferImpl * _mapped_kernel;
};

class OLMemoryInterfaceImpl : public OLMemoryInterface
{
public:
    OLPageLocation GetPageLocation(size_t max)                                  override;
                                                                          
                                                                          
    phys_addr_t    PhysPage(page_k page)                                        override;
    void *          MapPage(page_k page)                                        override;
    void          UnmapPage(void * virt)                                        override;
                                                                          
    page_k AllocatePage(OLPageLocation location, size_t flags)                  override;
    void       FreePage(page_k page)                                            override;
  
    void        UpdatePageEntryCache (OLPageEntryMeta &, OLCacheType cache)         override;
    void        UpdatePageEntryAccess(OLPageEntryMeta &, size_t access)             override;
    OLPageEntryMeta CreatePageEntry      (size_t access, OLCacheType cache)         override;
    
    error_t NewBuilder(const OOutlivableRef<OLBufferDescription> builder)       override;
};

extern void InitMemmory();
