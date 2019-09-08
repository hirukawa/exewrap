#define _CRT_SECURE_NO_WARNINGS

#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#include <windows.h>
#include <process.h>
#include <shlobj.h>
#include <stdio.h>
#include <jni.h>

#include "include/jvm.h"
#include "include/loader.h"
#include "include/notify.h"
#include "include/message.h"

void OutputMessage(const char* text);
UINT UncaughtException(JNIEnv* env, const char* thread, const jthrowable throwable);

static char** get_args(int* argc);
static char*  _w2a(LPCWSTR s);

typedef void(*SplashInit_t)(void);
typedef int(*SplashLoadMemory_t)(void* pdata, int size);
SplashInit_t       SplashInit;
SplashLoadMemory_t SplashLoadMemory;
HMODULE            splashscreendll;
static HANDLE      hConOut = NULL;


INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
{
	int         argc;
	char**      argv;
	SYSTEMTIME  startup;
	int         err;
	char*       relative_classpath;
	char*       relative_extdirs;
	BOOL        use_server_vm;
	BOOL        use_side_by_side_jre;
	HANDLE      synchronize_mutex_handle = NULL;
	char*       ext_flags;
	BYTE*       splash_screen_image = NULL;
	char*       splash_screen_name = NULL;
	char*       vm_args_opt = NULL;
	char        utilities[128];
	RESOURCE    res;
	LOAD_RESULT result;

	GetSystemTime(&startup);

	utilities[0] = '\0';

	argv = get_args(&argc);
	result.msg = malloc(2048);

	relative_classpath = (char*)GetResource("CLASS_PATH", NULL);
	relative_extdirs = (char*)GetResource("EXTDIRS", NULL);
	ext_flags = (char*)GetResource("EXTFLAGS", NULL);
	use_server_vm = (ext_flags != NULL && strstr(ext_flags, "SERVER") != NULL);
	use_side_by_side_jre = (ext_flags == NULL) || (strstr(ext_flags, "NOSIDEBYSIDE") == NULL);
	InitializePath(relative_classpath, relative_extdirs, use_server_vm, use_side_by_side_jre, &startup);

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

	// Display Splash Screen
	if (GetResource("SPLASH_SCREEN_IMAGE", &res) != NULL)
	{
		splashscreendll = LoadLibrary("splashscreen.dll");
		if (splashscreendll != NULL)
		{
			SplashInit = (SplashInit_t)GetProcAddress(splashscreendll, "SplashInit");
			if (SplashInit != NULL)
			{
				SplashLoadMemory = (SplashLoadMemory_t)GetProcAddress(splashscreendll, "SplashLoadMemory");
				if (SplashLoadMemory != NULL)
				{
					SplashInit();
					SplashLoadMemory(res.buf, res.len);
				}
			}
		}
	}

	if (vm_args_opt == NULL)
	{
		vm_args_opt = (char*)GetResource("VMARGS", NULL);
	}
	CreateJavaVM(vm_args_opt, "Loader", use_server_vm, use_side_by_side_jre, &startup, &err);
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
	if (ext_flags == NULL || strstr(ext_flags, "NOLOG") == NULL)
	{
		strcat(utilities, UTIL_FILE_LOG_STREAM);
	}
	if(LoadMainClass(argc, argv, utilities, &result) == FALSE)
	{
		OutputMessage(result.msg);
		goto EXIT;
	}

	if (GetResource("SPLASH_SCREEN_IMAGE", &res) != NULL)
	{
		BYTE* splash_screen_image_buf = res.buf;
		DWORD splash_screen_image_len = res.len;
		char* splash_screen_name = GetResource("SPLASH_SCREEN_NAME", NULL);

		SetSplashScreenResource(splash_screen_name, splash_screen_image_buf, splash_screen_image_len);
	}

	if (synchronize_mutex_handle != NULL)
	{
		ReleaseMutex(synchronize_mutex_handle);
		CloseHandle(synchronize_mutex_handle);
		synchronize_mutex_handle = NULL;
	}
	(*env)->CallStaticVoidMethod(env, result.MainClass, result.MainClass_main, result.MainClass_main_args);

	if ((*env)->ExceptionCheck(env) == JNI_TRUE)
	{
		jthrowable throwable = (*env)->ExceptionOccurred(env);
		if (throwable != NULL)
		{
			UncaughtException(env, "main", throwable);
			(*env)->DeleteLocalRef(env, throwable);
		}
		(*env)->ExceptionClear(env);
	}

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

	return result.msg_id;
}

void OutputMessage(const char* text)
{
	char  buffer[MAX_PATH];
	char* filename;

	if (text == NULL)
	{
		return;
	}

	GetModuleFileName(NULL, buffer, MAX_PATH);
	filename = strrchr(buffer, '\\') + 1;

	MessageBox(NULL, text, filename, MB_ICONEXCLAMATION | MB_APPLMODAL | MB_OK | MB_SETFOREGROUND);
}

UINT UncaughtException(JNIEnv* env, const char* thread, const jthrowable throwable)
{
	jclass    System                    = NULL;
	jfieldID  System_err                = NULL;
	jobject   printStream               = NULL;
	jclass    PrintStream               = NULL;
	jmethodID printStream_print         = NULL;
	jmethodID printStream_flush         = NULL;
	jclass    Throwable                 = NULL;
	jmethodID throwable_printStackTrace = NULL;
	jmethodID throwable_getMessage      = NULL;
	jstring   leading                   = NULL;
	jstring   message                   = NULL;
	char*     buf                       = NULL;
	char*     sjis                      = NULL;

	if(thread == NULL || throwable == NULL)
	{
		goto EXIT;
	}

	buf = malloc(65536);
	if(buf == NULL)
	{
		goto EXIT;
	}
	
	System = (*env)->FindClass(env, "java/lang/System");
	if(System == NULL)
	{
		sprintf(buf, _(MSG_ID_ERR_FIND_CLASS), "java.lang.System");
		OutputMessage(buf);
		goto EXIT;
	}
	System_err = (*env)->GetStaticFieldID(env, System, "err", "Ljava/io/PrintStream;");
	if(System_err == NULL)
	{
		sprintf(buf, _(MSG_ID_ERR_GET_FIELD), "java.lang.System.err");
		OutputMessage(buf);
		goto EXIT;
	}
	printStream = (*env)->GetStaticObjectField(env, System, System_err);
	if(printStream == NULL)
	{
		sprintf(buf, _(MSG_ID_ERR_GET_FIELD), "java.lang.System.err");
		OutputMessage(buf);
		goto EXIT;
	}
	PrintStream = (*env)->FindClass(env, "java/io/PrintStream");
	if(PrintStream == NULL)
	{
		sprintf(buf, _(MSG_ID_ERR_FIND_CLASS), "java.io.PrintStream");
		OutputMessage(buf);
		goto EXIT;
	}
	printStream_print = (*env)->GetMethodID(env, PrintStream, "print", "(Ljava/lang/String;)V");
	if(printStream_print == NULL)
	{
		sprintf(buf, _(MSG_ID_ERR_GET_METHOD), "java.io.PrintStream.print(String)");
		OutputMessage(buf);
		goto EXIT;
	}
	printStream_flush = (*env)->GetMethodID(env, PrintStream, "flush", "()V");
	if(printStream_flush == NULL)
	{
		sprintf(buf, _(MSG_ID_ERR_GET_METHOD), "java.io.PrintStream.flush()");
		OutputMessage(buf);
		goto EXIT;
	}
	Throwable = (*env)->FindClass(env, "java/lang/Throwable");
	if(Throwable == NULL)
	{
		sprintf(buf, _(MSG_ID_ERR_FIND_CLASS), "java.lang.Throwable");
		OutputMessage(buf);
		goto EXIT;
	}
	throwable_printStackTrace = (*env)->GetMethodID(env, Throwable, "printStackTrace", "()V");
	if(throwable_printStackTrace == NULL)
	{
		sprintf(buf, _(MSG_ID_ERR_GET_METHOD), "java.lang.Throwable.printStackTrace()");
		OutputMessage(buf);
		goto EXIT;
	}
	throwable_getMessage = (*env)->GetMethodID(env, Throwable, "getMessage", "()Ljava/lang/String;");
	if(throwable_getMessage == NULL)
	{
		sprintf(buf, _(MSG_ID_ERR_GET_METHOD), "java.lang.Throwable.getMessage()");
		OutputMessage(buf);
		goto EXIT;
	}

	strcpy(buf, "Exception in thread \"");
	strcat(buf, thread);
	strcat(buf, "\" ");
	leading = GetJString(env, buf);

	(*env)->CallObjectMethod(env, printStream, printStream_print, leading);
	(*env)->CallObjectMethod(env, throwable, throwable_printStackTrace);
	(*env)->CallObjectMethod(env, printStream, printStream_flush);
	
	message = (*env)->CallObjectMethod(env, throwable, throwable_getMessage);
	if(message != NULL)
	{
		sjis = GetShiftJIS(env, message);
	}
	sprintf(buf, "Exception in thread \"%s\"\r\n%s", thread, (sjis != NULL) ? sjis : "");
	OutputMessage(buf);
	
EXIT:
	if(sjis != NULL)
	{
		HeapFree(GetProcessHeap(), 0, sjis);
		sjis = NULL;
	}
	if(message != NULL)
	{
		(*env)->DeleteLocalRef(env, message);
		message = NULL;
	}
	if(buf != NULL)
	{
		free(buf);
		buf = NULL;
	}
	if(leading != NULL)
	{
		(*env)->DeleteLocalRef(env, leading);
		leading = NULL;
	}
	if(Throwable != NULL)
	{
		(*env)->DeleteLocalRef(env, Throwable);
		Throwable = NULL;
	}
	if(PrintStream != NULL)
	{
		(*env)->DeleteLocalRef(env, PrintStream);
		PrintStream = NULL;
	}
	if(printStream != NULL)
	{
		(*env)->DeleteLocalRef(env, printStream);
		printStream = NULL;
	}
	if(System != NULL)
	{
		(*env)->DeleteLocalRef(env, System);
		System = NULL;
	}

	return MSG_ID_ERR_UNCAUGHT_EXCEPTION;
}

static char** get_args(int* argc)
{
	LPWSTR  lpCmdLineW;
	LPWSTR* argvW;
	LPSTR*  argvA;
	int     i;
	int     ret = 0;

	lpCmdLineW = GetCommandLineW();
	argvW = CommandLineToArgvW(lpCmdLineW, argc);
	argvA = (LPSTR*)HeapAlloc(GetProcessHeap(), 0, (*argc + 1) * sizeof(LPSTR));
	for (i = 0; i < *argc; i++)
	{
		argvA[i] = _w2a(argvW[i]);
	}
	argvA[*argc] = NULL;

	return argvA;
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
	buf = (LPSTR)HeapAlloc(GetProcessHeap(), 0, ret + 1);
	ret = WideCharToMultiByte(CP_ACP, 0, s, -1, buf, (ret + 1), NULL, NULL);
	if (ret == 0)
	{
		HeapFree(GetProcessHeap(), 0, buf);
		return NULL;
	}
	buf[ret] = '\0';

	return buf;
}
