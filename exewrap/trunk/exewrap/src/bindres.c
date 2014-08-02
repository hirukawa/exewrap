/*

E:\temp\20140731\src>cl /O1 /Ob0 /Os bindres.c /link /merge:.rdata=.text /NOENTR
Y /ENTRY:main /NODEFAULTLIB kernel32.lib

*/


#include <windows.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
	char*  rscName;
	char*  rscFileName;
	DWORD  rscSize;
	char*  rscData;
	char*  filepath;
	char*  filename;
	HANDLE hRscFile;
	char*  p;
	DWORD  r;
	DWORD  readSize;
	BOOL   ret1;
	BOOL   ret2;
	int    i;
	HANDLE hRes;

	if(argc < 4)
	{
		printf("Usage: %s <filename> <resource-name> <resource-file>\n", argv[0]);
		return -1;
	}

	filepath = (char*)HeapAlloc(GetProcessHeap(), 0, MAX_PATH);
	filepath[0] = '\0';

	rscName  = argv[2];
	rscFileName = argv[3];

	if((hRscFile = CreateFile(rscFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
	{
		printf("Failed to open: %s\n", rscFileName);
		return -2;		
	}
	rscSize = GetFileSize(hRscFile, NULL);
	rscData = (char*)HeapAlloc(GetProcessHeap(), 0, rscSize);

	p = rscData;
	r = rscSize;
	GetFullPathName(argv[1], MAX_PATH, filepath, &filename);
	
	while(r > 0)
	{
		if(ReadFile(hRscFile, p, r, &readSize, NULL) == 0)
		{
			printf("Failed to read: %s\n", rscFileName);
			return -3;
		}
		p += readSize;
		r -= readSize;
	}
	CloseHandle(hRscFile);
	
	for(i = 0; i < 100; i++)
	{
		ret1 = FALSE;
		ret2 = FALSE;
		hRes = BeginUpdateResource(filepath, FALSE);
		if(hRes != NULL)
		{
			ret1 = UpdateResource(hRes, RT_RCDATA, rscName, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), rscData, rscSize);
			ret2 = EndUpdateResource(hRes, FALSE);
		}
		if(ret1 && ret2)
		{
			break;
		}
		Sleep(100);
	}
	if(ret1 == FALSE || ret2 == FALSE)
	{
		printf("Failed to update resource.\n");
		return -4;
	}
	CloseHandle(hRes);
	return 0;
}
