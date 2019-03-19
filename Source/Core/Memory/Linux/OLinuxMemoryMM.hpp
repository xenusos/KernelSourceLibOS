/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once

// Do note: these are not apart of the public API
// If a module *really* needs low level linux stuff, it can share our routines

// Do note: these functions are identical to the linux counterparts
//          although, some of these are ported to MSVC, most just pass through to bootstrap

typedef size_t kernel_pointer_t;

LIBLINUX_SYM page_k           virt_to_page(kernel_pointer_t address);
LIBLINUX_SYM phys_addr_t      virt_to_phys(kernel_pointer_t address);
LIBLINUX_SYM pfn_t            virt_to_pfn(kernel_pointer_t address);
                              
LIBLINUX_SYM page_k           pfn_to_page(pfn_t pfn);
LIBLINUX_SYM pfn_t            page_to_pfn(page_k page);
                              
LIBLINUX_SYM phys_addr_t      pfn_to_phys(pfn_t pfn);
LIBLINUX_SYM pfn_t            phys_to_pfn(phys_addr_t address);    // NOTE: UP_PAGE COULD BE ILLEGAL (EXCL CONFIG_FLATMEM - WE USE SPARSE MEM!!!!)
                                                                   // TODO: investigate
LIBLINUX_SYM kernel_pointer_t phys_to_virt(phys_addr_t addr);



extern page_k * AllocateLinuxPages(OLPageLocation location, size_t cnt, bool user, bool contig, size_t uflags = 0);
extern void     FreeLinuxPages(page_k * pages);

extern void InitMMIOHelper();
