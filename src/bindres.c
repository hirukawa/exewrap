#include <windows.h>
#include <stdio.h>
#include "include/startup.h"

int _main(int argc, char* argv[]) {
	if(argc < 4) {
		printf("Usage: %s <filename> <resource-name> <resource-file>\n", argv[0]);
		return -1;
	}

	char filepath[MAX_PATH] = {'\0'};
	char* filename;
	char* rscName  = argv[2];
	char* rscFileName = argv[3];

	HANDLE hRscFile;

	if((hRscFile = CreateFile(rscFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
		printf("Failed to open: %s\n", rscFileName);
		return -2;		
	}
	DWORD  rscSize = GetFileSize(hRscFile, NULL);

	char* rscData = (char*)malloc(rscSize);

	char* p = rscData;
	DWORD r = rscSize;
	DWORD readSize;
	BOOL  ret1;
	BOOL  ret2;
	int   i;
	DWORD dwRet = GetFullPathName(argv[1], MAX_PATH, filepath, &filename);
	
	while(r > 0) {
		if(ReadFile(hRscFile, p, r, &readSize, NULL) == 0) {
			printf("Failed to read: %s\n", rscFileName);
			return -3;
		}
		p += readSize;
		r -= readSize;
	}
	CloseHandle(hRscFile);
	
	HANDLE hRes;
	for(i = 0; i < 100; i++) {
		ret1 = FALSE;
		ret2 = FALSE;
		hRes = BeginUpdateResource(filepath, FALSE);
		if(hRes != NULL) {
			ret1 = UpdateResource(hRes, RT_RCDATA, rscName, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), rscData, rscSize);
			ret2 = EndUpdateResource(hRes, FALSE);
		}
		if(ret1 && ret2) {
			break;
		}
		Sleep(100);
	}
	if(ret1 == FALSE || ret2 == FALSE) {
		printf("Failed to update resource.\n");
		return -4;
	}
	CloseHandle(hRes);
	return 0;
}
