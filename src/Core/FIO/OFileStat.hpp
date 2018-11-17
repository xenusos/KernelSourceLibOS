/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  
#pragma once
#include <Core\FIO\OFileStat.hpp>
#include <ITypes\IKStat.hpp>

class OFileStatImp : public OFileStat
{
public:
	OFileStatImp(uint64_t modified_time, uint64_t creation_time, uint64_t accessed_time, uint64_t file_length, uint64_t UNIX_mode, uint64_t user_id);
	uint64_t GetModifiedTime()	override;
	uint64_t GetCreationTime()	override;
	uint64_t GetAccessedTime()	override;
	uint64_t GetFileLength()	override;
	uint64_t GetUNIXMode()		override;
	uint64_t GetUserID()		override;
	void *   LinuxDevice()		override;

    bool IsDirectory()          override;
    bool IsFile()               override;
    bool IsIPC()                override;
protected:
	void	InvaildateImp()		override;
private:
	uint64_t _modified_time;
	uint64_t _creation_time;
	uint64_t _accessed_time;
	uint64_t _file_length;
	uint64_t _UNIX_mode;
	uint64_t _user_id;
	dev_t _dev;
};

error_t CreateFileStat(const OOutlivableRef<OFileStatImp>& out, kstat_k stat);