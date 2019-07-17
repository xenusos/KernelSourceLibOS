/*
    Purpose: creates a generic interface that can be implemented on top of syscalls, traps, or other medium to implement function stubs in usermode that interact with the kernel
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/  


typedef void(* DelegatedCall_t)(xenus_attention_syscall_ref call);

LIBLINUX_SYM error_t AddKernelSymbol(const char * name, DelegatedCall_t fn);
