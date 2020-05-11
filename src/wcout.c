#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

int      wcout(const wchar_t* str);
int      wcerr(const wchar_t* str);
int      wcoutf(const wchar_t* format, ...);
int      wcerrf(const wchar_t* format, ...);
wchar_t* get_error_message(DWORD last_error);

static int write_to_handle(DWORD nStdHandle, const wchar_t* str);
static int write_to_handle_va_list(DWORD nStdHandle, const wchar_t* format, va_list argptr);

int wcout(const wchar_t* str)
{
	return write_to_handle(STD_OUTPUT_HANDLE, str);
}

int wcerr(const wchar_t* str)
{
	return write_to_handle(STD_ERROR_HANDLE, str);
}

int wcoutf(const wchar_t* format, ...)
{
	va_list  argptr;
	int ret;

	va_start(argptr, format);
	ret = write_to_handle_va_list(STD_OUTPUT_HANDLE, format, argptr);
	va_end(argptr);
	return ret;
}

int wcerrf(const wchar_t* format, ...)
{
	va_list  argptr;
	int ret;

	va_start(argptr, format);
	ret = write_to_handle_va_list(STD_ERROR_HANDLE, format, argptr);
	va_end(argptr);
	return ret;
}

static int write_to_handle(DWORD nStdHandle, const wchar_t* str)
{
	HANDLE handle;
	UINT   code_page;
	int    mb_size;
	char*  mb_buf;
	DWORD  written;

	if(str == NULL)
	{
		return 0;
	}
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

static int write_to_handle_va_list(DWORD nStdHandle, const wchar_t* format, va_list argptr)
{
	int      len = 8192;
	wchar_t* buf = NULL;
	int      ret;

	for(;;) {
		buf = (wchar_t*)malloc(len * sizeof(wchar_t));
		if(buf == NULL)
		{
			return -1;
		}
		ret = vswprintf_s(buf, len, format, argptr);
		if(ret >= 0)
		{
			write_to_handle(nStdHandle, buf);
		}
		free(buf);

		if(ret >= 0 || len >= 1048576)
		{
			break;
		}
		len *= 2;
	}
	return ret;
}

wchar_t* get_error_message(DWORD last_error)
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

