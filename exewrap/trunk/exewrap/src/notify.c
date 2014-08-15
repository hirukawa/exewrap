#include <windows.h>
#include <process.h>
#include <stdio.h>

#include "include\notify.h"

#define WM_APP_NOTIFY         (WM_APP + 0x702)

HANDLE g_event = NULL;

static char* create_arglist(int argc, char* argv[]);
static DWORD WINAPI listener(void* arglist);
static char* GetModuleSharedMemoryName();

static HANDLE shared_memory_handle = NULL;
static HANDLE listener_thread_handle = NULL;
static DWORD  listener_thread_id = 0;

DWORD (WINAPI *p_callback_function)(void* x);

HANDLE notify_exec(DWORD (WINAPI *_p_callback_function)(void*), int argc, char* argv[])
{
	DWORD dwReturn;
	int len = 2048;
	char*  shared_memory_name = GetModuleObjectName("SHARED_MEMORY");
	char*  synchronize_mutex_name = GetModuleObjectName("SYNCHRONIZE");
	HANDLE synchronize_mutex_handle;

	synchronize_mutex_handle = CreateMutex(NULL, FALSE, synchronize_mutex_name);
	WaitForSingleObject(synchronize_mutex_handle, INFINITE);

TRY_CREATE_SHARED_MEMORY:
	shared_memory_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE | SEC_COMMIT | SEC_NOCACHE, 0, len, shared_memory_name);
	dwReturn = GetLastError();
	if(dwReturn == ERROR_ALREADY_EXISTS)
	{
		DWORD wait_result = WAIT_OBJECT_0;
		LPBYTE lpShared = (LPBYTE)MapViewOfFile(shared_memory_handle, FILE_MAP_WRITE, 0, 0, 0);

		if(lpShared != NULL)
		{
			char*  shared_memory_read_event_name = GetModuleObjectName("SHARED_MEMORY_READ");
			HANDLE shared_memory_read_event_handle = CreateEvent(NULL, FALSE, FALSE, shared_memory_read_event_name);
			DWORD  process_id = *((DWORD*)(lpShared + 0));
			DWORD  thread_id = *((DWORD*)(lpShared + sizeof(DWORD)));
			char*  arglist = create_arglist(argc, argv);

			lstrcpy((char*)(lpShared + sizeof(DWORD) + sizeof(DWORD)), arglist);
			FlushViewOfFile(lpShared, 0);
			UnmapViewOfFile(lpShared);
			HeapFree(GetProcessHeap(), 0, arglist);
			AllowSetForegroundWindow(process_id);
			PostThreadMessage(thread_id, WM_APP_NOTIFY, (WPARAM)0, (LPARAM)0);
			wait_result = WaitForSingleObject(shared_memory_read_event_handle, 5000);
			CloseHandle(shared_memory_read_event_handle);
			HeapFree(GetProcessHeap(), 0, shared_memory_read_event_name);
		}
		CloseHandle(shared_memory_handle);
		shared_memory_handle = NULL;
		if(wait_result != WAIT_OBJECT_0)
		{
			goto TRY_CREATE_SHARED_MEMORY;
		}
		ReleaseMutex(synchronize_mutex_handle);
		CloseHandle(synchronize_mutex_handle);
		synchronize_mutex_handle = NULL;
	}
	else
	{
		p_callback_function = _p_callback_function;
		listener_thread_handle = (HANDLE)_beginthreadex(NULL, 0, listener, NULL, 0, &listener_thread_id);
		//listener_thread_handle = CreateThread(NULL, 0, listener, NULL, 0, &listener_thread_id);
		//_beginthread(listener, 0, NULL);
	}
	HeapFree(GetProcessHeap(), 0, synchronize_mutex_name);
	
	return synchronize_mutex_handle;
}

void notify_close()
{
	if(listener_thread_id != 0)
	{
		PostThreadMessage(listener_thread_id, WM_QUIT, (WPARAM)0, (LPARAM)0);
		listener_thread_id = 0;
	}

	if(shared_memory_handle != NULL)
	{
		if(CloseHandle(shared_memory_handle))
		{
			shared_memory_handle = NULL;
		}
	}

	if(listener_thread_handle != NULL)
	{
		WaitForSingleObject(listener_thread_handle, INFINITE);
		CloseHandle(listener_thread_handle);
		listener_thread_handle = NULL;
	}
}

static char* create_arglist(int argc, char* argv[])
{
	int size = 1;
	int i;
	char* buf;

	for(i = 0; i < argc; i++)
	{
		size += lstrlen(argv[i]) + 1;
	}
	buf = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
	for(i = 0; i < argc; i++)
	{
		lstrcat(buf, argv[i]);
		lstrcat(buf, "\n");
	}
	return buf;
}

static DWORD WINAPI listener(void* arglist)
{
	BOOL b;
	MSG msg;
	LPBYTE lpShared;
	
	lpShared = (LPBYTE)MapViewOfFile(shared_memory_handle, FILE_MAP_WRITE, 0, 0, 0);
	if(lpShared != NULL)
	{
		*((DWORD*)(lpShared + 0)) = GetCurrentProcessId();
		*((DWORD*)(lpShared + sizeof(DWORD))) = GetCurrentThreadId();
		FlushViewOfFile(lpShared, 0);
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
			HANDLE hThread;
			DWORD threadId;

			hThread = (HANDLE)_beginthreadex(NULL, 0, p_callback_function, shared_memory_handle, 0, &threadId);
			//hThread = CreateThread(NULL, 0, p_callback_function, shared_memory_handle, 0, &threadId);
			CloseHandle(hThread);
		}
	}

	return 0;
}

char* GetModuleObjectName(const char* prefix)
{
	char* objectName = (char*)HeapAlloc(GetProcessHeap(), 0, MAX_PATH + 32);
	char* moduleFileName = (char*)HeapAlloc(GetProcessHeap(), 0, MAX_PATH);

	GetModuleFileName(NULL, moduleFileName, MAX_PATH);
	lstrcpy(objectName, "EXEWRAP:");
	if(prefix != NULL)
	{
		lstrcat(objectName, prefix);
	}
	lstrcat(objectName, ":");
	lstrcat(objectName, (char*)(strrchr(moduleFileName, '\\') + 1));

	HeapFree(GetProcessHeap(), 0, moduleFileName);
	return objectName;
}
