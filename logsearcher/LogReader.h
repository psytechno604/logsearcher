#pragma once
#include <windows.h>


enum POINTER_POSITION {
	NONE, BLOCK, BUF
};
class CLogReader
{
	const char *filename;
	const char *filter;
	HANDLE fd, map;
	SYSTEM_INFO systemInfo;
	MEMORYSTATUSEX statex;
	LARGE_INTEGER fileSize, fileOffset;
	SIZE_T dwAllocationGranularity, blockSize, 
		offset, offsetSequence, startOffset, bufOffset;
	char *ptrInFile, *buf;
	
	int bufsize, lineNumber;
	const char *pWildSequence;  // Points to prospective wild string match after '*'
    char *pTameSequence, *pTame;  // Points to prospective tame string match
	bool FastWildCompare(const char *pWild, SIZE_T maxOffset);
	POINTER_POSITION ppTame, ppTameSequence;
	char *newPtr();
	char *newPtr(bool needCopy);
	void PrintLastError();
public:
	CLogReader(void);
	~CLogReader(void);
	bool Open(const char *filename);
	void Close();
	bool SetFilter(const char *filter);
	bool GetNextLine(char *buf, const int bufsize);

	int LineNumber(); //beyond requested interface - but useful to output results
};
