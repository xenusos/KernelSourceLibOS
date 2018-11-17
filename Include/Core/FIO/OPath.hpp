/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  
#pragma once

class OPath : public OObject
{
public:
	/* This is too OS dependent. We'll keep this mostly empty for now. */

	virtual bool IsEqualTo(const OPath * path) = 0;
	virtual uint_t ToString(char * str, uint_t length) = 0;
};