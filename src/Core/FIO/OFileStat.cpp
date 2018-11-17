/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  
#pragma once
#include <xenus_lazy.h>
#include <libtypes.hpp>
#include <libos.hpp>

#include <ITypes\IKStat.hpp>
#include "OFileStat.hpp"

#include <Utils\DateHelper.hpp>

OFileStatImp::OFileStatImp(uint64_t modified_time, uint64_t creation_time, uint64_t accessed_time, uint64_t file_length, uint64_t UNIX_mode, uint64_t user_id)
{
	_modified_time	= modified_time;
	_creation_time	= creation_time;
	_accessed_time	= accessed_time;
	_file_length	= file_length;
	_UNIX_mode		= UNIX_mode;
	_user_id		= user_id;
}

uint64_t OFileStatImp::GetModifiedTime()
{
	return _modified_time;
}

uint64_t OFileStatImp::GetCreationTime()
{
	return _creation_time;
}

uint64_t OFileStatImp::GetAccessedTime()
{
	return _accessed_time;
}

uint64_t OFileStatImp::GetFileLength()
{
	return _file_length;
}

uint64_t OFileStatImp::GetUNIXMode()
{
	return _UNIX_mode;
}

uint64_t OFileStatImp::GetUserID()
{
	return _user_id;
}

void * OFileStatImp::LinuxDevice()
{
	return nullptr;//_dev;
}

bool OFileStatImp::IsDirectory()
{
    return _UNIX_mode & DT_DIR;
}

bool OFileStatImp::IsFile()
{
    return !(OFileStatImp::IsDirectory());
}

bool OFileStatImp::IsIPC()
{
    return (_UNIX_mode & DT_SOCK) || (_UNIX_mode & DT_CHR);
}

void	OFileStatImp::InvaildateImp()
{

}

error_t CreateFileStat(const OOutlivableRef<OFileStatImp>& out, kstat_k stat)
{
	IKStat readable(stat);

	if (!stat)
		return kErrorIllegalBadArgument;

	if (!(out.PassOwnership(new OFileStatImp(TIMESPEC_PTR_TO_MS(readable.GetModifiedTime()),
										 TIMESPEC_PTR_TO_MS(readable.GetCreatedTime()),
										 TIMESPEC_PTR_TO_MS(readable.GetAccessTime()), 
										 readable.GetSize(), 
										 readable.GetMode(),
										 readable.GetUID()))))
		return kErrorOutOfMemory;

	return kStatusOkay;
}