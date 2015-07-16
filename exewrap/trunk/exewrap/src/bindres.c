/*

E:\temp\20140731\src>cl /O1 /Ob0 /Os bindres.c /link /merge:.rdata=.text /NOENTR
Y /ENTRY:main /NODEFAULTLIB kernel32.lib

*/


#include <windows.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
	int    is_reverse = 0;
	char*  rscName;
	char*  rscFileName;
	DWORD  rscSize;
	BYTE*  rscData;
	char*  filepath;
	char*  filename;
	HANDLE hRscFile;
	char*  p;
	DWORD  r;
	DWORD  readSize;
	BOOL   ret1;
	BOOL   ret2;
	DWORD  i;
	HANDLE hRes;
	

	if(argc < 4 || (strcmp(argv[1], "-r") == 0 && argc < 5))
	{
		printf("Usage: %s [-r] <filename> <resource-name> <resource-file>\n", argv[0]);
		return -1;
	}
	
	if(strcmp(argv[1], "-r") == 0)
	{
		is_reverse = 1;
	}

	filepath = (char*)malloc(MAX_PATH);
	GetFullPathName(argv[1 + is_reverse], MAX_PATH, filepath, &filename);

	rscName  = argv[2 + is_reverse];
	rscFileName = argv[3 + is_reverse];

	hRscFile = CreateFile(rscFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hRscFile == INVALID_HANDLE_VALUE)
	{
		printf("Failed to open: %s\n", rscFileName);
		return -2;		
	}
	rscSize = GetFileSize(hRscFile, NULL);
	rscData = (BYTE*)HeapAlloc(GetProcessHeap(), 0, rscSize);

	p = rscData;
	r = rscSize;
	
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
	
	if(is_reverse)
	{
		BYTE* tmp = (BYTE*)HeapAlloc(GetProcessHeap(), 0, rscSize);
		CopyMemory(tmp, rscData, rscSize);
		for (i = 0; i < rscSize; i++)
		{
			rscData[i] = ~tmp[rscSize - 1 - i];
		}
		HeapFree(GetProcessHeap(), 0, tmp);
	}
	
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

	HeapFree(GetProcessHeap(), 0, rscData);
	return 0;
}
