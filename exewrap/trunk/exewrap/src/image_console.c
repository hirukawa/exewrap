#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <process.h>
#include <shlobj.h>
#include <stdio.h>
#include <jni.h>

#include "include/jvm.h"
#include "include/loader.h"
#include "include/notify.h"
#include "include/message.h"
#include "include/trace.h"

void    OutputConsole(BYTE* buf, DWORD len);
void    OutputMessage(const char* text);
UINT    UncaughtException(const char* thread, const char* message, const char* trace);

static HANDLE hConOut = NULL;


int main(int argc, char* argv[])
{
	int         err;
	char*       relative_classpath;
	char*       relative_extdirs;
	BOOL        use_server_vm;
	BOOL        use_side_by_side_jre;
	HANDLE      synchronize_mutex_handle = NULL;
	char*       ext_flags;
	char*       vm_args_opt;
	char        utilities[128];
	RESOURCE    res;
	LOAD_RESULT result;

	utilities[0] = '\0';
	#ifdef TRACE
	StartTrace(TRUE);
	strcat(utilities, UTIL_CONSOLE_OUTPUT_STREAM);
	#endif

	result.msg = malloc(2048);

	relative_classpath = (char*)GetResource("CLASS_PATH", NULL);
	relative_extdirs = (char*)GetResource("EXTDIRS", NULL);
	ext_flags = (char*)GetResource("EXTFLAGS", NULL);
	use_server_vm = (ext_flags != NULL && strstr(ext_flags, "SERVER") != NULL);
	use_side_by_side_jre = (ext_flags == NULL) || (strstr(ext_flags, "NOSIDEBYSIDE") == NULL);
	InitializePath(relative_classpath, relative_extdirs, use_server_vm, use_side_by_side_jre);

	if (ext_flags != NULL && strstr(ext_flags, "SHARE") != NULL)
	{
		synchronize_mutex_handle = NotifyExec(RemoteCallMainMethod, argc, argv);
		if (synchronize_mutex_handle == NULL)
		{
			result.msg_id = 0;
			goto EXIT;
		}
	}
	if (ext_flags != NULL && strstr(ext_flags, "SINGLE") != NULL)
	{
		if (CreateMutex(NULL, TRUE, GetModuleObjectName("SINGLE")), GetLastError() == ERROR_ALREADY_EXISTS)
		{
			result.msg_id = 0;
			goto EXIT;
		}
	}

	vm_args_opt = (char*)GetResource("VMARGS", NULL);
	CreateJavaVM(vm_args_opt, "exewrap.core.ExewrapClassLoader", use_server_vm, use_side_by_side_jre, &err);
	if (err != JNI_OK)
	{
		OutputMessage(GetJniErrorMessage(err, &result.msg_id, result.msg));
		goto EXIT;
	}

	if(GetResource("TARGET_VERSION", &res) != NULL)
	{
		DWORD  version = GetJavaRuntimeVersion();
		DWORD  targetVersion = *(DWORD*)res.buf;
		if (targetVersion > version)
		{
			char* targetVersionString = (char*)res.buf + 4;
			result.msg_id = MSG_ID_ERR_TARGET_VERSION;
			sprintf(result.msg, _(MSG_ID_ERR_TARGET_VERSION), targetVersionString);
			OutputMessage(result.msg);
			goto EXIT;
		}
	}

	if (ext_flags == NULL || strstr(ext_flags, "IGNORE_UNCAUGHT_EXCEPTION") == NULL)
	{
		strcat(utilities, UTIL_UNCAUGHT_EXCEPTION_HANDLER);
	}
	if (LoadMainClass(argc, argv, utilities, &result) == FALSE)
	{
		OutputMessage(result.msg);
		goto EXIT;
	}
	if (synchronize_mutex_handle != NULL)
	{
		ReleaseMutex(synchronize_mutex_handle);
		CloseHandle(synchronize_mutex_handle);
		synchronize_mutex_handle = NULL;
	}
	(*env)->CallStaticVoidMethod(env, result.MainClass, result.MainClass_main, result.MainClass_main_args);

	(*env)->ExceptionDescribe(env);
	(*env)->ExceptionClear(env);

EXIT:
	if (synchronize_mutex_handle != NULL)
	{
		ReleaseMutex(synchronize_mutex_handle);
		CloseHandle(synchronize_mutex_handle);
	}
	if (result.msg != NULL)
	{
		free(result.msg);
	}
	if (env != NULL)
	{
		DetachJavaVM();
	}
	if (jvm != NULL)
	{
		DestroyJavaVM();
	}

	NotifyClose();

	#ifdef TRACE
	StopTrace();
	#endif

	return result.msg_id;
}

void OutputConsole(BYTE* buf, DWORD len)
{
	DWORD written;

	if (hConOut == INVALID_HANDLE_VALUE)
	{
		return;
	}
	if (hConOut == NULL)
	{
		hConOut = CreateFile("CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (hConOut == INVALID_HANDLE_VALUE)
		{
			return;
		}
	}
	WriteConsole(hConOut, buf, len, &written, NULL);
}

void OutputMessage(const char* text)
{
	DWORD written;

	if (text == NULL)
	{
		return;
	}

	WriteConsole(GetStdHandle(STD_ERROR_HANDLE), text, (DWORD)strlen(text), &written, NULL); 
	WriteConsole(GetStdHandle(STD_ERROR_HANDLE), "\r\n", 2, &written, NULL);
}

UINT UncaughtException(const char* thread, const char* message, const char* trace)
{
	if (thread != NULL)
	{
		char* buf = malloc(32 + strlen(thread));
		sprintf(buf, "Exception in thread \"%s\"", thread);
		OutputMessage(buf);
		free(buf);
	}
	if (trace != NULL)
	{
		OutputMessage(trace);
	}
	return MSG_ID_ERR_UNCAUGHT_EXCEPTION;
}
