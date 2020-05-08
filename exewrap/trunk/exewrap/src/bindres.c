/*
 * (build)
 * cl.exe /nologo /DUNICODE /D_UNICODE bindres.c
 */

#include <windows.h>
#include <stdio.h>
#include <wchar.h>

#define MAX_LONG_PATH 32768

static void     exit_process(DWORD last_error, const wchar_t* append);
static wchar_t* get_error_message(DWORD last_error);
static int      write_to_handle(DWORD nStdHandle, const wchar_t* str);

int wmain(int argc, wchar_t* argv[])
{
	BOOL      is_reverse = 0;
	wchar_t*  filename;
	wchar_t*  resource_name;
	wchar_t*  resource_filename;
	wchar_t*  filepath;
	wchar_t*  resource_filepath;
	HANDLE    resource_file_handle;
	DWORD     resource_size;
	BYTE*     resource_data;
	BYTE*     p;
	DWORD     r;
	DWORD     read_size;
	BYTE*     tmp;
	DWORD     i;
	BOOL      ret1 = FALSE;
	BOOL      ret2 = FALSE;
	HANDLE    resource_handle = NULL;
	DWORD     last_error = 0;

	if(argc < 4 || (wcscmp(argv[1], L"-r") == 0 && argc < 5))
	{
		write_to_handle(STD_OUTPUT_HANDLE, L"Usage: bindres.exe [-r] <filename> <resource-name> <resource-file>\r\n");
		ExitProcess(ERROR_BAD_ARGUMENTS);
	}
	
	if(wcscmp(argv[1], L"-r") == 0)
	{
		is_reverse = TRUE;
	} 
	filename = argv[1 + (is_reverse ? 1 : 0)];
	resource_name = argv[2 + (is_reverse ? 1 : 0)];
	resource_filename = argv[3 + (is_reverse ? 1 : 0)];

	filepath = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(filepath == NULL)
	{
		exit_process(ERROR_NOT_ENOUGH_MEMORY, L"malloc");
	}
	if(GetFullPathName(filename, MAX_LONG_PATH, filepath, NULL) == 0)
	{
		exit_process(GetLastError(), filename);
	}

	resource_filepath = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(resource_filepath == NULL)
	{
		exit_process(ERROR_NOT_ENOUGH_MEMORY, L"malloc");
	}
	if(GetFullPathName(resource_filename, MAX_LONG_PATH, resource_filepath, NULL) == 0)
	{
		exit_process(GetLastError(), resource_filename);
	}

	resource_file_handle = CreateFile(resource_filepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(resource_file_handle == INVALID_HANDLE_VALUE)
	{
		exit_process(GetLastError(), resource_filepath);
	}
	resource_size = GetFileSize(resource_file_handle, NULL);
	if(resource_size == INVALID_FILE_SIZE)
	{
		exit_process(GetLastError(), L"GetFileSize");
	}
	resource_data = (BYTE*)malloc(resource_size);
	if(resource_data == NULL)
	{
		exit_process(ERROR_NOT_ENOUGH_MEMORY, L"malloc");
	}
	
	p = resource_data;
	r = resource_size;
	
	while(r > 0)
	{
		if(ReadFile(resource_file_handle, p, r, &read_size, NULL) == 0)
		{
			exit_process(GetLastError(), resource_filepath);
		}
		p += read_size;
		r -= read_size;
	}
	CloseHandle(resource_file_handle);
	
	if(is_reverse)
	{
		tmp = (BYTE*)malloc(resource_size);
		if(tmp == NULL)
		{
			exit_process(ERROR_NOT_ENOUGH_MEMORY, L"malloc");
		}
		CopyMemory(tmp, resource_data, resource_size);
		for (i = 0; i < resource_size; i++)
		{
			resource_data[i] = ~tmp[resource_size - 1 - i];
		}
		free(tmp);
	}
	
	for(i = 0; i < 100; i++)
	{
		ret1 = FALSE;
		ret2 = FALSE;
		resource_handle = BeginUpdateResource(filepath, FALSE);
		if(resource_handle != NULL)
		{
			ret1 = UpdateResource(resource_handle, RT_RCDATA, resource_name, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), resource_data, resource_size);
			if(ret1 == FALSE)
			{
				last_error = GetLastError();
			}
			ret2 = EndUpdateResource(resource_handle, FALSE);
			if(ret1 == TRUE && ret2 == FALSE)
			{
				last_error = GetLastError();
			}
		}
		else
		{
			last_error = GetLastError();
		}
		if(ret1 && ret2)
		{
			break;
		}
		Sleep(100);
	}
	if(ret1 == FALSE || ret2 == FALSE)
	{
		exit_process(last_error, filepath);
	}
	CloseHandle(resource_handle);

	free(resource_data);
	return 0;
}

static void exit_process(DWORD last_error, const wchar_t* append)
{
	wchar_t* message;

	message = get_error_message(last_error);
	if(message != NULL)
	{
		write_to_handle(STD_ERROR_HANDLE, message);
		if(append != NULL)
		{
			write_to_handle(STD_ERROR_HANDLE, L" (");
			write_to_handle(STD_ERROR_HANDLE, append);
			write_to_handle(STD_ERROR_HANDLE, L")");
		}
	}
	else if(append != NULL)
	{
		write_to_handle(STD_ERROR_HANDLE, append);
	}
	if(message != NULL || append != NULL)
	{
		write_to_handle(STD_ERROR_HANDLE, L"\r\n");
	}
	ExitProcess(last_error);
}

static wchar_t* get_error_message(DWORD last_error)
{
	DWORD    len;
	wchar_t* buf = NULL;
	wchar_t* message = NULL;

	if(last_error == 0)
	{
		return NULL;
	}

	len = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, last_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (wchar_t*)&buf, 0, NULL);

	if(len > 0)
	{
		while(buf[len - 1] == L'\r' || buf[len - 1] == L'\n')
		{
			buf[--len] = L'\0';
		}
		message = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
		wcscpy_s(message, len + 1, buf);
	}
	LocalFree(buf);

	return message;
}

static int write_to_handle(DWORD nStdHandle, const wchar_t* str)
{
	HANDLE handle;
	UINT   code_page;
	int    mb_size;
	char*  mb_buf;
	DWORD  written;

	handle = GetStdHandle(nStdHandle);
	if(handle == INVALID_HANDLE_VALUE)
	{
		return -1;
	}
	code_page = GetConsoleOutputCP();
	if(code_page == 0)
	{
		return -2;
	}
	mb_size = WideCharToMultiByte(code_page, 0, str, -1, NULL, 0, NULL, NULL);
	if(mb_size == 0)
	{
		return -3;
	}
	mb_buf = malloc(mb_size);
	if(mb_buf == NULL)
	{
		return -4;
	}
	if(WideCharToMultiByte(code_page, 0, str, -1, mb_buf, mb_size, NULL, NULL) == 0)
	{
		free(mb_buf);
		return -5;
	}
	if(WriteFile(handle, mb_buf, mb_size - 1, &written, NULL) == 0)
	{
		free(mb_buf);
		return -6;
	}
	free(mb_buf);
	return written;
}

