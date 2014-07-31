#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
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

int _main(int argc, char* argv[]) {
	DWORD targetVersion;
	char* targetVersionString;
	char** opt = ParseOption(argc, argv);
	char* jarfile;
	char* exefile = NULL;
	char* temp;
	
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
	
	char* iconBuffer;
	DWORD iconSize;
	
	BOOL enableJava = FALSE;
	BOOL disablePack200 = FALSE;
	
	if((argc < 2) || ((opt['j'] == NULL) && (opt[0] == NULL))) {
		printf("exewrap 0.9.8 Native executable java application wrapper.\r\n"
			   "Copyright (C) 2005-2014 HIRUKAWA Ryo. All rights reserved.\r\n"
			   "\r\n"
			   "Usage: %s <options> <jar-file>\r\n"
			   "Options:\r\n"
			   "  -g                  \t create Window application.\r\n"
			   "  -s                  \t create Windows Service application.\r\n"
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
			, argv[0]);
		exit(0);
	}
	
	if(opt['j'] && *opt['j'] != '-' && *opt['j'] != '\0') {
		jarfile = opt['j'];
	} else {
		jarfile = opt[0];
		if(strlen(jarfile) > 4) {
			char buf[_MAX_PATH];
			char* p = buf;
			strcpy(buf, jarfile + strlen(jarfile) - 4);
			while(*p != '\0') {
				*p = tolower(*p);
				p++;
			}
			if(strcmp(buf, ".jar")) {
				fprintf(stderr, "You must specify the jar-file.");
				exit(1);
			}
		}
	}
	
	if(opt['o'] && *opt['o'] != '-' && *opt['o'] != '\0') {
		exefile = opt['o'];
	} else {
		exefile = (char*)HeapAlloc(GetProcessHeap(), 0, MAX_PATH);
		strcpy(exefile, jarfile);
		exefile[strlen(exefile) - 4] = 0;
		strcat(exefile, ".exe");
	}

	exefile = _tfullpath(NULL, exefile, _MAX_PATH);
	
	if(opt['g']) {
		exeBuffer = GetResource("IMAGE_GUI", RT_RCDATA, &exeSize);
	} else if(opt['s']) {
		exeBuffer = GetResource("IMAGE_SERVICE", RT_RCDATA, &exeSize);
	} else {
		exeBuffer = GetResource("IMAGE_CONSOLE", RT_RCDATA, &exeSize);
	}
	
	previousRevision = GetVersionRevision(exefile);
	DeleteFile(exefile);
	
	CreateExeFile(exefile, exeBuffer, exeSize);
	
	if(opt['t']) {
		targetVersion = GetTargetJavaRuntimeVersion(opt['t']);
	} else {
		targetVersion = 0x01050000; //default target version 1.5
	}
	targetVersionString = GetTargetJavaRuntimeVersionString(targetVersion);
	printf("Target: %s (%d.%d.%d.%d)\n", targetVersionString + 4, targetVersion >> 24 & 0xFF, targetVersion >> 16 & 0xFF, targetVersion >> 8 & 0xFF, targetVersion & 0xFF);
	SetResource(exefile, "TARGET_VERSION", RT_RCDATA, targetVersionString, 4 + strlen(targetVersionString + 4) + 1);
	
	if(opt['2']) {
		disablePack200 = TRUE;
	}

	if(opt['L'] && *opt['L'] != '-' && *opt['L'] != '\0') {
		SetResource(exefile, "EXTDIRS", RT_RCDATA, opt['L'], strlen(opt['L']) + 1);
	}

	enableJava = CreateJavaVM(NULL, FALSE, NULL) != NULL;
	if(targetVersion >= 0x01050000 && enableJava && !disablePack200) {
		GetFileData(jarfile, &jarSize);
		UsePack200(exefile, jarfile);
		classBuffer = GetResource("PACK_LOADER", RT_RCDATA, &classSize);
		SetResource(exefile, "PACK_LOADER", RT_RCDATA, classBuffer, classSize);
		printf("Pack200: enable\r\n");
	}
	if(classBuffer == NULL) {
		jarBuffer = GetFileData(jarfile, &jarSize);
		SetResource(exefile, "JAR", RT_RCDATA, jarBuffer, jarSize);
		HeapFree(GetProcessHeap(), 0, jarBuffer);
		classBuffer = GetResource("CLASSIC_LOADER", RT_RCDATA, &classSize);
		SetResource(exefile, "CLASSIC_LOADER", RT_RCDATA, classBuffer, classSize);
		printf("Pack200: disable\r\n");
	}
	
	if(opt['e'] && *opt['e'] != '-' && *opt['e'] != '\0') {
		SetResource(exefile, "EXTFLAGS", RT_RCDATA, opt['e'], strlen(opt['e']) + 1);
	}
	
	if(opt['a'] && *opt['a'] != '\0') {
		SetResource(exefile, "VMARGS", RT_RCDATA, opt['a'], strlen(opt['a']) + 1);
	}
	
	if(opt['i'] && *opt['i'] != '-' && *opt['i'] != '\0') {
		SetApplicationIcon(exefile, opt['i']);
	}
	
	if(opt['v'] && *opt['v'] != '-' && *opt['v'] != '\0') {
		versionNumber = opt['v'];
	} else {
		versionNumber = defaultVersion;
	}
	if(opt['d'] && *opt['d'] != '-' && *opt['d'] != '\0') {
		fileDescription = opt['d'];
		if(opt['s']) {
			SetResource(exefile, "SVCDESC", RT_RCDATA, opt['d'], strlen(opt['d']) + 1);
		}
	} else {
		fileDescription = (char*)"";
	}
	if(opt['c'] && *opt['c'] != '-' && *opt['c'] != '\0') {
		copyright = opt['c'];
	} else {
		copyright = (char*)"";
	}
	if(opt['C'] && *opt['C'] != '-' && *opt['C'] != '\0') {
		company_name = opt['C'];
	} else {
		company_name = (char*)"";
	}
	if(opt['p'] && *opt['p'] != '-' && *opt['p'] != '\0') {
		product_name = opt['p'];
	} else {
		product_name = (char*)"";
	}
	if(opt['V'] && *opt['V'] != '-' && *opt['V'] != '\0') {
		product_version = opt['V'];
	} else {
		product_version = defaultProductVersion;
	}
	original_filename = strrchr(exefile, '\\') + 1;
	char* newVersion = SetVersionInfo(exefile, versionNumber, previousRevision, fileDescription, copyright, company_name, product_name, product_version, original_filename, jarfile);
	printf("%s  version %s\r\n", strrchr(exefile, '\\') + 1, newVersion);
	exit(0);
}

DWORD GetVersionRevision(LPTSTR filename) {
	/* GetFileVersionInfoSize, GetFileVersionInfo を使うと内部で LoadLibrary が使用されるらしく
	 * その後のリソース書き込みがうまくいかなくなるようです。なので、自力で EXEファイルから
	 * リビジョンナンバーを取り出すように変更しました。
	 */
	DWORD  revision = 0;
	HANDLE hFile;
	char   HEADER[] = "VS_VERSION_INFO";
	char*  buf;
	DWORD  size;
	int    i, j;
	
	buf = (char*)HeapAlloc(GetProcessHeap(), 0, 8192);
	
	if((hFile = CreateFile(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
		return 0;
	}
	
	SetFilePointer(hFile, -1 * 8192, 0, FILE_END);
	ReadFile(hFile, buf, 8192, &size, NULL);
	CloseHandle(hFile);
	
	for(i = 0; i < size - strlen(HEADER); i++) {
		for(j = 0; j < strlen(HEADER); j++) {
			if(buf[i + j * 2] != HEADER[j]) break;
		}
		if(j == strlen(HEADER)) {
			revision = ((buf[i + 47] << 8) & 0xFF00)
				  | ((buf[i + 46]     ) & 0x00FF);
		}
	}
	HeapFree(GetProcessHeap(), 0, buf);
	return revision;
}

char* SetVersionInfo(LPCTSTR filename, char* versionNumber, DWORD previousRevision, char* fileDescription, char* copyright, char* company_name, char* product_name, char* product_version, char* original_filename, char* jarfile) {
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
	char* versioninfoBuffer;
	DWORD versioninfoSize;
	
	if(strrchr(jarfile, '\\') != NULL) {
		internalName = strrchr(jarfile, '\\') + 1;
	} else {
		internalName = jarfile;
	}
	
	strcpy(buffer, versionNumber);
	strcat(buffer, ".0.0.0.0");
	short file_version_major    = atoi(strtok(buffer, "."));
	short file_version_minor    = atoi(strtok(NULL, "."));
	short file_version_build    = atoi(strtok(NULL, "."));
	short file_version_revision = atoi(strtok(NULL, "."));
	
	strcpy(buffer, product_version);
	strcat(buffer, ".0.0.0.0");
	short product_version_major    = atoi(strtok(buffer, "."));
	short product_version_minor    = atoi(strtok(NULL, "."));
	short product_version_build    = atoi(strtok(NULL, "."));
	short product_version_revision = atoi(strtok(NULL, "."));
	
	// revison が明示的に指定されていなかった場合、既存ファイルから取得した値に 1　を加算して revision とする。
	strcpy(buffer, versionNumber);
	if(strtok(buffer, ".") != NULL) {
		if(strtok(NULL, ".") != NULL) {
			if(strtok(NULL, ".") != NULL) {
				if(strtok(NULL, ".") != NULL) {
					previousRevision = file_version_revision - 1;
				}
			}
		}
	}
	
	file_version_revision = previousRevision + 1;
	// build 加算判定ここまで。
	sprintf(file_version, "%d.%d.%d.%d", file_version_major, file_version_minor, file_version_build, file_version_revision);
	
	versioninfoBuffer = GetResource("VERSION_INFO", RT_RCDATA, &versioninfoSize);
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
	
	ZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, company_name, strlen(company_name), (WCHAR*)buffer, 128);
	for(i = 0; i < SIZE_TEXT; i++) {
		versioninfoBuffer[ADDR_COMPANY_NAME + i] = buffer[i];
	}
	
	ZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, fileDescription, strlen(fileDescription), (WCHAR*)buffer, 128);
	for(i = 0; i < SIZE_TEXT; i++) {
		versioninfoBuffer[ADDR_FILE_DESCRIPTION + i] = buffer[i];
	}
	
	ZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, file_version, strlen(file_version), (WCHAR*)buffer, 128);
	for(i = 0; i < SIZE_VERSION; i++) {
		versioninfoBuffer[ADDR_FILE_VERSION + i] = buffer[i];
	}
	
	ZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, internalName, strlen(internalName), (WCHAR*)buffer, 128);
	for(i = 0; i < SIZE_TEXT; i++) {
		versioninfoBuffer[ADDR_INTERNAL_NAME + i] = buffer[i];
	}
	
	ZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, copyright, strlen(copyright), (WCHAR*)buffer, 128);
	for(i = 0; i < SIZE_TEXT; i++) {
		versioninfoBuffer[ADDR_COPYRIGHT + i] = buffer[i];
	}
	
	ZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, original_filename, strlen(original_filename), (WCHAR*)buffer, 128);
	for(i = 0; i < SIZE_TEXT; i++) {
		versioninfoBuffer[ADDR_ORIGINAL_FILENAME + i] = buffer[i];
	}
	
	ZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, product_name, strlen(product_name), (WCHAR*)buffer, 128);
	for(i = 0; i < SIZE_TEXT; i++) {
		versioninfoBuffer[ADDR_PRODUCT_NAME + i] = buffer[i];
	}
	
	ZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, product_version, strlen(product_version), (WCHAR*)buffer, 128);
	for(i = 0; i < SIZE_VERSION; i++) {
		versioninfoBuffer[ADDR_PRODUCT_VERSION + i] = buffer[i];
	}
	
	SetResource(filename, (LPCTSTR)VS_VERSION_INFO, RT_VERSION, versioninfoBuffer, versioninfoSize);
	
	char* newVersion = (char*)HeapAlloc(GetProcessHeap(), 0, 128);
	sprintf(newVersion, "%d.%d.%d.%d", file_version_major, file_version_minor, file_version_build, file_version_revision);
	return newVersion;
}

char** ParseOption(int argc, char* argv[]) {
	int i;
	char** opt = (char**)HeapAlloc(GetProcessHeap(), 0, 256 * 8);

	for(i = 0; i < 256; i++) {
		opt[i] = NULL;
	}
	if((argc > 1) && (*argv[1] != '-')) {
		opt[0] = argv[1];
	}
	while(*++argv) {
		if(*argv[0] == '-') {
			if(argv[1] == NULL) {
				opt[*(argv[0] + 1)] = (char*)"";
			} else {
				opt[*(argv[0] + 1)] = argv[1];
			}
		}
	}
	argv--;
	if((opt[0] == NULL) && (*argv[0] != '-')) {
		opt[0] = argv[0];
	}
	return opt;
}

char* GetResource(LPCTSTR name, LPCTSTR type, DWORD* size) {
	HRSRC hrsrc;

	if((hrsrc = FindResource(NULL, name, type)) == NULL) {
		fprintf(stderr, "Fail to FindResource: %s\n", name);
		exit(0);
	}
	*size = SizeofResource(NULL, hrsrc);
	return (char*)LockResource(LoadResource(NULL, hrsrc));
}

void SetResource(LPCTSTR filename, LPCTSTR rscName, LPCTSTR rscType, char* rscData, DWORD rscSize) {
	HANDLE hRes;
	BOOL   ret1;
	BOOL   ret2;
	int i;
	
	for(i = 0; i < 100; i++) {
		ret1 = FALSE;
		ret2 = FALSE;
		hRes = BeginUpdateResource(filename, FALSE);
		if(hRes != NULL) {
			ret1 = UpdateResource(hRes, rscType, rscName, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), rscData, rscSize);
			ret2 = EndUpdateResource(hRes, FALSE);
		}
		if(ret1 && ret2) {
			break;
		}
		Sleep(100);
	}
	if(ret1 == FALSE || ret2 == FALSE) {
		fprintf(stderr, "Failed to update resource: %s: %s\n", filename, rscName);
		exit(0);
	}
	CloseHandle(hRes);
}

void CreateExeFile(LPCTSTR filename, char* data, DWORD size) {
	HANDLE hFile;

	if((hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
		if((hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
			fprintf(stderr, "Failed to create file: %s\n", filename);
			exit(0);
		}
	}

	DWORD writeSize;
	while(size > 0) {
		if(WriteFile(hFile, data, size, &writeSize, NULL) == 0) {
			fprintf(stderr, "Failed to write: %s\n", filename);
			exit(0);
		}
		data += writeSize;
		size -= writeSize;
	}
	CloseHandle(hFile);
}

char* GetFileData(LPCTSTR filename, DWORD* size) {
	HANDLE hFile;
	if((hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Failed to read file: %s\n", filename);
		exit(0);
	}
	*size = GetFileSize(hFile, NULL);
	char* buf = (char*)HeapAlloc(GetProcessHeap(), 0, *size);

	char* p = buf;
	DWORD r = *size;
	DWORD readSize;

	while(r > 0) {
		if(ReadFile(hFile, p, r, &readSize, NULL) == 0) {
			fprintf(stderr, "Failed to read: %s\n", filename);
			HeapFree(GetProcessHeap(), 0, buf);
			exit(0);
		}
		p += readSize;
		r -= readSize;
	}
	CloseHandle(hFile);

	return buf;
}

void SetApplicationIcon(LPCTSTR filename, LPCTSTR iconfile) {
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
	
	if(!strlen(iconfile)) {
		return;
	}
	if((f = _lopen(iconfile, OF_READ)) == -1) {
		return;
	}
	
	for(i = 0; i < 100; i++) {
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
		
		for(z = 0; z < id.idCount; z++) {
			pgid->idEntries[z].common = pid->idEntries[z].common;
			pgid->idEntries[z].nID = z + 1;
			nSize = pid->idEntries[z].common.dwBytesInRes;
			pvFile = HeapAlloc(GetProcessHeap(), 0, nSize);
			if(!pvFile) {
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
		if(ret1 && ret2 && ret3) {
			break;
		}
		Sleep(100);
	}
	if(ret1 == FALSE || ret2 == FALSE || ret3 == FALSE) {
		fprintf(stderr, "Failed to set icon group\n");
		return;
	}
	CloseHandle(hResource);
	return;
}

DWORD GetTargetJavaRuntimeVersion(char* version) {
	char* p = version;
	DWORD major = 0;
	DWORD minor = 0;
	DWORD build = 0;
	DWORD revision = 0;
	
	if(p != NULL) {
		major = atoi(p);
		if((p = strstr(p, ".")) != NULL) {
			minor = atoi(++p);
			if((p = strstr(p, ".")) != NULL) {
				build = atoi(++p);
				if((p = strstr(p, ".")) != NULL) {
					revision = atoi(++p);
				}
			}
		}
		return major << 24 & 0xFF000000 | minor << 16 & 0x00FF0000 | build << 8 & 0x0000FF00 | revision & 0x000000FF;
	}
	return 0x00000000;
}

LPTSTR GetTargetJavaRuntimeVersionString(DWORD version) {
	DWORD major = version >> 24 & 0xFF;
	DWORD minor = version >> 16 & 0xFF;
	DWORD build = version >> 8 & 0xFF;
	DWORD revision = version & 0xFF;
	LPTSTR buf = (LPTSTR)HeapAlloc(GetProcessHeap(), 0, 64);

	*(DWORD*)buf = version;

	//1.5, 1.6
	if(major == 1 && (minor == 5 || minor == 6)) {
		if(revision == 0) {
			sprintf(buf + 4, "Java %d.%d", minor, build);
		} else {
			sprintf(buf + 4, "Java %d.%d.%d", minor, build, revision);
		}
		return buf;
	}

	//1.2, 1.3, 1.4
	if(major == 1 && (minor == 2 || minor == 3 || minor == 4)) {
		if(revision == 0) {
			if(build == 0) {
				sprintf(buf + 4, "Java2 %d.%d", major, minor);
			} else {
				sprintf(buf + 4, "Java2 %d.%d.%d", major, minor, build);
			}
		} else {
			sprintf(buf + 4, "Java2 %d.%d.%d.%d", major, minor, build, revision);
		}
		return buf;
	}

	//1.0, 1.1
	if(major == 1 && (minor == 0 || minor == 1)) {
		if(revision == 0) {
			if(build == 0) {
				sprintf(buf + 4, "Java %d.%d", major, minor);
			} else {
				sprintf(buf + 4, "Java %d.%d.%d", major, minor, build);
			}
		} else {
			sprintf(buf + 4, "Java %d.%d.%d.%d", major, minor, build, revision);
		}
		return buf;
	}

	//other
	if(revision == 0) {
		sprintf(buf + 4, "Java %d.%d.%d", major, minor, build);
	} else {
		sprintf(buf + 4, "Java %d.%d.%d.%d", major, minor, build, revision);
	}
	return buf;
}

void UsePack200(LPCTSTR exefile, LPCTSTR jarfile) {
	DWORD size;
	char* buf = GetResource("JAR_OPTIMIZER", RT_RCDATA, &size);

	jclass optimizerClass = (*env)->DefineClass(env, "JarOptimizer", NULL, (jbyte*)buf, size);
	if(optimizerClass == NULL) {
		return;
	}
	jmethodID optimizerInit = (*env)->GetMethodID(env, optimizerClass, "<init>", "(Ljava/lang/String;)V");
	if(optimizerInit == NULL) {
		return;
	}

	jobject optimizer = (*env)->NewObject(env, optimizerClass, optimizerInit, GetJString(env, jarfile));
	if(optimizer == NULL) {
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
		return;
	}
	jmethodID getRelativeClassPath = (*env)->GetMethodID(env, optimizerClass, "getRelativeClassPath", "()[B");
	if(getRelativeClassPath == NULL) {
		return;
	}
	jmethodID getClassesPackGz = (*env)->GetMethodID(env, optimizerClass, "getClassesPackGz", "()[B");
	if(getClassesPackGz == NULL) {
		return;
	}
	jmethodID getResourcesGz = (*env)->GetMethodID(env, optimizerClass, "getResourcesGz", "()[B");
	if(getResourcesGz == NULL) {
		return;
	}
	jmethodID getSplashPath = (*env)->GetMethodID(env, optimizerClass, "getSplashPath", "()[B");
	if(getSplashPath == NULL) {
		return;
	}
	jmethodID getSplashImage = (*env)->GetMethodID(env, optimizerClass, "getSplashImage", "()[B");
	if(getSplashImage == NULL) {
		return;
	}
	jbyteArray relativeClassPath = (jbyteArray)((*env)->CallObjectMethod(env, optimizer, getRelativeClassPath));
	if(relativeClassPath != NULL) {
		size = (*env)->GetArrayLength(env, relativeClassPath);
		buf = (char*)((*env)->GetByteArrayElements(env, relativeClassPath, NULL));
		SetResource(exefile, "RELATIVE_CLASSPATH", RT_RCDATA, buf, size);
	}
	jbyteArray classesPackGz = (jbyteArray)((*env)->CallObjectMethod(env, optimizer, getClassesPackGz));
	if(classesPackGz != NULL) {
		size = (*env)->GetArrayLength(env, classesPackGz);
		buf = (char*)((*env)->GetByteArrayElements(env, classesPackGz, NULL));
		SetResource(exefile, "CLASSES_PACK_GZ", RT_RCDATA, buf, size);
	}
	jbyteArray resourcesGz = (jbyteArray)((*env)->CallObjectMethod(env, optimizer, getResourcesGz));
	if(resourcesGz != NULL) {
		size = (*env)->GetArrayLength(env, resourcesGz);
		buf = (char*)((*env)->GetByteArrayElements(env, resourcesGz, NULL));
		SetResource(exefile, "RESOURCES_GZ", RT_RCDATA, buf, size);
	}
	jbyteArray splashPath = (jbyteArray)((*env)->CallObjectMethod(env, optimizer, getSplashPath));
	if(splashPath != NULL) {
		size = (*env)->GetArrayLength(env, splashPath);
		buf = (char*)((*env)->GetByteArrayElements(env, splashPath, NULL));
		SetResource(exefile, "SPLASH_PATH", RT_RCDATA, buf, size);
	}
	jbyteArray splashImage = (jbyteArray)((*env)->CallObjectMethod(env, optimizer, getSplashImage));
	if(splashImage != NULL) {
		size = (*env)->GetArrayLength(env, splashImage);
		buf = (char*)((*env)->GetByteArrayElements(env, splashImage, NULL));
		SetResource(exefile, "SPLASH_IMAGE", RT_RCDATA, buf, size);
	}
}
