#include <stdio.h>

#include "LogReader.h"

#define offset_increase(p, o, pp, ps, os, pps) if(pp==BUF){\
	if(o==bufOffset-1){ \
		p=ptrInFile;\
		pp=BLOCK;\
		o=0;\
	}else{\
		p++;\
		o++;\
	}\
}else{\
	if(o==blockSize-1){\
		p=newPtr();\
		if(!ptrInFile) return false;\
		o=0;\
		if(pps==BLOCK){\
			os-=startOffset;\
			ps=buf+os;\
			pps=BUF;\
		}\
		startOffset=0;\
	}else{\
		p++;\
		o++;\
	}\
}

#define remap_sequence(p, o, pp, ps, os, pps) ps=p;os=o;pps=pp;

CLogReader::CLogReader(void)
{
	this->filter = NULL;
	this->buf = NULL;
	this->lineNumber = 0;
	this->ptrInFile = NULL;
	this->ppTame = BLOCK;
	this->ppTameSequence = NONE;
}

CLogReader::~CLogReader(void)
{
	Close();
}

void CLogReader::PrintLastError() {
	LPTSTR errMsg;
	DWORD errMsgLen, errNum;
	errNum = GetLastError();
	errMsgLen = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
	FORMAT_MESSAGE_FROM_SYSTEM, NULL, errNum, 
	MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&errMsg, 0, NULL);
	printf(errMsg);
}

void CLogReader::Close() {
	if (ptrInFile) {
		UnmapViewOfFile(ptrInFile);
		ptrInFile = NULL;
	}
}
bool CLogReader::Open(const char *filename) {
	__try {
	
		this->filename = filename;
	
		GetSystemInfo(&systemInfo);
		
		statex.dwLength = sizeof(statex);
		GlobalMemoryStatusEx (&statex);

		fd = CreateFile((LPCSTR)filename, GENERIC_READ, /*shared mode*/0, /*security*/NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, /*template*/NULL);

		if (fd == INVALID_HANDLE_VALUE) {
			PrintLastError();
			return false;
		}
		/* Get the input file size. */
		GetFileSizeEx(fd, &fileSize);
		map = CreateFileMapping(fd, NULL, PAGE_READONLY, 0, 0, NULL);

		if (!map) {
			PrintLastError();
			return false;
		}

		int N = int(0.1 * statex.ullAvailPhys/systemInfo.dwAllocationGranularity);	
		if (N <= 0) { //for some reason
			printf("WARNING: Possibly low memory?\n");
			N = 1;
		}
		dwAllocationGranularity = N * systemInfo.dwAllocationGranularity;	
		fileOffset.QuadPart = 0;
		blockSize = fileSize.QuadPart - fileOffset.QuadPart > dwAllocationGranularity ?
					dwAllocationGranularity : fileSize.QuadPart - fileOffset.QuadPart;
		ptrInFile = (char*)MapViewOfFile(map, FILE_MAP_READ, fileOffset.HighPart, fileOffset.LowPart, blockSize);
		offsetSequence = offset = 0;

		if(!ptrInFile) {
			PrintLastError();
			return false;
		}

		return true;
	}
	__except(GetExceptionCode(), GetExceptionInformation(), EXCEPTION_EXECUTE_HANDLER) {
		printf("ERROR: Cannot open file.\n");
		return false;
	}
}

bool CLogReader::GetNextLine(char *buf, const int bufsize) {	
	if (!ptrInFile) {
		printf("ERROR: No file mapping.\n");
		return false;
	}

	if(!filter) {
		printf("ERROR: No filter.\n");
		return false;
	}

	//I do not control buf memory allocation, so I _have_ to check it:
	if (!buf) {
		printf("ERROR: buf error (NULL).\n");
		return false;
	}
	__try {
		buf[bufsize-1]=0;
	}
	__except(GetExceptionCode(), GetExceptionInformation(), EXCEPTION_EXECUTE_HANDLER) {
		printf("ERROR: buf error (access denied or so).\n");
		return false;
	}

	this->buf = buf;
	this->bufsize = bufsize;
	
	//And there is some potentially dangerous file remapping inside, so...
	__try
	{
		do {		
			while(*(ptrInFile + offset)=='\r' || *(ptrInFile + offset)=='\n') {			
				if(offset == blockSize-1) {
					if(!newPtr()) {
						return false;
					}				
					offset=0;
				} else {
					offset++;
				}
			}		
			bufOffset = 0;
			pTame = ptrInFile + offset;
			bool result = FastWildCompare(filter, blockSize);
			
			lineNumber++;

			if (ppTame == BUF || offset < offsetSequence && ppTameSequence == BLOCK) {
				offset = offsetSequence;
			}
			
			if (ptrInFile) {
				if (result) {
					if (bufOffset < bufsize && bufOffset + offset - startOffset <= bufsize-1 && offset > startOffset) {
						memcpy(buf + bufOffset, ptrInFile + startOffset, offset - startOffset);
						bufOffset += offset - startOffset;
					} 
				}
				startOffset = offset;
				while(ptrInFile && *(ptrInFile + offset)!='\r' && *(ptrInFile + offset)!='\n') {
					offset++;
					if(offset == blockSize-1) {
						if (ptrInFile && bufOffset < bufsize && bufOffset + offset - startOffset <= bufsize-1 && offset > startOffset) {
							memcpy(buf + bufOffset, ptrInFile + startOffset, offset - startOffset);		
							bufOffset += offset - startOffset;
						}
						newPtr(false);
						offset=0;
						startOffset=0;
					}
				}
				if (result) {
					if (bufOffset < bufsize && bufOffset + offset - startOffset <= bufsize-1 && offset > startOffset) {
						memcpy(buf + bufOffset, ptrInFile + startOffset, offset - startOffset);					
					}
					if (bufOffset + offset - startOffset < bufsize) {
						*(buf + bufOffset + offset - startOffset)='\n'; //to simplify printf
					}
					return true;
				}
			}

		} while (ptrInFile);
		return false;
	}
	__except (GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		printf("ERROR: Fatal Error accessing mapped file.\n");
		return false;
	}	
}

bool CLogReader::SetFilter(const char *filter) {
	if(!filter || strlen(filter)<=0) {
		return false;
	}
	/*for (int i = 0; i < strlen(filter); i++) {
		if (filter[i] == '\n' || filter[i] == '\r') {
			return false; //or will not work as expected
		}
	}*/
	this->filter = filter;
	return true;	
}

char *CLogReader::newPtr() {
	return newPtr(true);
}
char *CLogReader::newPtr(bool needCopy) {
	
	
	if (needCopy 
		&& buf 
		&& ptrInFile) {
		if(bufOffset < bufsize 
		&& blockSize > 0 
		&& bufOffset + blockSize - startOffset <= bufsize-1 
		&& blockSize > startOffset) {
			memcpy(buf + bufOffset, ptrInFile + startOffset, blockSize - startOffset);
			bufOffset += blockSize - startOffset;
		} else {
			printf("ERROR: Not enough buffer for switching pages, try to increase buffer size (\"-b <bufsize>\")\n");
			UnmapViewOfFile(ptrInFile);
			ptrInFile = NULL;
			return ptrInFile;
		}
	}

	fileOffset.QuadPart += blockSize;

	blockSize = fileSize.QuadPart - fileOffset.QuadPart > dwAllocationGranularity ?
				dwAllocationGranularity : fileSize.QuadPart - fileOffset.QuadPart;

	UnmapViewOfFile(ptrInFile);
	
	
	ptrInFile = NULL;
	if (blockSize>0) {
		ptrInFile = (char*)MapViewOfFile(map, FILE_MAP_READ, fileOffset.HighPart, fileOffset.LowPart, blockSize);	
	}

	return ptrInFile;
}

int CLogReader::LineNumber() {
	return lineNumber;
}

bool CLogReader::FastWildCompare(const char *pWild, SIZE_T maxOffset)
{
	const char *pWildStart = pWild;
    pWildSequence = NULL;
    pTameSequence = NULL;	
	ppTameSequence = NONE;
	ppTame = BLOCK;
	startOffset = offset;
	bufOffset = 0;
    // Find a first wildcard, if one exists, and the beginning of any  
    // prospectively matching sequence after it.
    do {
        // Check for the end from the start.  Get out fast, if possible.
        //if (!*pTame)
		if(!pTame || *pTame == '\r' || *pTame == '\n') {
            if (*pWild) {
                while (*(pWild++) == '*') {
                    if (!(*pWild))
                    {
                        return true;   // "ab" matches "ab*".
                    }
                }
                return false;          // "abcd" doesn't match "abc".
            }
            else {
                return true;           // "abc" matches "abc".
            }
        }
        else if (*pWild == '*') {
            // Got wild: set up for the second loop and skip on down there.
            while (*(++pWild) == '*') {
                continue;
            }
 
            if (!*pWild) {
                return true;           // "abc*" matches "abcd".
            }
            // Search for the next prospective match.
            if (*pWild != '?') {
                while (*pWild != *pTame) {
					offset_increase(pTame, offset, ppTame, pTameSequence, offsetSequence, ppTameSequence);					
                    //if (!*(++pTame))
					if(!pTame || *pTame == '\r' || *pTame == '\n') {
                        return false;  // "a*bc" doesn't match "ab".
                    }
                }
            }
            // Keep fallback positions for retry in case of incomplete match.
            pWildSequence = pWild;
            remap_sequence(pTame, offset, ppTame, pTameSequence, offsetSequence, ppTameSequence);
            break;
        }
        else if (*pWild != *pTame && *pWild != '?') {
            return false;              // "abc" doesn't match "abd".
        }
        ++pWild;                       // Everything's a match, so far.
        offset_increase(pTame, offset, ppTame, pTameSequence, offsetSequence, ppTameSequence);
    } while (true);
 
    // Find any further wildcards and any further matching sequences.
    do
    {
        if (*pWild == '*') {
            // Got wild again.
            while (*(++pWild) == '*') {
                continue;
            }
            if (!*pWild) {
                return true;           // "ab*c*" matches "abcd".
            }
            //if (!*pTame)
			if(!pTame || *pTame == '\r' || *pTame == '\n') {
                return false;          // "*bcd*" doesn't match "abc".
            }
            // Search for the next prospective match.
            if (*pWild != '?') {
                while (*pWild != *pTame) {
					offset_increase(pTame, offset, ppTame, pTameSequence, offsetSequence, ppTameSequence);
                    //if (!*(++pTame))
					if(!pTame || *pTame == '\r' || *pTame == '\n') {
                        return false;  // "a*b*c" doesn't match "ab".
                    }
                }
            }
            // Keep the new fallback positions.
            pWildSequence = pWild;
            remap_sequence(pTame, offset, ppTame, pTameSequence, offsetSequence, ppTameSequence);
        }
        else if (*pWild != *pTame && *pWild != '?') {
            // The equivalent portion of the upper loop is really simple.
            //if (!*pTame)
			if(!pTame || *pTame == '\r' || *pTame == '\n') {
                return false;          // "*bcd" doesn't match "abc".
            }
            // A fine time for questions.
            while (*pWildSequence == '?') {
                ++pWildSequence;
                offset_increase(pTameSequence, offsetSequence, ppTameSequence, pTame, offset, ppTame);
            }
            pWild = pWildSequence;

            // Fall back, but never so far again.
			offset_increase(pTameSequence, offsetSequence, ppTameSequence, pTame, offset, ppTame);
            while (*pWild != *pTameSequence) {
                //if (!*pTameSequence)
				if(!pTameSequence || *pTameSequence == '\r' || *pTameSequence == '\n') {
                    return false;      // "*a*b" doesn't match "ac".
                }
				offset_increase(pTameSequence, offsetSequence, ppTameSequence, pTame, offset, ppTame);
            }
            remap_sequence(pTameSequence, offsetSequence, ppTameSequence, pTame, offset, ppTame);
        }
 
        // Another check for the end, at the end.
        //if (!*pTame)
		if(!pTame || *pTame == '\r' || *pTame == '\n') {
            if (!*pWild) {
                return true;           // "*bc" matches "abc".
            }
            else {
                return false;          // "*bc" doesn't match "abcd".
            }
        }
        ++pWild;                       // Everything's still a match.
        offset_increase(pTame, offset, ppTame, pTameSequence, offsetSequence, ppTameSequence);
    } while (true);
}