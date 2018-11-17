#pragma once

#include <Core/Memory/Linux/OLinuxMemory.hpp>

class OLKernelMappedBufferImpl : public OLGenericMappedBuffer
{
public:
    OLKernelMappedBufferImpl();

    size_t GetVAStart() override;
    size_t GetVAEnd() override;
    size_t GetLength() override;

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

    size_t GetVAStart() override;
    size_t GetVAEnd() override;
    size_t GetLength() override;

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

    bool IsVoid() override;
    error_t HasError() override;
    bool IsHandled() override;

    error_t MapKernel(const OUncontrollableRef<OLGenericMappedBuffer> kernel, pgprot_t prot) override;
    error_t MapUser(const OUncontrollableRef<OLGenericMappedBuffer> kernel, task_k task, pgprot_t prot) override;

    void SignalUnmapped();

protected:
    void InvaildateImp()  override;
    
    dyn_list_head_p _pages;
    OLGenericMappedBuffer * _mapped;
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