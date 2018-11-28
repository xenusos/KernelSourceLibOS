/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once

#include <Core/Memory/Linux/OLinuxMemory.hpp>

class OLKernelMappedBufferImpl : public OLGenericMappedBuffer
{
public:
    OLKernelMappedBufferImpl();

    error_t GetVAStart(size_t&)                                           override;
    error_t GetVAEnd(size_t&)                                             override;
    error_t GetLength(size_t&)                                            override;
    error_t Unmap()                                                       override;

    error_t CreateAddress(size_t pages, size_t & out);
    error_t Remap(dyn_list_head_p pages, size_t count, pgprot_t prot);

protected:
    void InvalidateImp()                                                  override;

    void * _va;
    size_t _length;
    vm_struct_k _vm;
    bool _mapped;
};

class OLUserMappedBufferImpl : public OLGenericMappedBuffer
{
public:
    OLUserMappedBufferImpl();

    error_t GetVAStart(size_t&)                                           override;
    error_t GetVAEnd(size_t&)                                             override;
    error_t GetLength(size_t&)                                            override;
    error_t Unmap()                                                       override;
                                                                 
    error_t CreateAddress(size_t pages, task_k task, size_t & out);
    error_t Remap(dyn_list_head_p pages, size_t count, pgprot_t prot);
protected:
    void InvalidateImp()                                                  override;

    size_t _va;
    size_t _length;
    mm_struct_k _mm;
    vm_area_struct_k _area;
    bool _mapped;
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
    
    virtual error_t MapKernel(const OUncontrollableRef<OLGenericMappedBuffer> kernel, pgprot_t prot) override; 
    virtual error_t MapUser  (const OUncontrollableRef<OLGenericMappedBuffer> kernel, pgprot_t prot) override;

    // Remap
    virtual error_t UpdateKernel(pgprot_t prot)                           override;
    virtual error_t UpdateUser  (pgprot_t prot)                           override;
    virtual error_t UpdateAll   (pgprot_t prot)                           override;
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
    OLPageLocation GetPageLocation(size_t max)                            override;
                                                                          
                                                                          
    phys_addr_t    PhysPage(page_k page)                                  override;
    void *          MapPage(page_k page)                                  override;
    void          UnmapPage(void * virt)                                  override;
                                                                          
    page_k AllocatePage(OLPageLocation location)                          override;
    void       FreePage(page_k page)                                      override;
    
    pgprot_t ProtFromCache (OLCacheType cache)                            override;
    pgprot_t ProtFromAccess(size_t access)                                override;
    pgprot_t CreateProt(size_t access, OLCacheType cache)                 override;
    
    error_t NewBuilder(const OOutlivableRef<OLBufferDescription> builder) override;
};

void InitMemmory();