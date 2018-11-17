#pragma once

#include <Core/Memory/Linux/OLinuxMemory.hpp>

class OLKernelMappedBufferImpl : public OLGenericMappedBuffer
{
public:
    OLKernelMappedBufferImpl();

    error_t GetVAStart(size_t&) override;
    error_t GetVAEnd(size_t&) override;
    error_t GetLength(size_t&) override;

    error_t Unmap() override;

    error_t Map(dyn_list_head_p pages, pgprot_t prot);

protected:
    void InvaildateImp()  override;
    void * _map;
    size_t _length;
};

class OLUserMappedBufferImpl : public OLGenericMappedBuffer
{
public:
    OLUserMappedBufferImpl();

    error_t GetVAStart(size_t&) override;
    error_t GetVAEnd(size_t&) override;
    error_t GetLength(size_t&) override;

    error_t Unmap() override;
    error_t Map(dyn_list_head_p pages, task_k task, pgprot_t prot);

protected:
    void InvaildateImp()  override;
    size_t _map;
    size_t _length;
    mm_struct_k _mm;
};

class OLBufferDescriptionImpl : public OLBufferDescription
{
public:
    OLBufferDescriptionImpl();
    error_t Construct();

    bool    PageIsPresent(int idx) override;
    error_t PageInsert(int idx, page_k page) override;
    error_t PagePhysAddr(int idx, phys_addr_t & addr) override;
    error_t PageMap(int idx, void * & addr) override;
    void PageUnmap(void * addr) override;

    error_t MapKernel(const OUncontrollableRef<OLGenericMappedBuffer> kernel, pgprot_t prot) override;
    error_t MapUser(const OUncontrollableRef<OLGenericMappedBuffer> kernel, task_k task, pgprot_t prot) override;
protected:
    void InvaildateImp()  override;
    
    dyn_list_head_p _pages;
    OLGenericMappedBuffer * _mapped_user;
    OLGenericMappedBuffer * _mapped_kernel;
    bool _user_mapped;
};

class OLMemoryInterfaceImpl : public OLMemoryInterface
{
public:
    OLPageLocation GetPageLocation(size_t max)  override;
    

    phys_addr_t    PhysPage(page_k page) override;
    void *          MapPage(page_k page) override;
    void          UnmapPage(void * virt) override;
    
    page_k AllocatePage(OLPageLocation location) override;
    void FreePage(page_k page) override;
    
    error_t NewBuilder(const OOutlivableRef<OLBufferDescription> builder) override;
};

void InitMemmory();