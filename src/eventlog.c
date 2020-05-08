/* このファイルの文字コードは Shift_JIS (MS932) です。*/

#include <windows.h>
#include <stdio.h>

#define DEFAULT_EVENT_ID 0x40000000L
#define MAX_LONG_PATH    32768

DWORD install_event_log(void);
DWORD remove_event_log(void);
DWORD write_event_log(WORD type, const wchar_t* message);

DWORD install_event_log()
{
	DWORD    error  = 0;
	wchar_t* buffer = NULL;
	wchar_t* module = NULL;
	wchar_t* key    = NULL;
	wchar_t* source;
	HKEY     hKey   = NULL;
	DWORD    keys;
	DWORD    types;

	buffer = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(buffer == NULL)
	{
		SetLastError(error = ERROR_NOT_ENOUGH_MEMORY);
		goto EXIT;
	}
	module = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(module == NULL)
	{
		SetLastError(error = ERROR_NOT_ENOUGH_MEMORY);
		goto EXIT;
	}
	key = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(key == NULL)
	{
		SetLastError(error = ERROR_NOT_ENOUGH_MEMORY);
		goto EXIT;
	}

	GetModuleFileName(NULL, buffer, MAX_LONG_PATH);
	wcscpy_s(module, MAX_LONG_PATH, buffer);
	*(wcsrchr(buffer, L'.')) = L'\0';
	source = wcsrchr(buffer, L'\\') + 1;
	wcscpy_s(key, MAX_LONG_PATH, L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\");
	wcscat_s(key, MAX_LONG_PATH, source);

	error = RegCreateKeyEx(HKEY_LOCAL_MACHINE, key, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &keys);
	if(error != ERROR_SUCCESS)
	{
		SetLastError(error);
		goto EXIT;
	}

	error = RegSetValueEx(hKey, L"EventMessageFile", 0, REG_EXPAND_SZ, (BYTE*)module, (DWORD)((wcslen(module) + 1) * sizeof(wchar_t)));
	if(error != ERROR_SUCCESS)
	{
		SetLastError(error);
		goto EXIT;
	}
	
	types = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
	error = RegSetValueEx(hKey, L"TypesSupported", 0, REG_DWORD, (BYTE*)&types, sizeof(DWORD));
	if(error != ERROR_SUCCESS)
	{
		SetLastError(error);
		goto EXIT;
	}

EXIT:
	if(hKey != NULL)
	{
		RegCloseKey(hKey);
	}
	if(key != NULL)
	{
		free(key);
	}
	if(module != NULL)
	{
		free(module);
	}
	if(buffer != NULL)
	{
		free(buffer);
	}
	return error;
}

DWORD remove_event_log()
{
	DWORD    error  = 0;
	wchar_t* buffer = NULL;
	wchar_t* key    = NULL;
	wchar_t* source;
	HKEY     hKey   = NULL;

	buffer = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(buffer == NULL)
	{
		SetLastError(error = ERROR_NOT_ENOUGH_MEMORY);
		goto EXIT;
	}
	key = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(key == NULL)
	{
		SetLastError(error = ERROR_NOT_ENOUGH_MEMORY);
		goto EXIT;
	}

	GetModuleFileName(NULL, buffer, MAX_LONG_PATH);
	*(wcsrchr(buffer, L'.')) = L'\0';
	source = wcsrchr(buffer, L'\\') + 1;
	wcscpy_s(key, MAX_LONG_PATH, L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\");

	error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_ALL_ACCESS, &hKey);
	if(error != ERROR_SUCCESS)
	{
		SetLastError(error);
		goto EXIT;
	}

	error = RegDeleteKey(hKey, source);
	if(error != ERROR_SUCCESS)
	{
		SetLastError(error);
		goto EXIT;
	}

EXIT:
	if(hKey != NULL)
	{
		RegCloseKey(hKey);
	}
	if(key != NULL)
	{
		free(key);
	}
	if(buffer != NULL)
	{
		free(buffer);
	}
	return error;
}

DWORD write_event_log(WORD type, const wchar_t* message)
{
	DWORD    error = 0;
	wchar_t* buffer = NULL;
	wchar_t* source;
	HANDLE   hEventLog = NULL;
	BOOL     b;

	buffer = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(buffer == NULL)
	{
		SetLastError(error = ERROR_NOT_ENOUGH_MEMORY);
		goto EXIT;
	}

	GetModuleFileName(NULL, buffer, MAX_LONG_PATH);
	*(wcsrchr(buffer, L'.')) = L'\0';
	source = wcsrchr(buffer, L'\\') + 1;

	hEventLog = RegisterEventSource(NULL, source);
	if(hEventLog == NULL)
	{
		error = GetLastError();
		goto EXIT;
	}
	
	b = ReportEvent(hEventLog, type, 0, DEFAULT_EVENT_ID, NULL, 1, 0, &message, NULL);
	if(b == 0)
	{
		error = GetLastError();
		goto EXIT;
	}
	
EXIT:
	if(hEventLog != NULL)
	{
		DeregisterEventSource(hEventLog);
	}
	if(buffer != NULL)
	{
		free(buffer);
	}
	return error;
}
