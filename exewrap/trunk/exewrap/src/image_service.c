#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <process.h>
#include <shlobj.h>
#include <stdio.h>
#include <jni.h>

#include "include/jvm.h"
#include "include/loader.h"
#include "include/message.h"
#include "include/eventlog.h"

#define RUN_AS_ADMINISTRATOR_ARG "__ruN_aA_administratoR__"
#define RUN_AS_ADMINISTRATOR       1
#define SERVICE_INSTALL            2
#define SERVICE_START_IMMEDIATELY  4
#define SERVICE_REMOVE             8
#define SERVICE_STOP_BEFORE_REMOVE 16
#define SERVICE_START_BY_SCM       32
#define SHOW_HELP_MESSAGE          64


void    OutputMessage(const char* text);
UINT    UncaughtException(const char* thread, const char* message, const char* trace);

static int         install_service(char* service_name, int argc, char* argv[], int opt_end);
static int         set_service_description(char* service_name, char* description);
static int         remove_service(char* service_name);
static int         start_service(char* service_name);
static int         stop_service(char* service_name);
static void        start_service_main();
static void        stop_service_main();
static BOOL WINAPI console_control_handler(DWORD dwCtrlType);
static void        service_control_handler(DWORD request);
static int         service_main(int argc, char* argv[]);
static int         parse_args(int* argc_ptr, char* argv[], int* opt_end);
static char**      parse_opt(int argc, char* argv[]);
static void        show_help_message();
static void        set_current_dir();
static char*       get_service_name(char* buf);
static char*       get_pipe_name(char* buf);
static int         run_as_administrator(HANDLE pipe, int argc, char* argv[], char* append);

static SERVICE_STATUS service_status = { 0 };
static SERVICE_STATUS_HANDLE hStatus;
static int       flags;
static int       ARG_COUNT;
static char**    ARG_VALUE;
static jclass    MainClass;
static jmethodID MainClass_stop;


static int service_main(int argc, char* argv[])
{
	int          err;
	char*        service_name = NULL;
	BOOL         is_service;
	char*        relative_classpath;
	char*        relative_extdirs;
	BOOL         use_server_vm;
	char*        ext_flags;
	char*        vm_args_opt;
	char         utilities[128];
	RESOURCE     res;
	LOAD_RESULT  result;
	jmethodID    MainClass_start;
	jobjectArray MainClass_start_args;
	int          i;

	service_name = get_service_name(NULL);
	is_service = (flags & SERVICE_START_BY_SCM);

	result.msg = malloc(2048);

	relative_classpath = (char*)GetResource("CLASS_PATH", NULL);
	relative_extdirs = (char*)GetResource("EXTDIRS", NULL);
	ext_flags = (char*)GetResource("EXTFLAGS", NULL);
	use_server_vm = (ext_flags != NULL && strstr(ext_flags, "SERVER") != NULL);
	InitializePath(relative_classpath, relative_extdirs, use_server_vm);

	vm_args_opt = (char*)GetResource("VMARGS", NULL);
	CreateJavaVM(vm_args_opt, use_server_vm, &err);
	if (err != JNI_OK)
	{
		OutputMessage(GetWinErrorMessage(err, &result.msg_id, result.msg));
		goto EXIT;
	}

	if (GetResource("TARGET_VERSION", &res) != NULL)
	{
		DWORD  version = GetJavaRuntimeVersion();
		DWORD  targetVersion = *(DWORD*)res.buf;
		if (targetVersion > version)
		{
			char* targetVersionString = (char*)res.buf + 4;
			result.msg_id = MSG_ID_ERR_TARGET_VERSION;
			sprintf(result.msg, _(MSG_ID_ERR_TARGET_VERSION), targetVersionString + 4);
			OutputMessage(result.msg);
			goto EXIT;
		}
	}

	utilities[0] = '\0';
	if (ext_flags == NULL || strstr(ext_flags, "IGNORE_UNCAUGHT_EXCEPTION") == NULL)
	{
		strcat(utilities, UTIL_UNCAUGHT_EXCEPTION_HANDLER);
	}
	if (is_service)
	{
		if (ext_flags == NULL || strstr(ext_flags, "NOLOG") == NULL)
		{
			strcat(utilities, UTIL_EVENT_LOG_STREAM);
		}
		strcat(utilities, UTIL_EVENT_LOG_HANDLER);
	}

	if (LoadMainClass(argc, argv, utilities, &result) == FALSE)
	{
		OutputMessage(result.msg);
		goto EXIT;
	}
	MainClass = result.MainClass;

	MainClass_start = (*env)->GetStaticMethodID(env, result.MainClass, "start", "([Ljava/lang/String;)V");
	if (MainClass_start == NULL)
	{
		result.msg_id = MSG_ID_ERR_FIND_METHOD_SERVICE_START;
		strcpy(result.msg, _(MSG_ID_ERR_FIND_METHOD_SERVICE_START));
		OutputMessage(result.msg);
		goto EXIT;
	}
	if (is_service && argc > 2)
	{
		MainClass_start_args = (*env)->NewObjectArray(env, argc - 2, (*env)->FindClass(env, "java/lang/String"), NULL);
		for (i = 2; i < argc; i++)
		{
			(*env)->SetObjectArrayElement(env, MainClass_start_args, (i - 2), GetJString(env, argv[i]));
		}
	}
	else if (!is_service && argc > 1)
	{
		MainClass_start_args = (*env)->NewObjectArray(env, argc - 1, (*env)->FindClass(env, "java/lang/String"), NULL);
		for (i = 1; i < argc; i++)
		{
			(*env)->SetObjectArrayElement(env, MainClass_start_args, (i - 1), GetJString(env, argv[i]));
		}
	}
	else
	{
		MainClass_start_args = (*env)->NewObjectArray(env, 0, (*env)->FindClass(env, "java/lang/String"), NULL);
	}
	MainClass_stop = (*env)->GetStaticMethodID(env, result.MainClass, "stop", "()V");
	if (MainClass_stop == NULL)
	{
		result.msg_id = MSG_ID_ERR_FIND_METHOD_SERVICE_STOP;
		strcpy(result.msg, _(MSG_ID_ERR_FIND_METHOD_SERVICE_STOP));
		OutputMessage(result.msg);
		goto EXIT;
	}

	sprintf(result.msg, _(MSG_ID_SUCCESS_SERVICE_START), service_name);
	if (is_service)
	{
		WriteEventLog(EVENTLOG_INFORMATION_TYPE, result.msg);
	}
	else
	{
		OutputMessage(result.msg);
	}

	// JavaVM が CTRL_SHUTDOWN_EVENT を受け取って終了してしまわないように、ハンドラを登録して先取りします。
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)console_control_handler, TRUE);
	// シャットダウン時にダイアログが表示されないようにします。
	SetProcessShutdownParameters(0x4FF, SHUTDOWN_NORETRY);

	(*env)->CallStaticVoidMethod(env, result.MainClass, MainClass_start, MainClass_start_args);

	if ((*env)->ExceptionCheck(env) == JNI_TRUE)
	{
		jthrowable throwable = (*env)->ExceptionOccurred(env);
		if (throwable != NULL)
		{
			ToString(env, throwable, result.msg);
			if (is_service)
			{
				WriteEventLog(EVENTLOG_ERROR_TYPE, result.msg);
			}
			else
			{
				OutputMessage(result.msg);
			}
			(*env)->DeleteLocalRef(env, throwable);
		}
		(*env)->ExceptionClear(env);
	}
	else
	{
		sprintf(result.msg, _(MSG_ID_SUCCESS_SERVICE_STOP), service_name);
		if (is_service)
		{
			WriteEventLog(EVENTLOG_INFORMATION_TYPE, result.msg);
		}
		else
		{
			OutputMessage(result.msg);
		}
	}

EXIT:
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
		//デーモンではないスレッド(たとえばSwing)が残っていると待機状態になってしまうため、
		//サービスでは、DestroyJavaVM() を実行しないようにしています。
		if (!is_service)
		{
			DestroyJavaVM();
		}
	}

	return result.msg_id;
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


int main(int argc, char* argv[])
{
	char*  service_name = NULL;
	int    opt_end;
	char*  pipe_name = NULL;
	HANDLE pipe = NULL;
	int    exit_code = 0;

	set_current_dir();
	service_name = get_service_name(NULL);
	flags = parse_args(&argc, argv, &opt_end);

	if (flags & SHOW_HELP_MESSAGE)
	{
		show_help_message();
		goto EXIT;
	}

	if (flags & SERVICE_START_BY_SCM)
	{
		SERVICE_TABLE_ENTRY ServiceTable[2];

		ARG_COUNT = argc;
		ARG_VALUE = argv;

		ServiceTable[0].lpServiceName = service_name;
		ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)start_service_main;
		ServiceTable[1].lpServiceName = NULL;
		ServiceTable[1].lpServiceProc = NULL;
		StartServiceCtrlDispatcher(ServiceTable);
		goto EXIT;
	}

	if (!(flags & RUN_AS_ADMINISTRATOR) && (flags & (SERVICE_INSTALL | SERVICE_REMOVE)))
	{
		pipe_name = get_pipe_name(NULL);
		pipe = CreateNamedPipe(pipe_name, PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_WAIT, 2, 1024, 1024, 1000, NULL);
		if (pipe == INVALID_HANDLE_VALUE)
		{
			OutputMessage(GetWinErrorMessage(GetLastError(), &exit_code, NULL));
			goto EXIT;
		}
		exit_code = run_as_administrator(pipe, argc, argv, RUN_AS_ADMINISTRATOR_ARG);
		goto EXIT;
	}

	if (flags & RUN_AS_ADMINISTRATOR)
	{
		pipe_name = get_pipe_name(NULL);
		pipe = CreateFile(pipe_name, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (pipe == INVALID_HANDLE_VALUE)
		{
			OutputMessage(GetWinErrorMessage(GetLastError(), &exit_code, NULL));
			goto EXIT;
		}
		SetStdHandle(STD_OUTPUT_HANDLE, pipe);
	}

	if (flags & SERVICE_INSTALL)
	{
		exit_code = install_service(service_name, argc, argv, opt_end);
		if (exit_code == 0)
		{
			if (flags & SERVICE_START_IMMEDIATELY)
			{
				exit_code = start_service(service_name);
			}
		}
		return exit_code;
	}
	if (flags & SERVICE_REMOVE)
	{
		if (flags & SERVICE_STOP_BEFORE_REMOVE)
		{
			exit_code = stop_service(service_name);
			if (exit_code != 0)
			{
				return exit_code;
			}
			Sleep(500);
		}
		exit_code = remove_service(service_name);
		return exit_code;
	}

	exit_code = service_main(argc, argv);

EXIT:
	if (pipe != NULL && pipe != INVALID_HANDLE_VALUE)
	{
		CloseHandle(pipe);
	}
	if (pipe_name != NULL)
	{
		HeapFree(GetProcessHeap(), 0, pipe_name);
	}
	if (service_name != NULL)
	{
		HeapFree(GetProcessHeap(), 0, service_name);
	}

	return exit_code;
}


static int install_service(char* service_name, int argc, char* argv[], int opt_end)
{
	int       PATH_SIZE = 1024;
	char*     path = NULL;
	int       i;
	char**    opt = NULL;
	char*     lpDisplayName = NULL;
	DWORD     dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	DWORD     dwStartType = SERVICE_AUTO_START;
	char*     lpDependencies = NULL;
	char*     lpServiceStartName = NULL;
	char*     lpPassword = NULL;
	SC_HANDLE hSCManager = NULL;
	SC_HANDLE hService = NULL;
	char*     buf = NULL;
	DWORD     size;
	int       err = 0;

	buf = (char*)malloc(2048);
	path = (char*)malloc(PATH_SIZE);
	path[0] = '"';
	GetModuleFileName(NULL, &path[1], PATH_SIZE - 1);
	strcat(path, "\" -service");
	for (i = opt_end + 2; i < argc; i++)
	{
		strcat(path, " \"");
		strcat(path, argv[i]);
		strcat(path, "\"");
	}

	opt = parse_opt(opt_end + 1, argv);
	if (opt['n'])
	{
		lpDisplayName = opt['n'];
	}
	if (opt['i'] && opt['u'] == 0 && opt['p'] == 0)
	{
		dwServiceType += SERVICE_INTERACTIVE_PROCESS;
	}
	if (opt['m'])
	{
		dwStartType = SERVICE_DEMAND_START;
	}
	if (opt['d'])
	{
		lpDependencies = (char*)malloc(strlen(opt['d']) + 2);
		lstrcpy(lpDependencies, opt['d']);
		lstrcat(lpDependencies, ";");
		while (strrchr(lpDependencies, ';') != NULL)
		{
			*(strrchr(lpDependencies, ';')) = '\0';
		}
	}
	if (opt['u'])
	{
		lpServiceStartName = opt['u'];
	}
	if (opt['p'])
	{
		lpPassword = opt['p'];
	}

	hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hSCManager == NULL)
	{
		GetWinErrorMessage(GetLastError(), &err, buf);
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);
		goto EXIT;
	}

	hService = CreateService(hSCManager, service_name, lpDisplayName, SERVICE_ALL_ACCESS, dwServiceType, dwStartType, SERVICE_ERROR_NORMAL, path, NULL, NULL, lpDependencies, lpServiceStartName, lpPassword);
	if (hService == NULL)
	{
		GetWinErrorMessage(GetLastError(), &err, buf);
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);
		goto EXIT;
	}
	else
	{
		sprintf(buf, _(MSG_ID_SUCCESS_SERVICE_INSTALL), service_name);
		strcat(buf, "\n");
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);
	}
	err = set_service_description(service_name, GetResource("SVCDESC", NULL));
	if (err != 0)
	{
		goto EXIT;
	}
	err = InstallEventLog();
	if (err != 0)
	{
		goto EXIT;
	}

EXIT:
	if (buf != NULL)
	{
		free(buf);
	}
	if (hService != NULL)
	{
		CloseServiceHandle(hService);
	}
	if (hSCManager != NULL)
	{
		CloseServiceHandle(hSCManager);
	}
	if (lpDependencies != NULL)
	{
		free(lpDependencies);
	}
	if (path != NULL)
	{
		free(path);
	}

	return err;
}


static int set_service_description(char* service_name, char* description)
{
	char* buf = NULL;
	DWORD size;
	char* key = NULL;
	HKEY  hKey = NULL;
	int   err = 0;

	if (description == NULL)
	{
		goto EXIT;
	}

	buf = (char*)malloc(2048);
	key = (char*)malloc(1024);
	strcpy(key, "SYSTEM\\CurrentControlSet\\Services\\");
	strcat(key, service_name);

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS)
	{
		GetWinErrorMessage(GetLastError(), &err, buf);
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);
		goto EXIT;
	}
	if (RegSetValueEx(hKey, "Description", 0, REG_SZ, (LPBYTE)description, (DWORD)strlen(description) + 1) != ERROR_SUCCESS)
	{
		GetWinErrorMessage(GetLastError(), &err, buf);
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);
		goto EXIT;
	}

EXIT:
	if (hKey != NULL)
	{
		RegCloseKey(hKey);
	}
	if (key != NULL)
	{
		free(key);
	}
	if (buf != NULL)
	{
		free(buf);
	}

	return err;
}


static int remove_service(char* service_name)
{
	char*          buf = NULL;
	DWORD          size;
	SC_HANDLE      hSCManager = NULL;
	SC_HANDLE      hService = NULL;
	SERVICE_STATUS status;
	BOOL           ret;
	int            err = 0;

	buf = (char*)malloc(2048);

	hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hSCManager == NULL)
	{
		GetWinErrorMessage(GetLastError(), &err, buf);
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);
		goto EXIT;
	}
	hService = OpenService(hSCManager, service_name, SERVICE_ALL_ACCESS | DELETE);
	if (hService == NULL)
	{
		GetWinErrorMessage(GetLastError(), &err, buf);
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);
		goto EXIT;
	}

	ret = QueryServiceStatus(hService, &status);
	if (ret == FALSE)
	{
		GetWinErrorMessage(GetLastError(), &err, buf);
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);
		goto EXIT;
	}
	if (status.dwCurrentState != SERVICE_STOPPED)
	{
		err = MSG_ID_ERR_SERVICE_NOT_STOPPED;
		sprintf(buf, _(MSG_ID_ERR_SERVICE_NOT_STOPPED), service_name);
		strcat(buf, "\n");
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);
		goto EXIT;
	}

	if (!DeleteService(hService))
	{
		GetWinErrorMessage(GetLastError(), &err, buf);
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);
		goto EXIT;
	}

	sprintf(buf, _(MSG_ID_SUCCESS_SERVICE_REMOVE), service_name);
	strcat(buf, "\n");
	WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);

	err = RemoveEventLog();
	if (err != 0)
	{
		goto EXIT;
	}

EXIT:
	if (hService != NULL)
	{
		CloseServiceHandle(hService);
	}
	if (hSCManager != NULL)
	{
		CloseServiceHandle(hSCManager);
	}
	if (service_name != NULL)
	{
		HeapFree(GetProcessHeap(), 0, service_name);
	}
	if (buf != NULL)
	{
		free(buf);
	}

	return err;
}


int start_service(char* service_name)
{
	SC_HANDLE hSCManager = NULL;
	SC_HANDLE hService = NULL;
	BOOL  ret;
	char* buf = NULL;
	DWORD size;
	int   err = 0;

	buf = (char*)malloc(2048);

	hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hSCManager == NULL)
	{
		GetWinErrorMessage(GetLastError(), &err, buf);
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);
		goto EXIT;
	}
	hService = OpenService(hSCManager, service_name, SERVICE_START);
	if(hService == NULL)
	{
		GetWinErrorMessage(GetLastError(), &err, buf);
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);
		goto EXIT;
	}
	ret = StartService(hService, 0, NULL);
	if (ret == FALSE)
	{
		GetWinErrorMessage(GetLastError(), &err, buf);
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);
		goto EXIT;
	}

	sprintf(buf, _(MSG_ID_SUCCESS_SERVICE_START), service_name);
	strcat(buf, "\n");
	WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);

EXIT:
	if (buf != NULL)
	{
		free(buf);
	}
	if (hService != NULL)
	{
		CloseServiceHandle(hService);
	}
	if (hSCManager != NULL)
	{
		CloseServiceHandle(hSCManager);
	}

	return err;
}


static int stop_service(char* service_name)
{
	SC_HANDLE hSCManager = NULL;
	SC_HANDLE hService = NULL;
	SERVICE_STATUS status;
	BOOL  ret;
	char* buf = NULL;
	DWORD size;
	int   i;
	int   err = 0;

	buf = (char*)malloc(2048);

	hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hSCManager == NULL)
	{
		GetWinErrorMessage(GetLastError(), &err, buf);
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);
		goto EXIT;
	}
	hService = OpenService(hSCManager, service_name, SERVICE_ALL_ACCESS);
	if (hService == NULL)
	{
		GetWinErrorMessage(GetLastError(), &err, buf);
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);
		goto EXIT;
	}
	ret = QueryServiceStatus(hService, &status);
	if (ret == FALSE)
	{
		GetWinErrorMessage(GetLastError(), &err, buf);
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);
		goto EXIT;
	}
	if (status.dwCurrentState != SERVICE_STOPPED && status.dwCurrentState != SERVICE_STOP_PENDING)
	{
		if (ControlService(hService, SERVICE_CONTROL_STOP, &status) == 0)
		{
			GetWinErrorMessage(GetLastError(), &err, buf);
			WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);
			goto EXIT;
		}
		buf = (char*)malloc(1024);
		sprintf(buf, _(MSG_ID_SERVICE_STOPING), service_name);
		strcat(buf, "\n");
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);
		for (i = 0; i < 240; i++)
		{
			if (QueryServiceStatus(hService, &status) == 0)
			{
				GetWinErrorMessage(GetLastError(), &err, buf);
				WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);
				goto EXIT;
			}
			if (status.dwCurrentState == SERVICE_STOPPED)
			{
				sprintf(buf, _(MSG_ID_SUCCESS_SERVICE_STOP), service_name);
				strcat(buf, "\n");
				WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &size, NULL);
				break;
			}
			Sleep(500);
		}
	}

EXIT:
	if (buf != NULL)
	{
		free(buf);
	}
	if (hService != NULL)
	{
		CloseServiceHandle(hService);
	}
	if (hSCManager != NULL)
	{
		CloseServiceHandle(hSCManager);
	}

	return err;
}


static void start_service_main()
{
	char* service_name = get_service_name(NULL);

	service_status.dwServiceType = SERVICE_WIN32;
	service_status.dwCurrentState = SERVICE_START_PENDING;
	service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	service_status.dwWin32ExitCode = 0;
	service_status.dwServiceSpecificExitCode = 0;
	service_status.dwCheckPoint = 0;
	service_status.dwWaitHint = 0;

	hStatus = RegisterServiceCtrlHandler(service_name, (LPHANDLER_FUNCTION)service_control_handler);
	if (hStatus == (SERVICE_STATUS_HANDLE)0)
	{
		// Registering Control Handler failed.
		goto EXIT;
	}

	service_status.dwCurrentState = SERVICE_RUNNING;
	SetServiceStatus(hStatus, &service_status);

	ARG_VALUE[0] = service_name;
	service_status.dwServiceSpecificExitCode = service_main(ARG_COUNT, ARG_VALUE);
	service_status.dwWin32ExitCode = NO_ERROR;

	service_status.dwCurrentState = SERVICE_STOPPED;
	service_status.dwCheckPoint++;
	service_status.dwWaitHint = 0;
	SetServiceStatus(hStatus, &service_status);

EXIT:
	if (service_name != NULL)
	{
		HeapFree(GetProcessHeap(), 0, service_name);
	}
}


static void stop_service_main()
{
	BOOL      is_service = (flags & SERVICE_START_BY_SCM);
	JNIEnv*   env = AttachJavaVM();
	char*     buf = NULL;

	(*env)->CallStaticVoidMethod(env, MainClass, MainClass_stop);

	if ((*env)->ExceptionCheck(env) == JNI_TRUE)
	{
		jthrowable throwable = (*env)->ExceptionOccurred(env);
		if (throwable != NULL)
		{
			buf = malloc(2048);
			ToString(env, throwable, buf);
			if (is_service)
			{
				WriteEventLog(EVENTLOG_ERROR_TYPE, buf);
			}
			else
			{
				OutputMessage(buf);
			}
			(*env)->DeleteLocalRef(env, throwable);
		}
		(*env)->ExceptionClear(env);
	}

	DetachJavaVM();

	if (buf != NULL)
	{
		free(buf);
	}
}


static BOOL WINAPI console_control_handler(DWORD dwCtrlType)
{
	static int ctrl_c = 0;

	switch (dwCtrlType)
	{
	case CTRL_C_EVENT:
		if (ctrl_c++ == 0)
		{
			//初回は終了処理を試みます。
			printf(_(MSG_ID_CTRL_SERVICE_STOP), "CTRL_C");
			printf("\r\n");
			stop_service_main();
			return TRUE;
		}
		else
		{
			printf(_(MSG_ID_CTRL_SERVICE_TERMINATE), "CTRL_C");
			printf("\r\n");
			return FALSE;
		}

	case CTRL_BREAK_EVENT:
		printf(_(MSG_ID_CTRL_BREAK), "CTRL_BREAK");
		printf("\r\n");
		return FALSE;

	case CTRL_CLOSE_EVENT:
		printf(_(MSG_ID_CTRL_SERVICE_STOP), "CTRL_CLOSE");
		printf("\r\n");
		stop_service_main();
		return TRUE;

	case CTRL_LOGOFF_EVENT:
		if (!(flags & SERVICE_START_BY_SCM))
		{
			printf(_(MSG_ID_CTRL_SERVICE_STOP), "CTRL_LOGOFF");
			printf("\r\n");
			stop_service_main();
		}
		return TRUE;

	case CTRL_SHUTDOWN_EVENT:
		if (!(flags & SERVICE_START_BY_SCM))
		{
			printf(_(MSG_ID_CTRL_SERVICE_STOP), "CTRL_SHUTDOWN");
			printf("\r\n");
			stop_service_main();
		}
		return TRUE;
	}
	return FALSE;
}


static void service_control_handler(DWORD request)
{
	switch (request) {
	case SERVICE_CONTROL_STOP:
	case SERVICE_CONTROL_SHUTDOWN:
		if (service_status.dwCurrentState == SERVICE_RUNNING)
		{
			service_status.dwWin32ExitCode = 0;
			service_status.dwCurrentState = SERVICE_STOP_PENDING;
			service_status.dwCheckPoint = 0;
			service_status.dwWaitHint = 2000;
			SetServiceStatus(hStatus, &service_status);
			stop_service_main();

			return;
		}
		else
		{
			service_status.dwCheckPoint++;
			SetServiceStatus(hStatus, &service_status);

			return;
		}
	}
	SetServiceStatus(hStatus, &service_status);
}


static int parse_args(int* argc_ptr, char* argv[], int* opt_end)
{
	int  argc = *argc_ptr;
	int  flags = 0;
	int  i;
	BOOL has_opt_s = FALSE;

	*opt_end = 0;

	if (strcmp(argv[argc - 1], RUN_AS_ADMINISTRATOR_ARG) == 0)
	{
		flags |= RUN_AS_ADMINISTRATOR;
		*argc_ptr = --argc;
	}
	if ((argc >= 2) && (strcmp(argv[1], "-service") == 0))
	{
		flags |= SERVICE_START_BY_SCM;
	}
	for (i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-s") == 0)
		{
			has_opt_s = TRUE;
		}
		if (strcmp(argv[i], "-install") == 0)
		{
			flags |= SERVICE_INSTALL;
			if (has_opt_s)
			{
				flags |= SERVICE_START_IMMEDIATELY;
			}
			*opt_end = i - 1;
			break;
		}
		if (strcmp(argv[i], "-remove") == 0)
		{
			flags |= SERVICE_REMOVE;
			if (has_opt_s)
			{
				flags |= SERVICE_STOP_BEFORE_REMOVE;
			}
			*opt_end = i - 1;
			break;
		}
	}
	if ((argc == 2) && ((strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "-?") == 0)))
	{
		flags |= SHOW_HELP_MESSAGE;
	}

	return flags;
}


static char** parse_opt(int argc, char* argv[])
{
	int i;
	char** opt = (char**)HeapAlloc(GetProcessHeap(), 0, 256 * 8);

	SecureZeroMemory(opt, 256 * 8);

	if ((argc > 1) && (*argv[1] != '-'))
	{
		opt[0] = argv[1];
	}
	for (i = 0; i < argc; i++)
	{
		if (*argv[i] == '-')
		{
			if (argv[i + 1] == NULL || *argv[i + 1] == '-')
			{
				opt[*(argv[i] + 1)] = "";
			}
			else
			{
				opt[*(argv[i] + 1)] = argv[i + 1];
			}
		}
	}
	if ((opt[0] == NULL) && (*argv[argc - 1] != '-'))
	{
		opt[0] = argv[argc - 1];
	}
	return opt;
}


static void show_help_message()
{
	char* buf; 
	char* name;

	buf = malloc(1024);
	GetModuleFileName(NULL, buf, 1024);
	name = strrchr(buf, '\\') + 1;

	printf("Usage:\r\n"
		"  %s [install-options] -install [runtime-arguments]\r\n"
		"  %s [remove-options] -remove\r\n"
		"  %s [runtime-arguments]\r\n"
		"\r\n"
		"Install Options:\r\n"
		"  -n <display-name>\t set service display name.\r\n"
		"  -i               \t allow interactive.\r\n"
		"  -m               \t \r\n"
		"  -d <dependencies>\t \r\n"
		"  -u <username>    \t \r\n"
		"  -p <password>    \t \r\n"
		"  -s               \t start service.\r\n"
		"\r\n"
		"Remove Options:\r\n"
		"  -s               \t stop service.\r\n"
		, name, name, name);
}


static void set_current_dir()
{
	char* b = (char*)HeapAlloc(GetProcessHeap(), 0, 1024);

	GetModuleFileName(NULL, b, 1024);
	*(strrchr(b, '\\')) = '\0';
	SetCurrentDirectory(b);

	HeapFree(GetProcessHeap(), 0, b);
}


static char* get_service_name(char* buf)
{
	char* b = (char*)HeapAlloc(GetProcessHeap(), 0, 1024);
	char* name;

	GetModuleFileName(NULL, b, 1024);
	*(strrchr(b, '.')) = '\0';
	name = strrchr(b, '\\') + 1;

	if (buf == NULL)
	{
		buf = (char*)HeapAlloc(GetProcessHeap(), 0, strlen(name) + 1);
	}
	strcpy(buf, name);
	HeapFree(GetProcessHeap(), 0, b);
	return buf;
}


static char* get_pipe_name(char* buf)
{
	char* b = (char*)HeapAlloc(GetProcessHeap(), 0, 1024);
	char* name;

	GetModuleFileName(NULL, b, 1024);
	name = strrchr(b, '\\') + 1;

	if (buf == NULL)
	{
		buf = (char*)HeapAlloc(GetProcessHeap(), 0, strlen(name) + 16);
	}
	sprintf(buf, "\\\\.\\pipe\\%s.pipe", name);
	HeapFree(GetProcessHeap(), 0, b);

	return buf;
}


static int run_as_administrator(HANDLE pipe, int argc, char* argv[], char* append)
{
	char*  module = NULL;
	char*  params = NULL;
	int    i;
	char*  buf = NULL;
	BOOL   ret;
	int    exit_code = 0;
	SHELLEXECUTEINFO si;

	module = malloc(1024);
	GetModuleFileName(NULL, module, 1024);

	params = malloc(2048);
	params[0] = '\0';
	for (i = 1; i < argc; i++)
	{
		strcat(params, "\"");
		strcat(params, argv[i]);
		strcat(params, "\" ");
	}
	strcat(params, append);

	ZeroMemory(&si, sizeof(SHELLEXECUTEINFO));
	si.cbSize = sizeof(SHELLEXECUTEINFO);
	si.fMask = SEE_MASK_NOCLOSEPROCESS;
	si.hwnd = GetActiveWindow();
	si.lpVerb = "runas";
	si.lpFile = module;
	si.lpParameters = params;
	si.lpDirectory = NULL;
	si.nShow = SW_HIDE;
	si.hInstApp = 0;
	si.hProcess = 0;

	ret = ShellExecuteEx(&si);
	if (GetLastError() == ERROR_CANCELLED)
	{
		OutputMessage(GetWinErrorMessage(GetLastError(), &exit_code, NULL));
		goto EXIT;
	}
	else if (ret == TRUE)
	{
		buf = (char*)HeapAlloc(GetProcessHeap(), 0, 1024);
		DWORD read_size;
		DWORD write_size;

		if (!ConnectNamedPipe(pipe, NULL))
		{
			OutputMessage(GetWinErrorMessage(GetLastError(), &exit_code, NULL));
			goto EXIT;
		}
		for (;;)
		{
			if (!ReadFile(pipe, buf, 1024, &read_size, NULL))
			{
				break;
			}
			WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, read_size, &write_size, NULL);
		}
		FlushFileBuffers(pipe);
		DisconnectNamedPipe(pipe);

		WaitForSingleObject(si.hProcess, INFINITE);
		ret = GetExitCodeProcess(si.hProcess, &exit_code);
		if (ret == FALSE)
		{
			exit_code = GetLastError();
		}
		CloseHandle(si.hProcess);
	}
	else
	{
		exit_code = GetLastError();
	}

EXIT:
	if (buf != NULL)
	{
		HeapFree(GetProcessHeap(), 0, buf);
	}
	if (params != NULL)
	{
		free(params);
	}
	if (module != NULL)
	{
		free(module);
	}

	return exit_code;
}
