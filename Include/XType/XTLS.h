/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  
#pragma once

//XStaticVar - to be used in functions 
//XStatic

#define GLOBAL_PER_THREAD(name, type)
#define LOCAL_STATIC_VAR(name, type)



//ideas:

void test()
{
	ThreadLocalStack<void *> a(0x123);
	ThreadLocalIP<void *> b();
	ThreadIndexor<int> c(5 /*5 threads will access this array. any more = panic*/);

	c.
}