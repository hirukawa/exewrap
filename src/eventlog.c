#include <windows.h>
#include <stdio.h>
#include "include/eventlog.h"

int  InstallEventLog();
int  RemoveEventLog();
void WriteEventLog(WORD type, const char* message);
static void ShowErrorMessage();

int InstallEventLog()
{
	int   err = 0;
	HKEY  hKey = NULL;
	DWORD keys;
	DWORD LastError = 0;
	char buffer[MAX_PATH];
	char module[MAX_PATH];
	char* source;
	char* key;
	DWORD types;

	GetModuleFileName(NULL, buffer, MAX_PATH);
	lstrcpy(module, buffer);
	*(strrchr(buffer, '.')) = 0;
	source = strrchr(buffer, '\\') + 1;
	key = (char*)HeapAlloc(GetProcessHeap(), 0, 1024);
	lstrcpy(key, "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\");
	lstrcat(key, source);
	
	if((LastError = RegCreateKeyEx(HKEY_LOCAL_MACHINE, key, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &keys)) != ERROR_SUCCESS)
	{
		SetLastError(LastError);
		ShowErrorMessage();
		err = (int)LastError;
		goto EXIT;
	}
	
	if((LastError = RegSetValueEx(hKey, "EventMessageFile", 0, REG_EXPAND_SZ, (LPBYTE)module, lstrlen(module) + 1)) != ERROR_SUCCESS)
	{
		SetLastError(LastError);
		ShowErrorMessage();
		err = (int)LastError;
		goto EXIT;
	}
	
	types = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE; 
	if((LastError = RegSetValueEx(hKey, "TypesSupported", 0, REG_DWORD, (LPBYTE)&types, sizeof(DWORD))) != ERROR_SUCCESS)
	{
		SetLastError(LastError);
		ShowErrorMessage();
		err = (int)LastError;
		goto EXIT;
	}

EXIT:
	if(hKey != NULL)
	{
		RegCloseKey(hKey);
	}
	if(key != NULL)
	{
		HeapFree(GetProcessHeap(), 0, key);
	}
	return err;
}

int RemoveEventLog()
{
	int   err = 0;
	HKEY  hKey = NULL;
	DWORD LastError = 0;
	char buffer[MAX_PATH];
	char module[MAX_PATH];
	char* source;
	char* key;
	
	key = (char*)HeapAlloc(GetProcessHeap(), 0, 256);
	lstrcpy(key, "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\");

	GetModuleFileName(NULL, buffer, MAX_PATH);
	lstrcpy(module, buffer);
	*(strrchr(buffer, '.')) = 0;
	source = strrchr(buffer, '\\') + 1;
	
	if((LastError = RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_ALL_ACCESS, &hKey)) != ERROR_SUCCESS)
	{
		SetLastError(LastError);
		ShowErrorMessage();
		err = (int)LastError;
		goto EXIT;
	}
	
	if((LastError = RegDeleteKey(hKey, source)) != ERROR_SUCCESS)
	{
		SetLastError(LastError);
		ShowErrorMessage();
		err = (int)LastError;
		goto EXIT;
	}

EXIT:
	if(hKey != NULL)
	{
		RegCloseKey(hKey);
	}
	if(key != NULL)
	{
		HeapFree(GetProcessHeap(), 0, key);
	}
	return err;
}

void WriteEventLog(WORD type, const char* message)
{
    HANDLE hEventLog;
	const char* messages[] = {message};
	char buffer[MAX_PATH];
	char* source;

	GetModuleFileName(NULL, buffer, MAX_PATH);
	*(strrchr(buffer, '.')) = 0;
	source = strrchr(buffer, '\\') + 1;

	if((hEventLog = RegisterEventSource(NULL, source)) == NULL)
	{
		char m[] = "error: RegisterEvnetSource\n";
		int size = lstrlen(m);
		DWORD written;

		WriteConsole(GetStdHandle(STD_ERROR_HANDLE), m, size, &written, NULL);
		ExitProcess(0);
	}
	
	ReportEvent(hEventLog, type, 0, DEFAULT_EVENT_ID, NULL, 1, 0, (LPCTSTR*)messages, NULL);
	
	DeregisterEventSource(hEventLog);
}

void ShowErrorMessage()
{
	LPVOID Message = NULL;
	DWORD LastError = GetLastError();

	if(LastError != 0)
	{
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&Message, 0, NULL);

		printf("%s", Message);
		LocalFree(Message);
	}
}
