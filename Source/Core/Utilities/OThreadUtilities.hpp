/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <Core/Utilities/OThreadUtilities.hpp>

LIBLINUX_SYM void Utilities::Tasks::DisablePreemption();
LIBLINUX_SYM void Utilities::Tasks::AllowPreempt();
LIBLINUX_SYM bool Utilities::Tasks::IsTask32Bit(task_k handle);
