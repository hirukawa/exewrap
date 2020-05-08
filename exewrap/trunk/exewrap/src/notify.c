#include <windows.h>
#include <process.h>
#include <stdio.h>

#include "include\loader.h"

#define WM_APP_NOTIFY (WM_APP + 0x702)

HANDLE notify_exec(UINT (WINAPI *start_address)(void*), int argc, const wchar_t* argv[]);
void   notify_close(void);

static wchar_t* create_arglist(int argc, const wchar_t* argv[]);
static UINT WINAPI listener(void* arglist);

static HANDLE shared_memory_handle = NULL;
static HANDLE listener_thread_handle = NULL;
static UINT   listener_thread_id = 0;

UINT (WINAPI *p_callback_function)(void* x);
HANDLE g_event = NULL;


HANDLE notify_exec(UINT (WINAPI *_p_callback_function)(void*), int argc, const wchar_t* argv[])
{
	DWORD    ret;
	DWORD    shared_memory_size = 4096;
	wchar_t* shared_memory_name = get_module_object_name(L"SHARED_MEMORY");
	wchar_t* synchronize_mutex_name = get_module_object_name(L"SYNCHRONIZE");
	HANDLE   synchronize_mutex_handle = CreateMutex(NULL, FALSE, synchronize_mutex_name);

	WaitForSingleObject(synchronize_mutex_handle, INFINITE);

TRY_CREATE_SHARED_MEMORY:
	shared_memory_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE | SEC_COMMIT | SEC_NOCACHE, 0, shared_memory_size, shared_memory_name);
	ret = GetLastError();
	if(ret == ERROR_ALREADY_EXISTS)
	{
		DWORD wait_result = WAIT_OBJECT_0;
		LPBYTE lpShared = (LPBYTE)MapViewOfFile(shared_memory_handle, FILE_MAP_WRITE, 0, 0, 0);

		if(lpShared != NULL)
		{
			DWORD process_id = *((DWORD*)(lpShared + 0));
			DWORD thread_id = *((DWORD*)(lpShared + sizeof(DWORD)));

			if(process_id != 0 && thread_id != 0)
			{
				wchar_t* arglist = create_arglist(argc, argv);

				if(arglist != NULL)
				{
					wchar_t* shared_memory_read_event_name = get_module_object_name(L"SHARED_MEMORY_READ");
					HANDLE   shared_memory_read_event_handle = CreateEvent(NULL, FALSE, FALSE, shared_memory_read_event_name);

					wcscpy_s((wchar_t*)(lpShared + sizeof(DWORD) + sizeof(DWORD)), shared_memory_size - sizeof(DWORD) - sizeof(DWORD), arglist);
					FlushViewOfFile(lpShared, 0);
					UnmapViewOfFile(lpShared);
					free(arglist);
					AllowSetForegroundWindow(process_id);
					PostThreadMessage(thread_id, WM_APP_NOTIFY, (WPARAM)0, (LPARAM)0);
					wait_result = WaitForSingleObject(shared_memory_read_event_handle, 5000);

					CloseHandle(shared_memory_read_event_handle);
					free(shared_memory_read_event_name);
				}
			}
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
	}
	free(synchronize_mutex_name);
	
	return synchronize_mutex_handle;
}

void notify_close()
{
	if(shared_memory_handle != NULL)
	{
		wchar_t* synchronize_mutex_name = get_module_object_name(L"SYNCHRONIZE");
		HANDLE   synchronize_mutex_handle = CreateMutex(NULL, FALSE, synchronize_mutex_name);
		LPBYTE   lpShared;

		WaitForSingleObject(synchronize_mutex_handle, INFINITE);

		lpShared = (LPBYTE)MapViewOfFile(shared_memory_handle, FILE_MAP_WRITE, 0, 0, 0);
		if(lpShared != NULL)
		{
			*((DWORD*)(lpShared + 0)) = 0;
			*((DWORD*)(lpShared + sizeof(DWORD))) = 0;
			FlushViewOfFile(lpShared, 0);
			UnmapViewOfFile(lpShared);
		}

		ReleaseMutex(synchronize_mutex_handle);
		CloseHandle(synchronize_mutex_handle);
		free(synchronize_mutex_name);
	}

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


static wchar_t* create_arglist(int argc, const wchar_t* argv[])
{
	int      i;
	size_t   len = 0;
	wchar_t* buf;

	for(i = 0; i < argc; i++)
	{
		len += wcslen(argv[i]) + 1;
	}
	buf = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
	if(buf == NULL)
	{
		return NULL;
	}
	buf[0] = L'\0';
	for(i = 0; i < argc; i++)
	{
		wcscat_s(buf, len + 1, argv[i]);
		wcscat_s(buf, len + 1, L"\n");
	}
	return buf;
}


static UINT WINAPI listener(void* arglist)
{
	BOOL   b;
	MSG    msg;
	LPBYTE lpShared;

	UNREFERENCED_PARAMETER(arglist);
	
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
			UINT   threadId;

			hThread = (HANDLE)_beginthreadex(NULL, 0, p_callback_function, shared_memory_handle, 0, &threadId);
			CloseHandle(hThread);
		}
	}

	return 0;
}
