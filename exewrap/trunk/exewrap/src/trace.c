#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <stdio.h>
#include <string.h>
#include "minhook\include\MinHook.h"

#define BUFFER_SIZE 1024

const char* StartTrace();
void        StopTrace();

static char*  _w2a(LPCWSTR s);

static char*  filename1 = NULL;
static char*  filename2 = NULL;
static HANDLE hStdOut = NULL;
static FILE*  fpStdOut = NULL;
static FILE*  fp = NULL;

typedef HANDLE(WINAPI *CREATEFILEA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef HANDLE(WINAPI *CREATEFILEW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef HMODULE(WINAPI *LOADLIBRARYA)(LPCSTR);
typedef HMODULE(WINAPI *LOADLIBRARYW)(LPCWSTR);
typedef HMODULE(WINAPI *LOADLIBRARYEXA)(LPCSTR, HANDLE, DWORD);
typedef HMODULE(WINAPI *LOADLIBRARYEXW)(LPCWSTR, HANDLE, DWORD);

CREATEFILEA    fpCreateFileA = NULL;
CREATEFILEW    fpCreateFileW = NULL;
LOADLIBRARYA   fpLoadLibraryA = NULL;
LOADLIBRARYW   fpLoadLibraryW = NULL;
LOADLIBRARYEXA fpLoadLibraryExA = NULL;
LOADLIBRARYEXW fpLoadLibraryExW = NULL;

HANDLE  WINAPI DetourCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
HANDLE  WINAPI DetourCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
HMODULE WINAPI DetourLoadLibraryA(LPCSTR lpLibFileName);
HMODULE WINAPI DetourLoadLibraryW(LPCWSTR lpLibFileName);
HMODULE WINAPI DetourLoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
HMODULE WINAPI DetourLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);

static void   print(LPCSTR type, LPCSTR full_path_name);
static LPSTR  is_file_in_JRE(LPCSTR lpFileName);

const char* StartTrace(BOOL replace_stdout)
{
	BOOL succeeded = FALSE;

	filename1 = (char*)malloc(1024);
	GetModuleFileName(NULL, filename1, 1024);
	*strrchr(filename1, '.') = '\0';
	strcat(filename1, ".log");

	if (replace_stdout)
	{
		hStdOut = CreateFile(filename1, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hStdOut == INVALID_HANDLE_VALUE)
		{
			goto EXIT;
		}

		fpStdOut = freopen(filename1, "wb", stdout);
		if (fpStdOut == NULL)
		{
			goto EXIT;
		}
	}
	else
	{
		filename2 = (char*)malloc(1024);
		strcpy(filename2, filename1);
		*strrchr(filename2, '.') = '\0';
		strcat(filename2, "2.log");
		fp = fopen(filename2, "wb");
	}

	if (MH_Initialize() != MH_OK)
	{
		goto EXIT;
	}

	if (MH_CreateHook(&CreateFileA, &DetourCreateFileA, (LPVOID*)(&fpCreateFileA)) != MH_OK)
	{
		goto EXIT;
	}
	if (MH_EnableHook(&CreateFileA) != MH_OK)
	{
		goto EXIT;
	}

	if (MH_CreateHook(&CreateFileW, &DetourCreateFileW, (LPVOID*)(&fpCreateFileW)) != MH_OK)
	{
		goto EXIT;
	}
	if (MH_EnableHook(&CreateFileW) != MH_OK)
	{
		goto EXIT;
	}

	if (MH_CreateHook(&LoadLibraryA, &DetourLoadLibraryA, (LPVOID*)(&fpLoadLibraryA)) != MH_OK)
	{
		goto EXIT;
	}
	if (MH_EnableHook(&LoadLibraryA) != MH_OK)
	{
		goto EXIT;
	}

	if (MH_CreateHook(&LoadLibraryW, &DetourLoadLibraryW, (LPVOID*)(&fpLoadLibraryW)) != MH_OK)
	{
		goto EXIT;
	}
	if (MH_EnableHook(&LoadLibraryW) != MH_OK)
	{
		goto EXIT;
	}

	if (MH_CreateHook(&LoadLibraryExA, &DetourLoadLibraryExA, (LPVOID*)(&fpLoadLibraryExA)) != MH_OK)
	{
		goto EXIT;
	}
	if (MH_EnableHook(&LoadLibraryExA) != MH_OK)
	{
		goto EXIT;
	}

	if (MH_CreateHook(&LoadLibraryExW, &DetourLoadLibraryExW, (LPVOID*)(&fpLoadLibraryExW)) != MH_OK)
	{
		goto EXIT;
	}
	if (MH_EnableHook(&LoadLibraryExW) != MH_OK)
	{
		goto EXIT;
	}

	succeeded = TRUE;

EXIT:
	if (!succeeded)
	{
		free(filename1);
		filename1 = NULL;
	}
	return filename1;
}


void StopTrace()
{
	if (MH_DisableHook(&LoadLibraryW) != MH_OK)
	{
	}

	if (MH_DisableHook(&CreateFileW) != MH_OK)
	{
	}

	if (MH_Uninitialize() != MH_OK)
	{
	}

	if (hStdOut != NULL && hStdOut != INVALID_HANDLE_VALUE)
	{
		FlushFileBuffers(hStdOut);
		CloseHandle(hStdOut);
	}
	hStdOut = NULL;

	if (fpStdOut != NULL)
	{
		fflush(fpStdOut);
		fclose(fpStdOut);
	}
	fpStdOut = NULL;

	if (fp != NULL)
	{
		fclose(fp);
	}
	fp = NULL;

	if (filename2 != NULL)
	{
		free(filename2);
	}

	if (filename1 != NULL)
	{
		free(filename1);
	}
}


static void print(LPCSTR type, LPCSTR full_path_name)
{
	if (fpStdOut != NULL)
	{
		printf("[%s %s]\n", type, full_path_name);
		fflush(stdout);
	}
	else if (fp != NULL)
	{
		fprintf(fp, "[%s %s]\n", type, full_path_name);
		fflush(fp);
	}
}


static LPSTR is_file_in_JRE(LPCSTR lpFileName)
{
	BOOL   b = FALSE;
	LPSTR  buf = NULL;
	LPSTR  java_home = NULL;
	LPSTR  full_path_name = NULL;
	DWORD  size1;
	DWORD  size2;
	LPSTR  ptr;

	buf = (LPSTR)malloc(BUFFER_SIZE);
	size1 = GetEnvironmentVariableA("JAVA_HOME", buf, BUFFER_SIZE);
	if (size1 > 0)
	{
		java_home = (LPSTR)malloc(BUFFER_SIZE);
		size1 = GetFullPathNameA(buf, BUFFER_SIZE, java_home, &ptr);
		if (size1 > 0)
		{
			full_path_name = (LPSTR)malloc(BUFFER_SIZE);
			size2 = GetFullPathNameA(lpFileName, BUFFER_SIZE, full_path_name, &ptr);
			if (size2 > 0)
			{
				if (_memicmp(java_home, full_path_name, size1) == 0)
				{
					b = TRUE;
				}
			}
		}
	}

	if (buf != NULL)
	{
		free(buf);
	}
	if (java_home != NULL)
	{
		free(java_home);
	}
	if (!b && full_path_name != NULL)
	{
		free(full_path_name);
		full_path_name = NULL;
	}
	return full_path_name;
}


HANDLE WINAPI DetourCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	LPSTR full_path_name;
	
	full_path_name = is_file_in_JRE(lpFileName);
	if (full_path_name != NULL)
	{
		print("CreateFile", full_path_name);
		free(full_path_name);
	}

	return fpCreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}


HANDLE WINAPI DetourCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	LPSTR lpFileNameA = _w2a(lpFileName);
	LPSTR full_path_name;

	full_path_name = is_file_in_JRE(lpFileNameA);
	if (full_path_name != NULL)
	{
		print("CreateFile", full_path_name);
		free(full_path_name);
	}
	free(lpFileNameA);

	return fpCreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}


HMODULE WINAPI DetourLoadLibraryA(LPCSTR lpLibFileName)
{
	HMODULE hModule;
	LPSTR   buf = (LPSTR)malloc(BUFFER_SIZE);
	LPSTR   full_path_name = NULL;

	hModule = fpLoadLibraryA(lpLibFileName);
	if (hModule != NULL)
	{
		if (GetModuleFileNameA(hModule, buf, BUFFER_SIZE) > 0)
		{
			full_path_name = is_file_in_JRE(buf);
			if (full_path_name != NULL)
			{
				print("LoadLibrary", full_path_name);
			}
		}
	}

	if (full_path_name != NULL)
	{
		free(full_path_name);
	}
	free(buf);

	return hModule;
}


HMODULE WINAPI DetourLoadLibraryW(LPCWSTR lpLibFileName)
{
	HMODULE hModule;
	LPSTR   buf = (LPSTR)malloc(BUFFER_SIZE);
	LPSTR   full_path_name = NULL;

	hModule = fpLoadLibraryW(lpLibFileName);
	if (hModule != NULL)
	{
		if (GetModuleFileName(hModule, buf, BUFFER_SIZE) > 0)
		{
			full_path_name = is_file_in_JRE(buf);
			if (full_path_name != NULL)
			{
				print("LoadLibrary", full_path_name);
			}
		}
	}

	if (full_path_name != NULL)
	{
		free(full_path_name);
	}
	free(buf);

	return hModule;
}


HMODULE WINAPI DetourLoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	HMODULE hModule;
	LPSTR   buf = (LPSTR)malloc(BUFFER_SIZE);
	LPSTR   full_path_name = NULL;

	hModule = fpLoadLibraryExA(lpLibFileName, hFile, dwFlags);
	if (hModule != NULL)
	{
		if (GetModuleFileNameA(hModule, buf, BUFFER_SIZE) > 0)
		{
			full_path_name = is_file_in_JRE(buf);
			if (full_path_name != NULL)
			{
				print("LoadLibrary", full_path_name);
			}
		}
	}

	if (full_path_name != NULL)
	{
		free(full_path_name);
	}
	free(buf);

	return hModule;
}


HMODULE WINAPI DetourLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	HMODULE hModule;
	LPSTR   buf = (LPSTR)malloc(BUFFER_SIZE);
	LPSTR   full_path_name = NULL;

	hModule = fpLoadLibraryExW(lpLibFileName, hFile, dwFlags);
	if (hModule != NULL)
	{
		if (GetModuleFileNameA(hModule, buf, BUFFER_SIZE) > 0)
		{
			full_path_name = is_file_in_JRE(buf);
			if (full_path_name != NULL)
			{
				print("LoadLibrary", full_path_name);
			}
		}
	}

	if (full_path_name != NULL)
	{
		free(full_path_name);
	}
	free(buf);

	return hModule;
}

static char* _w2a(LPCWSTR s)
{
	char* buf;
	int ret;

	ret = WideCharToMultiByte(CP_ACP, 0, s, -1, NULL, 0, NULL, NULL);
	if (ret <= 0)
	{
		return NULL;
	}
	buf = (LPSTR)malloc(ret + 1);
	ret = WideCharToMultiByte(CP_ACP, 0, s, -1, buf, (ret + 1), NULL, NULL);
	if (ret == 0)
	{
		free(buf);
		return NULL;
	}
	buf[ret] = '\0';

	return buf;
}
