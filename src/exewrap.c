#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <tchar.h>
#include <locale.h>
#include <jni.h>

#include "include/jvm.h"
#include "include/icon.h"
#include "include/message.h"

char** ParseOption(int argc, char* argv[]);
char*  GetResource(LPCTSTR name, LPCTSTR type, DWORD* size);
void   SetResource(LPCTSTR filename, LPCTSTR rscName, LPCTSTR rscType, char* rscData, DWORD rscSize);
void   CreateExeFile(LPCTSTR filename, char* data, DWORD size);
char*  GetFileData(LPCTSTR filename, DWORD* size);

void    InitResource(LPCTSTR name);
DWORD   GetResourceSize(LPCTSTR name);
jbyte*  GetResourceBuffer(LPCTSTR name);

DWORD  GetVersionRevision(LPTSTR filename);
char*  SetVersionInfo(LPCTSTR filename, char* versionNumber, DWORD previousBuild, char* fileDescription, char* copyright, char* company_name, char* product_name, char* product_version, char* original_filename, char* jarfile);
void   SetApplicationIcon(LPCTSTR filename, LPCTSTR iconfile);

DWORD GetTargetJavaRuntimeVersion(char* version);
LPTSTR GetTargetJavaRuntimeVersionString(DWORD version);

void UsePack200(LPCTSTR exefile, LPCTSTR jarfile);

char defaultVersion[] = "0.0";
char defaultProductVersion[] = "0.0";

int main(int argc, char* argv[])
{
	DWORD targetVersion;
	char* targetVersionString;
	char** opt = ParseOption(argc, argv);
	int   architecture_bits = 0;
	char  image_name[32];
	char* jarfile;
	char* exefile = NULL;
	
	char* exeBuffer;
	DWORD exeSize;
	char* jarBuffer;
	DWORD jarSize;
	char* classBuffer = NULL;
	DWORD classSize;
	
	DWORD previousRevision = 0;
	char* versionNumber;
	char* fileDescription;
	char* copyright;
	char* company_name;
	char* product_name;
	char* product_version;
	char* original_filename;
	
	BOOL enableJava = FALSE;
	BOOL disablePack200 = FALSE;
	char* newVersion;
	
	if((argc < 2) || ((opt['j'] == NULL) && (opt[0] == NULL)))
	{
		int bits = GetProcessArchitecture();

		printf("exewrap 1.0.0 for %s (%d-bit)\r\n"
			   "Native executable java application wrapper.\r\n"
			   "Copyright (C) 2005-2014 HIRUKAWA Ryo. All rights reserved.\r\n"
			   "\r\n"
			   "Usage: %s <options> <jar-file>\r\n"
			   "Options:\r\n"
			   "  -g                  \t create Window application.\r\n"
			   "  -s                  \t create Windows Service application.\r\n"
			   "  -A <architecture>   \t select exe-file architecture. (default %s)\r\n"
			   "  -t <version>        \t set target java runtime version. (default 1.5)\r\n"
			   "  -2                  \t disable Pack200.\r\n"
			   "  -L <ext-dirs>       \t set ext-dirs.\r\n"
			   "  -e <ext-flags>      \t set extended flags.\r\n"
			   "  -a <vm-args>        \t set Java VM arguments.\r\n"
			   "  -i <icon-file>      \t set application icon.\r\n"
			   "  -v <version>        \t set version number.\r\n"
			   "  -d <description>    \t set file description.\r\n"
			   "  -c <copyright>      \t set copyright.\r\n"
			   "  -C <company-name>   \t set company name.\r\n"
			   "  -p <product-name>   \t set product name.\r\n"
			   "  -V <product-version>\t set product version.\r\n"
			   "  -j <jar-file>       \t input jar-file.\r\n"
			   "  -o <exe-file>       \t output exe-file.\r\n"
			, (bits == 64 ? "x64" : "x86"), bits, argv[0], (bits == 64 ? "x64" : "x86"));

		return 0;
	}
	
	if(opt['j'] && *opt['j'] != '-' && *opt['j'] != '\0')
	{
		jarfile = opt['j'];
	}
	else
	{
		jarfile = opt[0];

		if(lstrlen(jarfile) > 4)
		{
			char buf[_MAX_PATH];

			lstrcpy(buf, jarfile + lstrlen(jarfile) - 4);
			if(lstrcmp(_strupr(buf), ".JAR"))
			{
				printf("You must specify the jar-file.\n");
				return 1;
			}
		}
	}
	
	exefile = (char*)HeapAlloc(GetProcessHeap(), 0, MAX_PATH);
	if(opt['o'] && *opt['o'] != '-' && *opt['o'] != '\0')
	{
		lstrcpy(exefile, opt['o']);
	}
	else
	{
		lstrcpy(exefile, jarfile);
		exefile[lstrlen(exefile) - 4] = 0;
		lstrcat(exefile, ".exe");
	}

	{
		char buf[_MAX_PATH];
		char* p;

		if(GetFullPathName(exefile, _MAX_PATH, buf, &p) == 0)
		{
			printf("Invalid path: %s\n", exefile);
		}
		lstrcpy(exefile, buf);
	}
	

	if(opt['A'])
	{
		if(strstr(opt['A'], "86") != NULL)
		{
			architecture_bits = 32;
		}
		else if (strstr(opt['A'], "64") != NULL)
		{
			architecture_bits = 64;
		}
	}
	if(architecture_bits == 0)
	{
		architecture_bits = GetProcessArchitecture();
	}
	printf("Architecture: %s\n", (architecture_bits == 64 ? "x64 (64-bit)" : "x86 (32-bit)"));

	if(opt['g'])
	{
		sprintf(image_name, "IMAGE_GUI_%d", architecture_bits);
	}
	else if(opt['s'])
	{
		sprintf(image_name, "IMAGE_SERVICE_%d", architecture_bits);
	}
	else
	{
		sprintf(image_name, "IMAGE_CONSOLE_%d", architecture_bits);
	}
	exeBuffer = GetResource(image_name, RT_RCDATA, &exeSize);
	
	previousRevision = GetVersionRevision(exefile);
	DeleteFile(exefile);
	
	CreateExeFile(exefile, exeBuffer, exeSize);

	if(opt['t'])
	{
		targetVersion = GetTargetJavaRuntimeVersion(opt['t']);
	}
	else
	{
		targetVersion = 0x01050000; //default target version 1.5
	}
	targetVersionString = GetTargetJavaRuntimeVersionString(targetVersion);
	printf("Target: %s (%d.%d.%d.%d)\n", targetVersionString + 4, targetVersion >> 24 & 0xFF, targetVersion >> 16 & 0xFF, targetVersion >> 8 & 0xFF, targetVersion & 0xFF);
	SetResource(exefile, "TARGET_VERSION", RT_RCDATA, targetVersionString, 4);
	
	if(opt['2'])
	{
		disablePack200 = TRUE;
	}

	if(opt['L'] && *opt['L'] != '-' && *opt['L'] != '\0')
	{
		SetResource(exefile, "EXTDIRS", RT_RCDATA, opt['L'], lstrlen(opt['L']) + 1);
	}

	enableJava = CreateJavaVM(NULL, FALSE, NULL) != NULL;
	if(targetVersion >= 0x01050000 && enableJava && !disablePack200)
	{
		GetFileData(jarfile, &jarSize);
		UsePack200(exefile, jarfile);
		classBuffer = GetResource("PACK_LOADER", RT_RCDATA, &classSize);
		SetResource(exefile, "PACK_LOADER", RT_RCDATA, classBuffer, classSize);
		printf("Pack200: enable\r\n");
	}
	if(classBuffer == NULL)
	{
		jarBuffer = GetFileData(jarfile, &jarSize);
		SetResource(exefile, "JAR", RT_RCDATA, jarBuffer, jarSize);
		HeapFree(GetProcessHeap(), 0, jarBuffer);
		classBuffer = GetResource("CLASSIC_LOADER", RT_RCDATA, &classSize);
		SetResource(exefile, "CLASSIC_LOADER", RT_RCDATA, classBuffer, classSize);
		if(!enableJava)
		{
			printf("Pack200: disable / JavaVM (%d-bit) not found.\r\n", GetProcessArchitecture());
		}
		else if(targetVersion < 0x01050000)
		{
			printf("Pack200: disable / Target version is lower than 1.5\r\n");
		}
		else
		{
			printf("Pack200: disable\r\n");
		}
	}
	
	if(opt['e'] && *opt['e'] != '-' && *opt['e'] != '\0')
	{
		SetResource(exefile, "EXTFLAGS", RT_RCDATA, opt['e'], lstrlen(opt['e']) + 1);
	}
	
	if(opt['a'] && *opt['a'] != '\0')
	{
		SetResource(exefile, "VMARGS", RT_RCDATA, opt['a'], lstrlen(opt['a']) + 1);
	}
	
	if(opt['i'] && *opt['i'] != '-' && *opt['i'] != '\0')
	{
		SetApplicationIcon(exefile, opt['i']);
	}
	
	if(opt['v'] && *opt['v'] != '-' && *opt['v'] != '\0')
	{
		versionNumber = opt['v'];
	}
	else
	{
		versionNumber = defaultVersion;
	}

	if(opt['d'] && *opt['d'] != '-' && *opt['d'] != '\0')
	{
		fileDescription = opt['d'];
		if(opt['s'])
		{
			SetResource(exefile, "SVCDESC", RT_RCDATA, opt['d'], lstrlen(opt['d']) + 1);
		}
	}
	else
	{
		fileDescription = (char*)"";
	}

	if(opt['c'] && *opt['c'] != '-' && *opt['c'] != '\0')
	{
		copyright = opt['c'];
	}
	else
	{
		copyright = (char*)"";
	}

	if(opt['C'] && *opt['C'] != '-' && *opt['C'] != '\0')
	{
		company_name = opt['C'];
	}
	else
	{
		company_name = (char*)"";
	}

	if(opt['p'] && *opt['p'] != '-' && *opt['p'] != '\0')
	{
		product_name = opt['p'];
	}
	else
	{
		product_name = (char*)"";
	}

	if(opt['V'] && *opt['V'] != '-' && *opt['V'] != '\0')
	{
		product_version = opt['V'];
	}
	else
	{
		product_version = defaultProductVersion;
	}

	original_filename = strrchr(exefile, '\\') + 1;
	newVersion = SetVersionInfo(exefile, versionNumber, previousRevision, fileDescription, copyright, company_name, product_name, product_version, original_filename, jarfile);
	printf("%s (%d-bit) version %s\r\n", strrchr(exefile, '\\') + 1, architecture_bits, newVersion);
	
	return 0;
}

DWORD GetVersionRevision(char* filename)
{
	/* GetFileVersionInfoSize, GetFileVersionInfo を使うと内部で LoadLibrary が使用されるらしく
	 * その後のリソース書き込みがうまくいかなくなるようです。なので、自力で EXEファイルから
	 * リビジョンナンバーを取り出すように変更しました。
	 */
	DWORD  revision = 0;
	HANDLE hFile;
	char   HEADER[] = "VS_VERSION_INFO";
	int    len;
	char*  buf;
	DWORD  size;
	unsigned int i;
	int j;
	
	buf = (char*)HeapAlloc(GetProcessHeap(), 0, 8192);
	
	if((hFile = CreateFile(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
	{
		return 0;
	}
	
	SetFilePointer(hFile, -1 * 8192, 0, FILE_END);
	ReadFile(hFile, buf, 8192, &size, NULL);
	CloseHandle(hFile);
	
	len = lstrlen(HEADER);
	for(i = 0; i < size - len; i++)
	{
		for(j = 0; j < len; j++)
		{
			if(buf[i + j * 2] != HEADER[j]) break;
		}
		if(j == lstrlen(HEADER))
		{
			revision = ((buf[i + 47] << 8) & 0xFF00) | ((buf[i + 46]     ) & 0x00FF);
		}
	}
	HeapFree(GetProcessHeap(), 0, buf);
	return revision;
}

char* SetVersionInfo(LPCTSTR filename, char* versionNumber, DWORD previousRevision, char* fileDescription, char* copyright, char* company_name, char* product_name, char* product_version, char* original_filename, char* jarfile)
{
	int   i;
	int   SIZE_VERSION            = 48;
	int   SIZE_TEXT               = 256;
	int   ADDR_COMPANY_NAME       = 0x00B8;
	int   ADDR_FILE_DESCRIPTION   = 0x01E4;
	int   ADDR_FILE_VERSION       = 0x0308;
	int   ADDR_INTERNAL_NAME      = 0x0358;
	int   ADDR_COPYRIGHT          = 0x0480;
	int   ADDR_ORIGINAL_FILENAME  = 0x05AC;
	int   ADDR_PRODUCT_NAME       = 0x06D0;
	int   ADDR_PRODUCT_VERSION    = 0x07F8;
	char* internalName;
	char  file_version[48];
	char  buffer[260];
	char* tmp;
	char* versioninfoBuffer = NULL;
	DWORD versioninfoSize;
	short file_version_major;
	short file_version_minor;
	short file_version_build;
	short file_version_revision;
	short product_version_major;
	short product_version_minor;
	short product_version_build;
	short product_version_revision;
	char* newVersion;

	if(strrchr(jarfile, '\\') != NULL)
	{
		internalName = strrchr(jarfile, '\\') + 1;
	}
	else
	{
		internalName = jarfile;
	}
	
	lstrcpy(buffer, versionNumber);
	lstrcat(buffer, ".0.0.0.0");
	file_version_major    = atoi(strtok(buffer, "."));
	file_version_minor    = atoi(strtok(NULL, "."));
	file_version_build    = atoi(strtok(NULL, "."));
	file_version_revision = atoi(strtok(NULL, "."));
	
	lstrcpy(buffer, product_version);
	lstrcat(buffer, ".0.0.0.0");
	product_version_major    = atoi(strtok(buffer, "."));
	product_version_minor    = atoi(strtok(NULL, "."));
	product_version_build    = atoi(strtok(NULL, "."));
	product_version_revision = atoi(strtok(NULL, "."));
	
	// revison が明示的に指定されていなかった場合、既存ファイルから取得した値に 1　を加算して revision とする。
	lstrcpy(buffer, versionNumber);
	if(strtok(buffer, ".") != NULL)
	{
		if(strtok(NULL, ".") != NULL)
		{
			if(strtok(NULL, ".") != NULL)
			{
				if(strtok(NULL, ".") != NULL)
				{
					previousRevision = file_version_revision - 1;
				}
			}
		}
	}
	
	file_version_revision = (short)previousRevision + 1;
	// build 加算判定ここまで。
	sprintf(file_version, "%d.%d.%d.%d", file_version_major, file_version_minor, file_version_build, file_version_revision);
	
	tmp = GetResource("VERSION_INFO", RT_RCDATA, &versioninfoSize);
	versioninfoBuffer = (char*)HeapAlloc(GetProcessHeap(), 0, versioninfoSize);
	CopyMemory(versioninfoBuffer, tmp, versioninfoSize);

	//FILEVERSION
	versioninfoBuffer[48] = file_version_minor & 0xFF;
	versioninfoBuffer[49] = (file_version_minor >> 8) & 0xFF;
	versioninfoBuffer[50] = file_version_major & 0xFF;
	versioninfoBuffer[51] = (file_version_major >> 8) & 0xFF;
	versioninfoBuffer[52] = file_version_revision & 0xFF;
	versioninfoBuffer[53] = (file_version_revision >> 8) & 0xFF;
	versioninfoBuffer[54] = file_version_build & 0xFF;
	versioninfoBuffer[55] = (file_version_build >> 8) & 0xFF;
	//PRODUCTVERSION
	versioninfoBuffer[56] = product_version_minor & 0xFF;
	versioninfoBuffer[57] = (product_version_minor >> 8) & 0xFF;
	versioninfoBuffer[58] = product_version_major & 0xFF;
	versioninfoBuffer[59] = (product_version_major >> 8) & 0xFF;
	versioninfoBuffer[60] = product_version_revision & 0xFF;
	versioninfoBuffer[61] = (product_version_revision >> 8) & 0xFF;
	versioninfoBuffer[62] = product_version_build & 0xFF;
	versioninfoBuffer[63] = (product_version_build >> 8) & 0xFF;
	
	SecureZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, company_name, lstrlen(company_name), (WCHAR*)buffer, 128);
	for(i = 0; i < SIZE_TEXT; i++)
	{
		versioninfoBuffer[ADDR_COMPANY_NAME + i] = buffer[i];
	}
	
	SecureZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, fileDescription, lstrlen(fileDescription), (WCHAR*)buffer, 128);
	for(i = 0; i < SIZE_TEXT; i++)
	{
		versioninfoBuffer[ADDR_FILE_DESCRIPTION + i] = buffer[i];
	}
	
	SecureZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, file_version, lstrlen(file_version), (WCHAR*)buffer, 128);
	for(i = 0; i < SIZE_VERSION; i++)
	{
		versioninfoBuffer[ADDR_FILE_VERSION + i] = buffer[i];
	}
	
	SecureZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, internalName, lstrlen(internalName), (WCHAR*)buffer, 128);
	for(i = 0; i < SIZE_TEXT; i++)
	{
		versioninfoBuffer[ADDR_INTERNAL_NAME + i] = buffer[i];
	}
	
	SecureZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, copyright, lstrlen(copyright), (WCHAR*)buffer, 128);
	for(i = 0; i < SIZE_TEXT; i++)
	{
		versioninfoBuffer[ADDR_COPYRIGHT + i] = buffer[i];
	}
	
	SecureZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, original_filename, lstrlen(original_filename), (WCHAR*)buffer, 128);
	for(i = 0; i < SIZE_TEXT; i++)
	{
		versioninfoBuffer[ADDR_ORIGINAL_FILENAME + i] = buffer[i];
	}
	
	SecureZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, product_name, lstrlen(product_name), (WCHAR*)buffer, 128);
	for(i = 0; i < SIZE_TEXT; i++)
	{
		versioninfoBuffer[ADDR_PRODUCT_NAME + i] = buffer[i];
	}
	
	SecureZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, product_version, lstrlen(product_version), (WCHAR*)buffer, 128);
	for(i = 0; i < SIZE_VERSION; i++)
	{
		versioninfoBuffer[ADDR_PRODUCT_VERSION + i] = buffer[i];
	}
	
	SetResource(filename, (LPCTSTR)VS_VERSION_INFO, RT_VERSION, versioninfoBuffer, versioninfoSize);
	
	newVersion = (char*)HeapAlloc(GetProcessHeap(), 0, 128);
	sprintf(newVersion, "%d.%d.%d.%d", file_version_major, file_version_minor, file_version_build, file_version_revision);

	HeapFree(GetProcessHeap(), 0, versioninfoBuffer);

	return newVersion;
}

char** ParseOption(int argc, char* argv[])
{
	char** opt = (char**)HeapAlloc(GetProcessHeap(), 0, 256 * 8);
	
	SecureZeroMemory(opt, 256 * 8);

	if((argc > 1) && (*argv[1] != '-'))
	{
		opt[0] = argv[1];
	}
	while(*++argv)
	{
		if(*argv[0] == '-')
		{
			if(argv[1] == NULL)
			{
				opt[*(argv[0] + 1)] = (char*)"";
			}
			else
			{
				opt[*(argv[0] + 1)] = argv[1];
			}
		}
	}
	argv--;
	if((opt[0] == NULL) && (*argv[0] != '-'))
	{
		opt[0] = argv[0];
	}

	return opt;
}

char* GetResource(LPCTSTR name, LPCTSTR type, DWORD* size)
{
	HRSRC hrsrc;

	if((hrsrc = FindResource(NULL, name, type)) == NULL)
	{
		printf("Fail to FindResource: %s\n", name);
		ExitProcess(0);
	}
	*size = SizeofResource(NULL, hrsrc);
	return (char*)LockResource(LoadResource(NULL, hrsrc));
}

void SetResource(LPCTSTR filename, LPCTSTR rscName, LPCTSTR rscType, char* rscData, DWORD rscSize)
{
	HANDLE hRes;
	BOOL   ret1;
	BOOL   ret2;
	int i;

	for (i = 0; i < 100; i++)
	{
		ret1 = FALSE;
		ret2 = FALSE;
		hRes = BeginUpdateResource(filename, FALSE);
		if (hRes != NULL)
		{
			ret1 = UpdateResource(hRes, rscType, rscName, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), rscData, rscSize);
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
		printf("Failed to update resource: %s: %s\n", filename, rscName);
		ExitProcess(0);
	}
	CloseHandle(hRes);
}

void CreateExeFile(LPCTSTR filename, char* data, DWORD size)
{
	HANDLE hFile;
	DWORD writeSize;

	if((hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
	{
		if((hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
		{
			printf("Failed to create file: %s\n", filename);
			ExitProcess(0);
		}
	}

	while(size > 0)
	{
		if(WriteFile(hFile, data, size, &writeSize, NULL) == 0)
		{
			printf("Failed to write: %s\n", filename);
			ExitProcess(0);
		}
		data += writeSize;
		size -= writeSize;
	}
	CloseHandle(hFile);
}

char* GetFileData(LPCTSTR filename, DWORD* size)
{
	HANDLE hFile;
	char* buf;
	char* p;
	DWORD r;
	DWORD readSize;

	if((hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
	{
		printf("Failed to read file: %s\n", filename);
		ExitProcess(0);
	}
	*size = GetFileSize(hFile, NULL);
	p = buf = (char*)HeapAlloc(GetProcessHeap(), 0, *size);

	r = *size;
	while(r > 0)
	{
		if(ReadFile(hFile, p, r, &readSize, NULL) == 0)
		{
			printf("Failed to read: %s\n", filename);
			HeapFree(GetProcessHeap(), 0, buf);
			ExitProcess(0);
		}
		p += readSize;
		r -= readSize;
	}
	CloseHandle(hFile);

	return buf;
}

void SetApplicationIcon(LPCTSTR filename, LPCTSTR iconfile)
{
	void *pvFile;
	DWORD nSize;
	int f, z;
	ICONDIR id, *pid;
	GRPICONDIR *pgid;
	HANDLE hResource;
	BOOL ret1;
	BOOL ret2;
	BOOL ret3;
	int i;
	
	//Delete default icon.
	for(z = 0; z < 99; z++)
	{
		hResource = BeginUpdateResource(filename, FALSE);
		ret1 = UpdateResource(hResource, RT_ICON, MAKEINTRESOURCE(z + 1), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), NULL, 0);
		EndUpdateResource(hResource, FALSE);
		if(ret1 == FALSE)
		{
			break;
		}
	}

	//Delete default icon group.
	{
		hResource = BeginUpdateResource(filename, FALSE);
		UpdateResource(hResource, RT_GROUP_ICON, MAKEINTRESOURCE(1), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), NULL, 0);
		EndUpdateResource(hResource, FALSE);
	}

	if(!lstrlen(iconfile))
	{
		return;
	}
	if((f = _lopen(iconfile, OF_READ)) == -1)
	{
		return;
	}
	
	for(i = 0; i < 100; i++)
	{
		ret1 = FALSE;
		ret2 = FALSE;
		ret3 = FALSE;
		hResource = BeginUpdateResource(filename, FALSE);
		
		_lread(f, &id, sizeof(id));
		_llseek(f, 0, SEEK_SET);
		pid = (ICONDIR *)HeapAlloc(GetProcessHeap(), 0, sizeof(ICONDIR) + sizeof(ICONDIRENTRY) * (id.idCount - 1));
		pgid = (GRPICONDIR *)HeapAlloc(GetProcessHeap(), 0, sizeof(GRPICONDIR) + sizeof(GRPICONDIRENTRY) * (id.idCount - 1));
		_lread(f, pid, sizeof(ICONDIR) + sizeof(ICONDIRENTRY) * (id.idCount - 1));
		memcpy(pgid, pid, sizeof(GRPICONDIR));
		
		for(z = 0; z < id.idCount; z++)
		{
			pgid->idEntries[z].common = pid->idEntries[z].common;
			pgid->idEntries[z].nID = z + 1;
			nSize = pid->idEntries[z].common.dwBytesInRes;
			pvFile = HeapAlloc(GetProcessHeap(), 0, nSize);
			if(!pvFile)
			{
				_lclose(f);
				return;
			}
			_llseek(f, pid->idEntries[z].dwImageOffset, SEEK_SET);
			_lread(f, pvFile, nSize);
			ret1 = UpdateResource(hResource, RT_ICON, MAKEINTRESOURCE(z + 1), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), pvFile, nSize);
			HeapFree(GetProcessHeap(), 0, pvFile);
		}
		nSize = sizeof(GRPICONDIR) + sizeof(GRPICONDIRENTRY) * (id.idCount - 1);
		ret2 = UpdateResource(hResource, RT_GROUP_ICON, MAKEINTRESOURCE(1), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), pgid, nSize);
		_lclose(f);
		
		ret3 = EndUpdateResource(hResource, FALSE);
		if(ret1 && ret2 && ret3)
		{
			break;
		}
		Sleep(100);
	}
	if(ret1 == FALSE || ret2 == FALSE || ret3 == FALSE)
	{
		printf("Failed to set icon\n");
		return;
	}
	CloseHandle(hResource);
}

DWORD GetTargetJavaRuntimeVersion(char* version)
{
	char* p = version;
	DWORD major = 0;
	DWORD minor = 0;
	DWORD build = 0;
	DWORD revision = 0;
	
	if(p != NULL)
	{
		major = atoi(p);
		if((p = strstr(p, ".")) != NULL)
		{
			minor = atoi(++p);
			if((p = strstr(p, ".")) != NULL)
			{
				build = atoi(++p);
				if((p = strstr(p, ".")) != NULL)
				{
					revision = atoi(++p);
				}
			}
		}
		return major << 24 & 0xFF000000 | minor << 16 & 0x00FF0000 | build << 8 & 0x0000FF00 | revision & 0x000000FF;
	}
	return 0x00000000;
}

LPTSTR GetTargetJavaRuntimeVersionString(DWORD version)
{
	DWORD major = version >> 24 & 0xFF;
	DWORD minor = version >> 16 & 0xFF;
	DWORD build = version >> 8 & 0xFF;
	DWORD revision = version & 0xFF;
	LPTSTR buf = (LPTSTR)HeapAlloc(GetProcessHeap(), 0, 64);

	*(DWORD*)buf = version;

	//1.5, 1.6
	if(major == 1 && (minor == 5 || minor == 6))
	{
		if(revision == 0)
		{
			sprintf(buf + 4, "Java %d.%d", minor, build);
		}
		else
		{
			sprintf(buf + 4, "Java %d.%d.%d", minor, build, revision);
		}
		return buf;
	}

	//1.2, 1.3, 1.4
	if(major == 1 && (minor == 2 || minor == 3 || minor == 4))
	{
		if(revision == 0)
		{
			if(build == 0)
			{
				sprintf(buf + 4, "Java2 %d.%d", major, minor);
			}
			else
			{
				sprintf(buf + 4, "Java2 %d.%d.%d", major, minor, build);
			}
		}
		else
		{
			sprintf(buf + 4, "Java2 %d.%d.%d.%d", major, minor, build, revision);
		}
		return buf;
	}

	//1.0, 1.1
	if(major == 1 && (minor == 0 || minor == 1))
	{
		if(revision == 0)
		{
			if(build == 0)
			{
				sprintf(buf + 4, "Java %d.%d", major, minor);
			}
			else
			{
				sprintf(buf + 4, "Java %d.%d.%d", major, minor, build);
			}
		}
		else
		{
			sprintf(buf + 4, "Java %d.%d.%d.%d", major, minor, build, revision);
		}
		return buf;
	}

	//other
	if(revision == 0)
	{
		sprintf(buf + 4, "Java %d.%d.%d", major, minor, build);
	}
	else
	{
		sprintf(buf + 4, "Java %d.%d.%d.%d", major, minor, build, revision);
	}
	return buf;
}

void UsePack200(LPCTSTR exefile, LPCTSTR jarfile)
{
	DWORD size;
	char* buf;
	jclass optimizerClass;
	jmethodID optimizerInit;
	jobject optimizer;
	jmethodID getRelativeClassPath;
	jmethodID getClassesPackGz;
	jmethodID getResourcesGz;
	jmethodID getSplashPath;
	jmethodID getSplashImage;
	jbyteArray relativeClassPath;
	jbyteArray classesPackGz;
	jbyteArray resourcesGz;
	jbyteArray splashPath;
	jbyteArray splashImage;

	buf = GetResource("JAR_OPTIMIZER", RT_RCDATA, &size);

	optimizerClass = (*env)->DefineClass(env, "JarOptimizer", NULL, (jbyte*)buf, size);
	if(optimizerClass == NULL)
	{
		return;
	}
	optimizerInit = (*env)->GetMethodID(env, optimizerClass, "<init>", "(Ljava/lang/String;)V");
	if(optimizerInit == NULL)
	{
		return;
	}

	optimizer = (*env)->NewObject(env, optimizerClass, optimizerInit, GetJString(env, jarfile));
	if(optimizer == NULL)
	{
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
		return;
	}
	getRelativeClassPath = (*env)->GetMethodID(env, optimizerClass, "getRelativeClassPath", "()[B");
	if(getRelativeClassPath == NULL)
	{
		return;
	}
	getClassesPackGz = (*env)->GetMethodID(env, optimizerClass, "getClassesPackGz", "()[B");
	if(getClassesPackGz == NULL)
	{
		return;
	}
	getResourcesGz = (*env)->GetMethodID(env, optimizerClass, "getResourcesGz", "()[B");
	if(getResourcesGz == NULL)
	{
		return;
	}
	getSplashPath = (*env)->GetMethodID(env, optimizerClass, "getSplashPath", "()[B");
	if(getSplashPath == NULL)
	{
		return;
	}
	getSplashImage = (*env)->GetMethodID(env, optimizerClass, "getSplashImage", "()[B");
	if(getSplashImage == NULL)
	{
		return;
	}
	relativeClassPath = (jbyteArray)((*env)->CallObjectMethod(env, optimizer, getRelativeClassPath));
	if(relativeClassPath != NULL)
	{
		size = (*env)->GetArrayLength(env, relativeClassPath);
		buf = (char*)((*env)->GetByteArrayElements(env, relativeClassPath, NULL));
		SetResource(exefile, "RELATIVE_CLASSPATH", RT_RCDATA, buf, size);
	}
	classesPackGz = (jbyteArray)((*env)->CallObjectMethod(env, optimizer, getClassesPackGz));
	if(classesPackGz != NULL)
	{
		size = (*env)->GetArrayLength(env, classesPackGz);
		buf = (char*)((*env)->GetByteArrayElements(env, classesPackGz, NULL));
		SetResource(exefile, "CLASSES_PACK_GZ", RT_RCDATA, buf, size);
	}
	resourcesGz = (jbyteArray)((*env)->CallObjectMethod(env, optimizer, getResourcesGz));
	if(resourcesGz != NULL)
	{
		size = (*env)->GetArrayLength(env, resourcesGz);
		buf = (char*)((*env)->GetByteArrayElements(env, resourcesGz, NULL));
		SetResource(exefile, "RESOURCES_GZ", RT_RCDATA, buf, size);
	}
	splashPath = (jbyteArray)((*env)->CallObjectMethod(env, optimizer, getSplashPath));
	if(splashPath != NULL)
	{
		size = (*env)->GetArrayLength(env, splashPath);
		buf = (char*)((*env)->GetByteArrayElements(env, splashPath, NULL));
		SetResource(exefile, "SPLASH_PATH", RT_RCDATA, buf, size);
	}
	splashImage = (jbyteArray)((*env)->CallObjectMethod(env, optimizer, getSplashImage));
	if(splashImage != NULL)
	{
		size = (*env)->GetArrayLength(env, splashImage);
		buf = (char*)((*env)->GetByteArrayElements(env, splashImage, NULL));
		SetResource(exefile, "SPLASH_IMAGE", RT_RCDATA, buf, size);
	}
}
