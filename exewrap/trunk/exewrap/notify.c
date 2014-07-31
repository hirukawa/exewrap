#include <windows.h>

#include "include\notify.h"

#define WM_APP_LISTENER_READY (WM_APP + 0x701)
#define WM_APP_NOTIFY         (WM_APP + 0x702)

HANDLE g_event = NULL;

static char* create_arglist(int argc, char* argv[]);
static unsigned int listener(void* arglist);
static LPSTR GetModuleSharedMemoryName();

static HANDLE shared_memory_handle = NULL;

void (*p_callback_function)(void* x);

HANDLE notify_exec(void(*_p_callback_function)(void*), int argc, char* argv[])
{
	DWORD dwReturn;
	int len = 2048;
	LPSTR  shared_memory_name = GetModuleObjectName("SHARED_MEMORY");
	LPSTR  synchronize_mutex_name = GetModuleObjectName("SYNCHRONIZE");
	HANDLE synchronize_mutex_handle;

	synchronize_mutex_handle = CreateMutex(NULL, FALSE, synchronize_mutex_name);
	WaitForSingleObject(synchronize_mutex_handle, INFINITE);

TRY_CREATE_SHARED_MEMORY:
	shared_memory_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, len, shared_memory_name);
	dwReturn = GetLastError();
	if(dwReturn == ERROR_ALREADY_EXISTS)
	{
		DWORD wait_result = WAIT_OBJECT_0;
		LPBYTE lpShared = (LPBYTE)MapViewOfFile(shared_memory_handle, FILE_MAP_WRITE, 0, 0, 0);
		if(lpShared != NULL)
		{
			LPSTR shared_memory_read_event_name = GetModuleObjectName("SHARED_MEMORY_READ");
			HANDLE shared_memory_read_event_handle = CreateEvent(NULL, FALSE, FALSE, shared_memory_read_event_name);
			DWORD process_id = *((DWORD*)(lpShared + 0));
			DWORD thread_id = *((DWORD*)(lpShared + sizeof(DWORD)));
			char* arglist = create_arglist(argc, argv);
			strcpy((LPTSTR)(lpShared + sizeof(DWORD) + sizeof(DWORD)), arglist);
			HeapFree(GetProcessHeap(), 0, arglist);
			FlushViewOfFile(lpShared, len);
			AllowSetForegroundWindow(process_id);
			PostThreadMessage(thread_id, WM_APP_NOTIFY, (WPARAM)0, (LPARAM)0);
			UnmapViewOfFile(lpShared);
			wait_result = WaitForSingleObject(shared_memory_read_event_handle, 5000);
			CloseHandle(shared_memory_read_event_handle);
			HeapFree(GetProcessHeap(), 0, shared_memory_read_event_name);
		}
		CloseHandle(shared_memory_handle);
		shared_memory_handle = NULL;
		if(wait_result != WAIT_OBJECT_0) {
			goto TRY_CREATE_SHARED_MEMORY;
		}
		ReleaseMutex(synchronize_mutex_handle);
		CloseHandle(synchronize_mutex_handle);
		synchronize_mutex_handle = NULL;
	} else {
		p_callback_function = _p_callback_function;
		_beginthread(listener, 0, NULL);
	}
	HeapFree(GetProcessHeap(), 0, synchronize_mutex_name);
	return synchronize_mutex_handle;
}

void notify_close()
{
	if(shared_memory_handle != NULL)
	{
		if(CloseHandle(shared_memory_handle))
		{
			shared_memory_handle = NULL;
		}
	}
}

static char* create_arglist(int argc, char* argv[]) {
	int size = 1;
	int i;
	for(i = 0; i < argc; i++)
	{
		size += strlen(argv[i]) + 1;
	}
	char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
	for(i = 0; i < argc; i++)
	{
		strcat(buffer, argv[i]);
		strcat(buffer, "\n");
	}
	return buffer;
}

static unsigned listener(void* arglist) {
	BOOL b;
	MSG msg;
	LPBYTE lpShared;
	
	lpShared = (LPBYTE)MapViewOfFile(shared_memory_handle, FILE_MAP_WRITE, 0, 0, 0);
	if(lpShared != NULL)
	{
		*((DWORD*)(lpShared + 0)) = GetCurrentProcessId();
		*((DWORD*)(lpShared + sizeof(DWORD))) = GetCurrentThreadId();
		FlushViewOfFile(lpShared, 2048);
		UnmapViewOfFile(lpShared);
	}
	
	while((b = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if(b == -1) {
			break;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if(msg.message == WM_APP_NOTIFY)
		{
			_beginthread(p_callback_function, 0, shared_memory_handle);
		}
	}
	_endthread();
}

LPSTR GetModuleObjectName(LPCSTR prefix) {
	LPSTR objectName = (LPSTR)HeapAlloc(GetProcessHeap(), 0, MAX_PATH + 32);
	LPSTR moduleFileName = (LPSTR)HeapAlloc(GetProcessHeap(), 0, MAX_PATH);

	GetModuleFileName(NULL, moduleFileName, MAX_PATH);
	strcpy(objectName, "EXEWRAP:");
	if(prefix != NULL) {
		strcat(objectName, prefix);
	}
	strcat(objectName, ":");
	strcat(objectName, (char*)(lstrrchr(moduleFileName, '\\') + 1));

	HeapFree(GetProcessHeap(), 0, moduleFileName);
	return objectName;
}
