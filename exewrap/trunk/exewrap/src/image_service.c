/* このファイルの文字コードは Shift_JIS (MS932) です。*/

#include <windows.h>
#include <process.h>
#include <shlobj.h>
#include <stdio.h>
#include <jni.h>

#include "include/jvm.h"
#include "include/loader.h"
#include "include/message.h"
#include "include/eventlog.h"
#include "include/wcout.h"

#define BUFFER_SIZE   32768
#define MAX_LONG_PATH 32768
#define ERROR_UNKNOWN ((DWORD)STG_E_UNKNOWN)

#define RUN_AS_ADMINISTRATOR_ARG   L"__ruN_aA_administratoR__"
#define RUN_AS_ADMINISTRATOR       1
#define SERVICE_INSTALL            2
#define SERVICE_START_IMMEDIATELY  4
#define SERVICE_REMOVE             8
#define SERVICE_STOP_BEFORE_REMOVE 16
#define SERVICE_START_BY_SCM       32
#define SHOW_HELP_MESSAGE          64

#define PIPE_BUFFER_SIZE           2048
#define PIPE_TIMEOUT_MILLIS        1000

#define EVENTLOG_STDOUT            0x7E80
#define EVENTLOG_STDERR            0x7F80

DWORD uncaught_exception(JNIEnv* env, jobject thread, jthrowable throwable);

static DWORD       install_service(const wchar_t* service_name, int argc, const wchar_t* argv[], int opt_end);
static DWORD       set_service_description(const wchar_t* service_name, const wchar_t* description);
static DWORD       remove_service(const wchar_t* service_name);
static DWORD       start_service(const wchar_t* service_name);
static DWORD       stop_service(const wchar_t* service_name);
static void        start_service_main(void);
static void        stop_service_main(void);
static BOOL WINAPI console_control_handler(DWORD dwCtrlType);
static void        service_control_handler(DWORD request);
static int         service_main(int argc, const wchar_t* argv[]);
static int         parse_args(int* argc_ptr, const wchar_t* argv[], int* opt_end);
static wchar_t**   parse_opt(int argc, const wchar_t* argv[]);
static void        write_message(WORD event_type, const wchar_t* message);
static void        write_message_by_error_code(WORD evnet_type, DWORD last_error, const wchar_t* append);
static void        show_help_message(void);
static void        set_current_directory(void);
static wchar_t*    get_service_name(void);
static wchar_t*    get_pipe_name(void);
static DWORD       run_as_administrator(HANDLE pipe, int argc, const wchar_t* argv[], const wchar_t* append);

static SERVICE_STATUS service_status = { 0 };
static SERVICE_STATUS_HANDLE hStatus;
static int       flags;
static int       ARG_COUNT;
static wchar_t** ARG_VALUE;
static jclass    MainClass;
static jmethodID MainClass_stop;
static HANDLE    hConOut = NULL;

int wmain(int argc, wchar_t* argv[])
{
	wchar_t*  service_name = NULL;
	int       opt_end      = 0;
	wchar_t*  pipe_name    = NULL;
	HANDLE    pipe         = NULL;
	DWORD     error        = NO_ERROR;

	service_name = get_service_name();
	flags = parse_args(&argc, argv, &opt_end);

	if(flags & SHOW_HELP_MESSAGE)
	{
		wchar_t* ext_flags = from_utf8((char*)get_resource(L"EXTFLAGS", NULL));
		if(ext_flags == NULL || wcsstr(ext_flags, L"NOHELP") == NULL)
		{
			show_help_message();
			goto EXIT;
		}
	}

	if(flags & SERVICE_START_BY_SCM)
	{
		SERVICE_TABLE_ENTRY ServiceTable[2];

		set_current_directory();
		
		ARG_COUNT = argc;
		ARG_VALUE = argv;

		ServiceTable[0].lpServiceName = service_name;
		ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)start_service_main;
		ServiceTable[1].lpServiceName = NULL;
		ServiceTable[1].lpServiceProc = NULL;
		StartServiceCtrlDispatcher(ServiceTable);
		goto EXIT;
	}

	if(!(flags & RUN_AS_ADMINISTRATOR) && (flags & (SERVICE_INSTALL | SERVICE_REMOVE)))
	{
		pipe_name = get_pipe_name();
		pipe = CreateNamedPipe(pipe_name, PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_WAIT, 2, PIPE_BUFFER_SIZE, PIPE_BUFFER_SIZE, PIPE_TIMEOUT_MILLIS, NULL);
		if(pipe == INVALID_HANDLE_VALUE)
		{
			error = GetLastError();
			write_message_by_error_code(EVENTLOG_STDERR, error, L"CreateNamedPipe");
			goto EXIT;
		}
		error = run_as_administrator(pipe, argc, argv, RUN_AS_ADMINISTRATOR_ARG);
		if(error != NO_ERROR)
		{
			// run_as_administrator呼び出し先によって詳細なエラーメッセージが直接出力されるため、
			// run_as_administratorの戻り値に対するエラーメッセージを出力する必要はありません。
		}
		goto EXIT;
	}

	if(flags & RUN_AS_ADMINISTRATOR)
	{
		pipe_name = get_pipe_name();
		pipe = CreateFile(pipe_name, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if(pipe == INVALID_HANDLE_VALUE)
		{
			error = GetLastError();
			write_message_by_error_code(EVENTLOG_STDERR, error, NULL);
			goto EXIT;
		}
		SetStdHandle(STD_OUTPUT_HANDLE, pipe);
	}

	if(flags & SERVICE_INSTALL)
	{
		error = install_service(service_name, argc, argv, opt_end);
		if(error == NO_ERROR)
		{
			if(flags & SERVICE_START_IMMEDIATELY)
			{
				error = start_service(service_name);
			}
		}
		goto EXIT;
	}
	if(flags & SERVICE_REMOVE)
	{
		if(flags & SERVICE_STOP_BEFORE_REMOVE)
		{
			error = stop_service(service_name);
			if(error != NO_ERROR)
			{
				goto EXIT;
			}
			Sleep(500);
		}
		error = remove_service(service_name);
		goto EXIT;
	}

	error = service_main(argc, argv);

EXIT:
	if(pipe != NULL && pipe != INVALID_HANDLE_VALUE)
	{
		CloseHandle(pipe);
	}
	if(pipe_name != NULL)
	{
		free(pipe_name);
	}
	if(service_name != NULL)
	{
		free(service_name);
	}

	ExitProcess(error);
}


static int service_main(int argc, const wchar_t* argv[])
{
	SYSTEMTIME   startup;
	int          error;
	wchar_t*     relative_classpath = NULL;
	wchar_t*     relative_extdirs   = NULL;
	wchar_t*     ext_flags          = NULL;
	wchar_t*     vm_args_opt        = NULL;
	wchar_t*     utilities          = NULL;
	BOOL         use_server_vm;
	BOOL         use_side_by_side_jre;
	BOOL         is_security_manager_required = FALSE;
	RESOURCE     res;
	LOAD_RESULT  result = { 0 };
	wchar_t*     service_name = NULL;
	BOOL         is_service = (flags & SERVICE_START_BY_SCM);
	WORD         event_type = (is_service ? EVENTLOG_ERROR_TYPE : EVENTLOG_STDERR);
	jmethodID    MainClass_start;
	jobjectArray MainClass_start_args;
	int          i;

	GetSystemTime(&startup);

	utilities = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(utilities == NULL)
	{
		error = ERROR_NOT_ENOUGH_MEMORY;
		write_message_by_error_code(event_type, error, L"malloc");
		goto EXIT;
	}
	utilities[0] = L'\0';

	result.msg_id = NO_ERROR;
	result.msg = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(result.msg == NULL)
	{
		error = ERROR_NOT_ENOUGH_MEMORY;
		write_message_by_error_code(event_type, error, L"malloc");
		goto EXIT;
	}

	service_name = get_service_name();

	relative_classpath = from_utf8((char*)get_resource(L"CLASS_PATH", NULL));
	relative_extdirs = from_utf8((char*)get_resource(L"EXTDIRS", NULL));
	ext_flags = from_utf8((char*)get_resource(L"EXTFLAGS", NULL));
	use_server_vm = (ext_flags != NULL && wcsstr(ext_flags, L"SERVER") != NULL);
	use_side_by_side_jre = (ext_flags == NULL) || (wcsstr(ext_flags, L"NOSIDEBYSIDE") == NULL);
	initialize_path(relative_classpath, relative_extdirs, use_server_vm, use_side_by_side_jre);

	if(relative_classpath != NULL)
	{
		free(relative_classpath);
		relative_classpath = NULL;
	}
	if(relative_extdirs != NULL)
	{
		free(relative_extdirs);
		relative_extdirs = NULL;
	}

	if(ext_flags == NULL || wcsstr(ext_flags, L"NOENCODINGFIX") == NULL)
	{
		wcscat_s(utilities, BUFFER_SIZE, UTIL_ENCODING_FIX);
	}
	if(ext_flags == NULL || wcsstr(ext_flags, L"IGNORE_UNCAUGHT_EXCEPTION") == NULL)
	{
		wcscat_s(utilities, BUFFER_SIZE, UTIL_UNCAUGHT_EXCEPTION_HANDLER);
	}
	if(is_service)
	{
		if(ext_flags == NULL || wcsstr(ext_flags, L"NOLOG") == NULL)
		{
			wcscat_s(utilities, BUFFER_SIZE, UTIL_EVENT_LOG_STREAM);
		}
		wcscat_s(utilities, BUFFER_SIZE, UTIL_EVENT_LOG_HANDLER);
	}
	if(ext_flags != NULL)
	{
		free(ext_flags);
		ext_flags = NULL;
	}

	if(!is_service)
	{
		vm_args_opt = from_utf8((char*)get_resource(L"VMARGS_B", NULL));
	}
	if(vm_args_opt == NULL)
	{
		vm_args_opt = from_utf8((char*)get_resource(L"VMARGS", NULL));
	}
	create_java_vm(vm_args_opt, use_server_vm, use_side_by_side_jre, &is_security_manager_required, &error);
	if(error != JNI_OK)
	{
		get_jni_error_message(error, &result.msg_id, result.msg, BUFFER_SIZE);
		write_message(event_type, result.msg);
		goto EXIT;
	}
	if(vm_args_opt != NULL)
	{
		free(vm_args_opt);
		vm_args_opt = NULL;
	}

	set_application_properties(&startup);

	if(get_resource(L"TARGET_VERSION", &res) != NULL)
	{
		wchar_t* target_version = from_utf8((char*)res.buf);
		DWORD target_major     = 0;
		DWORD target_minor     = 0;
		DWORD target_build     = 0;
		DWORD target_revision  = 0;
		DWORD runtime_major    = 0;
		DWORD runtime_minor    = 0;
		DWORD runtime_build    = 0;
		DWORD runtime_revision = 0;
		BOOL  version_error    = FALSE;

		get_java_runtime_version(target_version, &target_major, &target_minor, &target_build, &target_revision);
		get_java_runtime_version(NULL, &runtime_major, &runtime_minor, &runtime_build, &runtime_revision);

		if(runtime_major < target_major)
		{
			version_error = TRUE;
		}
		else if(runtime_major == target_major)
		{
			if(runtime_minor < target_minor)
			{
				version_error = TRUE;
			}
			else if(runtime_minor == target_minor)
			{
				if(runtime_build < target_build)
				{
					version_error = TRUE;
				}
				else if(runtime_build == target_build)
				{
					if(runtime_revision < target_revision)
					{
						version_error = TRUE;
					}
				}
			}
		}

		if(version_error == TRUE)
		{
			wchar_t* target_version_string;

			target_version_string = get_java_version_string(target_major, target_minor, target_build, target_revision);
			result.msg_id = MSG_ID_ERR_TARGET_VERSION;
			swprintf_s(result.msg, BUFFER_SIZE, _(MSG_ID_ERR_TARGET_VERSION), target_version_string);
			write_message(event_type, result.msg);
			error = ERROR_BAD_ENVIRONMENT;
			goto EXIT;
		}
		free(target_version);
	}

	if(load_main_class(argc, argv, utilities, &result) == FALSE)
	{
		write_message(event_type, result.msg);
		error = ERROR_INVALID_DATA;
		goto EXIT;
	}
	MainClass = result.MainClass;

	MainClass_start = (*env)->GetStaticMethodID(env, result.MainClass, "start", "([Ljava/lang/String;)V");
	if(MainClass_start == NULL)
	{
		result.msg_id = MSG_ID_ERR_FIND_METHOD_SERVICE_START;
		wcscpy_s(result.msg, BUFFER_SIZE, _(MSG_ID_ERR_FIND_METHOD_SERVICE_START));
		write_message(event_type, result.msg);
		error = ERROR_INVALID_FUNCTION;
		goto EXIT;
	}
	if(is_service && argc > 2)
	{
		MainClass_start_args = (*env)->NewObjectArray(env, argc - 2, (*env)->FindClass(env, "java/lang/String"), NULL);
		for(i = 2; i < argc; i++)
		{
			(*env)->SetObjectArrayElement(env, MainClass_start_args, (i - 2), to_jstring(env, argv[i]));
		}
	}
	else if(!is_service && argc > 1)
	{
		MainClass_start_args = (*env)->NewObjectArray(env, argc - 1, (*env)->FindClass(env, "java/lang/String"), NULL);
		for (i = 1; i < argc; i++)
		{
			(*env)->SetObjectArrayElement(env, MainClass_start_args, (i - 1), to_jstring(env, argv[i]));
		}
	}
	else
	{
		MainClass_start_args = (*env)->NewObjectArray(env, 0, (*env)->FindClass(env, "java/lang/String"), NULL);
	}
	MainClass_stop = (*env)->GetStaticMethodID(env, result.MainClass, "stop", "()V");
	if(MainClass_stop == NULL)
	{
		result.msg_id = MSG_ID_ERR_FIND_METHOD_SERVICE_STOP;
		wcscpy_s(result.msg, BUFFER_SIZE, _(MSG_ID_ERR_FIND_METHOD_SERVICE_STOP));
		write_message(event_type, result.msg);
		error = ERROR_INVALID_FUNCTION;
		goto EXIT;
	}

	if(is_service)
	{
		swprintf_s(result.msg, BUFFER_SIZE, _(MSG_ID_SUCCESS_SERVICE_START), service_name);
		write_event_log(EVENTLOG_INFORMATION_TYPE, result.msg);
	}

	// JavaVM が CTRL_SHUTDOWN_EVENT を受け取って終了してしまわないように、ハンドラを登録して先取りします。
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)console_control_handler, TRUE);
	// シャットダウン時にダイアログが表示されないようにします。
	SetProcessShutdownParameters(0x4FF, SHUTDOWN_NORETRY);

	if(is_security_manager_required)
	{
		wchar_t* msg = install_security_manager(env);
		if(msg != NULL)
		{
			write_message(event_type, msg);
			free(msg);
		}
		goto EXIT;
	}

	if((*env)->ExceptionCheck(env) == JNI_TRUE)
	{
		(*env)->ExceptionClear(env);
	}
	(*env)->CallStaticVoidMethod(env, result.MainClass, MainClass_start, MainClass_start_args);

	if((*env)->ExceptionCheck(env) == JNI_TRUE)
	{
		jthrowable throwable = (*env)->ExceptionOccurred(env);
		if(throwable != NULL)
		{
			uncaught_exception(env, NULL, throwable);
			(*env)->DeleteLocalRef(env, throwable);
		}
		(*env)->ExceptionClear(env);
	}
	else
	{
		if(is_service)
		{
			swprintf_s(result.msg, BUFFER_SIZE, _(MSG_ID_SUCCESS_SERVICE_STOP), service_name);
			write_event_log(EVENTLOG_INFORMATION_TYPE, result.msg);
		}
	}

EXIT:
	if(result.msg != NULL)
	{
		free(result.msg);
	}
	if(env != NULL)
	{
		detach_java_vm();
	}
	if(jvm != NULL)
	{
		//デーモンではないスレッド(たとえばSwing)が残っていると待機状態になってしまうため、
		//サービスでは、DestroyJavaVM() を実行しないようにしています。
		if(!is_service)
		{
			destroy_java_vm();
		}
	}

	return result.msg_id;
}


static DWORD install_service(const wchar_t* service_name, int argc, const wchar_t* argv[], int opt_end)
{
	DWORD     error              = NO_ERROR;
	wchar_t*  description        = NULL;
	wchar_t*  buf                = NULL;
	wchar_t*  path               = NULL;
	int       i;
	wchar_t** opt                = NULL;
	wchar_t*  lpDisplayName      = NULL;
	DWORD     dwServiceType      = SERVICE_WIN32_OWN_PROCESS;
	DWORD     dwStartType        = SERVICE_AUTO_START;
	wchar_t*  lpDependencies     = NULL;
	wchar_t*  lpServiceStartName = NULL;
	wchar_t*  lpPassword         = NULL;
	SC_HANDLE hSCManager         = NULL;
	SC_HANDLE hService           = NULL;

	description = from_utf8((char*)get_resource(L"SVCDESC", NULL));

	buf = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(buf == NULL)
	{
		error = ERROR_NOT_ENOUGH_MEMORY;
		goto EXIT;
	}

	path = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(path == NULL)
	{
		error = ERROR_NOT_ENOUGH_MEMORY;
		goto EXIT;
	}

	GetModuleFileName(NULL, buf, MAX_LONG_PATH);
	
	wcscpy_s(path, BUFFER_SIZE, L"\"");
	wcscat_s(path, BUFFER_SIZE, buf);
	wcscat_s(path, BUFFER_SIZE, L"\" -service");
	for(i = opt_end + 2; i < argc; i++)
	{
		wcscat_s(path, BUFFER_SIZE, L" \"");
		wcscat_s(path, BUFFER_SIZE, argv[i]);
		wcscat_s(path, BUFFER_SIZE, L"\"");
	}

	opt = parse_opt(opt_end + 1, argv);
	if(opt['n'])
	{
		lpDisplayName = opt['n'];
	}
	if(opt['i'] && opt['u'] == NULL && opt['p'] == NULL)
	{
		dwServiceType += SERVICE_INTERACTIVE_PROCESS;
	}
	if(opt['m'])
	{
		dwStartType = SERVICE_DEMAND_START;
	}
	if(opt['d'])
	{
		wchar_t* p;
		size_t len = wcslen(opt['d']) + 1;
		lpDependencies = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
		wcscpy_s(lpDependencies, len + 1, opt['d']);
		wcscat_s(lpDependencies, len + 1, L";");
		while((p = wcsrchr(lpDependencies, L';')) != NULL)
		{
			*p = L'\0';
		}
	}
	if(opt['u'])
	{
		lpServiceStartName = opt['u'];
	}
	if(opt['p'])
	{
		lpPassword = opt['p'];
	}

	hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if(hSCManager == NULL)
	{
		error = GetLastError();
		write_message_by_error_code(EVENTLOG_STDOUT, error, NULL);
		goto EXIT;
	}

	hService = CreateService(hSCManager, service_name, lpDisplayName, SERVICE_ALL_ACCESS, dwServiceType, dwStartType, SERVICE_ERROR_NORMAL, path, NULL, NULL, lpDependencies, lpServiceStartName, lpPassword);
	if(hService == NULL)
	{
		error = GetLastError();
		write_message_by_error_code(EVENTLOG_STDOUT, error, NULL);
		goto EXIT;
	}
	else
	{
		swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_SUCCESS_SERVICE_INSTALL), service_name);
		write_message(EVENTLOG_STDOUT, buf);
	}
	error = set_service_description(service_name, description);
	if(error != NO_ERROR)
	{
		goto EXIT;
	}
	error = install_event_log();
	if(error != NO_ERROR)
	{
		goto EXIT;
	}

EXIT:
	if(hService != NULL)
	{
		CloseServiceHandle(hService);
	}
	if(hSCManager != NULL)
	{
		CloseServiceHandle(hSCManager);
	}
	if(lpDependencies != NULL)
	{
		free(lpDependencies);
	}
	if(path != NULL)
	{
		free(path);
	}
	if(buf != NULL)
	{
		free(buf);
	}
	if(description != NULL)
	{
		free(description);
	}

	return error;
}


static DWORD set_service_description(const wchar_t* service_name, const wchar_t* description)
{
	DWORD     error = NO_ERROR;
	wchar_t*  buf   = NULL;
	size_t    size;
	wchar_t*  key   = NULL;
	HKEY      hKey  = NULL;

	if(description == NULL)
	{
		goto EXIT;
	}

	buf = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(buf == NULL)
	{
		error = ERROR_NOT_ENOUGH_MEMORY;
		write_message_by_error_code(EVENTLOG_STDOUT, error, L"malloc");
		goto EXIT;
	}

	key = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(key == NULL)
	{
		error = ERROR_NOT_ENOUGH_MEMORY;
		write_message_by_error_code(EVENTLOG_STDOUT, error, L"malloc");
		goto EXIT;
	}

	wcscpy_s(key, MAX_LONG_PATH, L"SYSTEM\\CurrentControlSet\\Services\\");
	wcscat_s(key, MAX_LONG_PATH, service_name);

	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS)
	{
		error = GetLastError();
		write_message_by_error_code(EVENTLOG_STDOUT, error, key);
		goto EXIT;
	}
	size = (wcslen(description) + 1) * sizeof(wchar_t);
	if(RegSetValueEx(hKey, L"Description", 0, REG_SZ, (LPBYTE)description, (DWORD)size) != ERROR_SUCCESS)
	{
		error = GetLastError();
		write_message_by_error_code(EVENTLOG_STDOUT, error, L"RegSetValueEx:Description");
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
	if(buf != NULL)
	{
		free(buf);
	}

	return error;
}


static DWORD remove_service(const wchar_t* service_name)
{
	DWORD           error      = NO_ERROR;
	wchar_t*        buf        = NULL;
	SC_HANDLE       hSCManager = NULL;
	SC_HANDLE       hService   = NULL;
	SERVICE_STATUS  status;
	BOOL            ret;

	buf = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(buf == NULL)
	{
		error = ERROR_NOT_ENOUGH_MEMORY;
		write_message_by_error_code(EVENTLOG_STDOUT, error, L"malloc");
		goto EXIT;
	}

	hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if(hSCManager == NULL)
	{
		error = GetLastError();
		write_message_by_error_code(EVENTLOG_STDOUT, error, L"OpenSCManager");
		goto EXIT;
	}
	hService = OpenService(hSCManager, service_name, SERVICE_ALL_ACCESS | DELETE);
	if (hService == NULL)
	{
		error = GetLastError();
		write_message_by_error_code(EVENTLOG_STDOUT, error, NULL);
		goto EXIT;
	}

	ret = QueryServiceStatus(hService, &status);
	if(ret == FALSE)
	{
		error = GetLastError();
		write_message_by_error_code(EVENTLOG_STDOUT, error, L"QueryServiceStatus");
		goto EXIT;
	}
	if (status.dwCurrentState != SERVICE_STOPPED)
	{
		swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_ERR_SERVICE_NOT_STOPPED), service_name);
		write_message(EVENTLOG_STDOUT, buf);
		error = ERROR_UNKNOWN;
		goto EXIT;
	}

	if(!DeleteService(hService))
	{
		error = GetLastError();
		write_message_by_error_code(EVENTLOG_STDOUT, error, NULL);
		goto EXIT;
	}

	swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_SUCCESS_SERVICE_REMOVE), service_name);
	write_message(EVENTLOG_STDOUT, buf);

	error = remove_event_log();
	if(error != NO_ERROR)
	{
		goto EXIT;
	}

EXIT:
	if(hService != NULL)
	{
		CloseServiceHandle(hService);
	}
	if(hSCManager != NULL)
	{
		CloseServiceHandle(hSCManager);
	}
	if(buf != NULL)
	{
		free(buf);
	}

	return error;
}


DWORD start_service(const wchar_t* service_name)
{
	DWORD      error      = NO_ERROR;
	wchar_t*   buf        = NULL;
	SC_HANDLE  hSCManager = NULL;
	SC_HANDLE  hService   = NULL;
	BOOL       ret;

	buf = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(buf == NULL)
	{
		error = ERROR_NOT_ENOUGH_MEMORY;
		write_message_by_error_code(EVENTLOG_STDOUT, error, L"malloc");
		goto EXIT;
	}

	hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if(hSCManager == NULL)
	{
		error = GetLastError();
		write_message_by_error_code(EVENTLOG_STDOUT, error, L"OpenSCManager");
		goto EXIT;
	}
	hService = OpenService(hSCManager, service_name, SERVICE_START);
	if(hService == NULL)
	{
		error = GetLastError();
		write_message_by_error_code(EVENTLOG_STDOUT, error, L"OpenService");
		goto EXIT;
	}
	ret = StartService(hService, 0, NULL);
	if(ret == FALSE)
	{
		error = GetLastError();
		write_message_by_error_code(EVENTLOG_STDOUT, error, L"StartService");
		goto EXIT;
	}

	swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_SUCCESS_SERVICE_START), service_name);
	write_message(EVENTLOG_STDOUT, buf);

EXIT:
	if(hService != NULL)
	{
		CloseServiceHandle(hService);
	}
	if(hSCManager != NULL)
	{
		CloseServiceHandle(hSCManager);
	}
	if(buf != NULL)
	{
		free(buf);
	}

	return error;
}


static DWORD stop_service(const wchar_t* service_name)
{
	DWORD           error      = NO_ERROR;
	wchar_t*        buf        = NULL;
	SC_HANDLE       hSCManager = NULL;
	SC_HANDLE       hService   = NULL;
	SERVICE_STATUS  status;
	BOOL            ret;
	int             i;

	buf = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(buf == NULL)
	{
		error = ERROR_NOT_ENOUGH_MEMORY;
		write_message_by_error_code(EVENTLOG_STDOUT, error, L"malloc");
		goto EXIT;
	}

	hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if(hSCManager == NULL)
	{
		error = GetLastError();
		write_message_by_error_code(EVENTLOG_STDOUT, error, L"OpenSCManager");
		goto EXIT;
	}
	hService = OpenService(hSCManager, service_name, SERVICE_ALL_ACCESS);
	if(hService == NULL)
	{
		error = GetLastError();
		write_message_by_error_code(EVENTLOG_STDOUT, error, L"OpenService");
		goto EXIT;
	}
	ret = QueryServiceStatus(hService, &status);
	if(ret == FALSE)
	{
		error = GetLastError();
		write_message_by_error_code(EVENTLOG_STDOUT, error, L"QueryServiceStatus");
		goto EXIT;
	}
	if(status.dwCurrentState != SERVICE_STOPPED && status.dwCurrentState != SERVICE_STOP_PENDING)
	{
		if(ControlService(hService, SERVICE_CONTROL_STOP, &status) == 0)
		{
			error = GetLastError();
			write_message_by_error_code(EVENTLOG_STDOUT, error, L"ControlService");
			goto EXIT;
		}
		swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_SERVICE_STOPING), service_name);
		write_message(EVENTLOG_STDOUT, buf);
		for(i = 0; i < 240; i++)
		{
			if(QueryServiceStatus(hService, &status) == 0)
			{
				error = GetLastError();
				write_message_by_error_code(EVENTLOG_STDOUT, error, L"QueryServiceStatus");
				goto EXIT;
			}
			if(status.dwCurrentState == SERVICE_STOPPED)
			{
				swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_SUCCESS_SERVICE_STOP), service_name);
				write_message(EVENTLOG_STDOUT, buf);
				break;
			}
			Sleep(500);
		}
	}

EXIT:
	if(hService != NULL)
	{
		CloseServiceHandle(hService);
	}
	if(hSCManager != NULL)
	{
		CloseServiceHandle(hSCManager);
	}
	if(buf != NULL)
	{
		free(buf);
	}

	return error;
}


static void start_service_main()
{
	wchar_t* service_name = get_service_name();

	service_status.dwServiceType = SERVICE_WIN32;
	service_status.dwCurrentState = SERVICE_START_PENDING;
	service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	service_status.dwWin32ExitCode = 0;
	service_status.dwServiceSpecificExitCode = 0;
	service_status.dwCheckPoint = 0;
	service_status.dwWaitHint = 0;

	hStatus = RegisterServiceCtrlHandler(service_name, (LPHANDLER_FUNCTION)service_control_handler);
	if(hStatus == (SERVICE_STATUS_HANDLE)0)
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
	if(service_name != NULL)
	{
		free(service_name);
	}
}


static void stop_service_main()
{
	JNIEnv* env = attach_java_vm();
	jstring thread_name = to_jstring(env, L"scm");

	// スレッド名を変更します
	if(thread_name != NULL)
	{
		jclass    Thread               = NULL;
		jmethodID Thread_currentThread = NULL;
		jobject   thread               = NULL;
		jmethodID thread_setName       = NULL;
		
		Thread = (*env)->FindClass(env, "java/lang/Thread");
		if(Thread == NULL)
		{
			goto NEXT;
		}
		Thread_currentThread = (*env)->GetStaticMethodID(env, Thread, "currentThread", "()Ljava/lang/Thread;");
		if(Thread_currentThread == NULL)
		{
			goto NEXT;
		}
		thread_setName = (*env)->GetMethodID(env, Thread, "setName", "(Ljava/lang/String;)V");
		if(thread_setName == NULL)
		{
			goto NEXT;
		}

		thread = (*env)->CallStaticObjectMethod(env, Thread, Thread_currentThread);
		if(thread == NULL)
		{
			goto NEXT;
		}
		(*env)->CallObjectMethod(env, thread, thread_setName, thread_name);

		NEXT:
		if(thread != NULL)
		{
			(*env)->DeleteLocalRef(env, thread);
		}
		(*env)->DeleteLocalRef(env, thread_name);
		thread_name = NULL;
	}

	if((*env)->ExceptionCheck(env) == JNI_TRUE)
	{
		(*env)->ExceptionClear(env);
	}
	(*env)->CallStaticVoidMethod(env, MainClass, MainClass_stop);

	if((*env)->ExceptionCheck(env) == JNI_TRUE)
	{
		jthrowable throwable = (*env)->ExceptionOccurred(env);
		if(throwable != NULL)
		{
			uncaught_exception(env, NULL, throwable);
			(*env)->DeleteLocalRef(env, throwable);
		}
		(*env)->ExceptionClear(env);
	}

	detach_java_vm();
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
			wcoutf(_(MSG_ID_CTRL_SERVICE_STOP), L"CTRL_C");
			wcout(L"\r\n");
			stop_service_main();
			return TRUE;
		}
		else
		{
			wcoutf(_(MSG_ID_CTRL_SERVICE_TERMINATE), L"CTRL_C");
			wcout(L"\r\n");
			return FALSE;
		}

	case CTRL_BREAK_EVENT:
		wcoutf(_(MSG_ID_CTRL_BREAK), L"CTRL_BREAK");
		wcout(L"\r\n");
		return FALSE;

	case CTRL_CLOSE_EVENT:
		wcoutf(_(MSG_ID_CTRL_SERVICE_STOP), L"CTRL_CLOSE");
		wcout(L"\r\n");
		stop_service_main();
		return TRUE;

	case CTRL_LOGOFF_EVENT:
		if(!(flags & SERVICE_START_BY_SCM))
		{
			wcoutf(_(MSG_ID_CTRL_SERVICE_STOP), L"CTRL_LOGOFF");
			wcout(L"\r\n");
			stop_service_main();
		}
		return TRUE;

	case CTRL_SHUTDOWN_EVENT:
		if(!(flags & SERVICE_START_BY_SCM))
		{
			wcoutf(_(MSG_ID_CTRL_SERVICE_STOP), L"CTRL_SHUTDOWN");
			wcout(L"\r\n");
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


static int parse_args(int* argc_ptr, const wchar_t* argv[], int* opt_end)
{
	int  argc = *argc_ptr;
	int  flags = 0;
	int  i;
	BOOL has_opt_s = FALSE;

	*opt_end = 0;

	if(wcscmp(argv[argc - 1], RUN_AS_ADMINISTRATOR_ARG) == 0)
	{
		flags |= RUN_AS_ADMINISTRATOR;
		*argc_ptr = --argc;
	}
	if((argc >= 2) && (wcscmp(argv[1], L"-service") == 0))
	{
		flags |= SERVICE_START_BY_SCM;
	}
	for(i = 1; i < argc; i++)
	{
		if(wcscmp(argv[i], L"-s") == 0)
		{
			has_opt_s = TRUE;
		}
		if(wcscmp(argv[i], L"-install") == 0)
		{
			flags |= SERVICE_INSTALL;
			if(has_opt_s)
			{
				flags |= SERVICE_START_IMMEDIATELY;
			}
			*opt_end = i - 1;
			break;
		}
		if(wcscmp(argv[i], L"-remove") == 0)
		{
			flags |= SERVICE_REMOVE;
			if(has_opt_s)
			{
				flags |= SERVICE_STOP_BEFORE_REMOVE;
			}
			*opt_end = i - 1;
			break;
		}
	}
	if((argc == 2) && (wcscmp(argv[1], L"-help") == 0))
	{
		flags |= SHOW_HELP_MESSAGE;
	}

	return flags;
}


static wchar_t** parse_opt(int argc, const wchar_t* argv[])
{
	int i;
	wchar_t** opt = (wchar_t**)calloc(256, sizeof(wchar_t*));

	if(opt == NULL)
	{
		return NULL;
	}

	if((argc > 1) && (*argv[1] != L'-'))
	{
		opt[0] = (wchar_t*)argv[1];
	}
	for(i = 0; i < argc; i++)
	{
		if(*argv[i] == L'-')
		{
			if(argv[i + 1] == NULL || *argv[i + 1] == L'-')
			{
				opt[*(argv[i] + 1)] = L"";
			}
			else
			{
				opt[*(argv[i] + 1)] = (wchar_t*)argv[i + 1];
			}
		}
	}
	if((opt[0] == NULL) && (*argv[argc - 1] != L'-'))
	{
		opt[0] = (wchar_t*)argv[argc - 1];
	}
	return opt;
}


static void write_message(WORD event_type, const wchar_t* message)
{
	if(message == NULL)
	{
		return;
	}

	if(event_type == EVENTLOG_STDOUT)
	{
		wcout(message);
		wcout(L"\r\n");
	}
	else if(event_type == EVENTLOG_STDERR)
	{
		wcerr(message);
		wcerr(L"\r\n");
	}
	else
	{
		write_event_log(event_type, message);
	}
}

static void write_message_by_error_code(WORD event_type, DWORD last_error, const wchar_t* append)
{
	wchar_t* message = NULL;
	wchar_t* s       = NULL;

	message = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(message != NULL)
	{
		message[0] = L'\0';

		s = get_error_message(last_error);
		if(s != NULL)
		{
			wcscat_s(message, BUFFER_SIZE, s);
			if(append != NULL)
			{
				wcscat_s(message, BUFFER_SIZE, L" (");
				wcscat_s(message, BUFFER_SIZE, append);
				wcscat_s(message, BUFFER_SIZE, L")");
			}
			free(s);
		}
		else if(append != NULL)
		{
			wcscat_s(message, BUFFER_SIZE, append);
		}

		if(wcslen(message) > 0)
		{
			write_message(event_type, message);

		}
		free(message);
	}
}

static void show_help_message()
{
	wchar_t* buf; 
	wchar_t* filename;

	buf = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(buf == NULL)
	{
		return;
	}

	GetModuleFileName(NULL, buf, MAX_LONG_PATH);
	filename = wcsrchr(buf, L'\\') + 1;

	wcoutf(L"Usage:\r\n"
			L"  %ls [install-options] -install [runtime-arguments]\r\n"
			L"  %ls [remove-options] -remove\r\n"
			L"  %ls [runtime-arguments]\r\n"
			L"\r\n"
			L"Install Options:\r\n"
			L"  -n <display-name>\t set service display name.\r\n"
			L"  -i               \t allow interactive.\r\n"
			L"  -m               \t \r\n"
			L"  -d <dependencies>\t \r\n"
			L"  -u <username>    \t \r\n"
			L"  -p <password>    \t \r\n"
			L"  -s               \t start service.\r\n"
			L"\r\n"
			L"Remove Options:\r\n"
			L"  -s               \t stop service.\r\n"
			, filename, filename, filename);

	if(buf != NULL)
	{
		free(buf);
	}
}


static void set_current_directory()
{
	wchar_t* filepath = NULL;

	filepath = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(filepath == NULL)
	{
		return;
	}

	GetModuleFileName(NULL, filepath, MAX_LONG_PATH);
	*(wcsrchr(filepath, L'\\')) = L'\0';
	SetCurrentDirectory(filepath);

	free(filepath);
}


static wchar_t* get_service_name()
{
	wchar_t* name = NULL;
	wchar_t* buf  = NULL;
	wchar_t* p    = NULL;
	size_t   len;

	buf = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(buf == NULL)
	{
		goto EXIT;
	}

	GetModuleFileName(NULL, buf, BUFFER_SIZE);
	*(wcsrchr(buf, L'.')) = L'\0';
	p = wcsrchr(buf, L'\\') + 1;

	len = wcslen(p);
	name = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
	if(name != NULL)
	{
		wcscpy_s(name, len + 1, p);
	}

EXIT:
	if(buf != NULL)
	{
		free(buf);
	}

	return name;
}


static wchar_t* get_pipe_name()
{
	wchar_t* name = NULL;
	wchar_t* buf = NULL;
	wchar_t* p    = NULL;
	size_t   len;

	buf = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(buf == NULL)
	{
		goto EXIT;
	}

	GetModuleFileName(NULL, buf, BUFFER_SIZE);
	p = wcsrchr(buf, L'\\') + 1;

	len = wcslen(p) + 14;
	name = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
	if(name != NULL)
	{
		wcscpy_s(name, len + 1, L"\\\\.\\pipe\\");
		wcscat_s(name, len + 1, p);
		wcscat_s(name, len + 1, L".pipe");
	}

EXIT:
	if(buf != NULL)
	{
		free(buf);
	}

	return name;
}


static DWORD run_as_administrator(HANDLE pipe, int argc, const wchar_t* argv[], const wchar_t* append)
{
	DWORD            error = NO_ERROR;
	wchar_t*         module = NULL;
	wchar_t*         params = NULL;
	int              i;
	SHELLEXECUTEINFO si;
	BOOL             ret;
	char*            buf = NULL;

	module = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(module == NULL)
	{
		error = ERROR_NOT_ENOUGH_MEMORY;
		goto EXIT;
	}
	params = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(params == NULL)
	{
		error = ERROR_NOT_ENOUGH_MEMORY;
		goto EXIT;
	}

	GetModuleFileName(NULL, module, MAX_LONG_PATH);

	params[0] = L'\0';
	for(i = 1; i < argc; i++)
	{
		wcscat_s(params, BUFFER_SIZE, L"\"");
		wcscat_s(params, BUFFER_SIZE, argv[i]);
		wcscat_s(params, BUFFER_SIZE, L"\" ");
	}
	wcscat_s(params, BUFFER_SIZE, append);

	ZeroMemory(&si, sizeof(SHELLEXECUTEINFO));
	si.cbSize = sizeof(SHELLEXECUTEINFO);
	si.fMask = SEE_MASK_NOCLOSEPROCESS;
	si.hwnd = GetActiveWindow();
	si.lpVerb = L"runas";
	si.lpFile = module;
	si.lpParameters = params;
	si.lpDirectory = NULL;
	si.nShow = SW_HIDE;
	si.hInstApp = 0;
	si.hProcess = 0;

	ret = ShellExecuteEx(&si);
	if(GetLastError() == ERROR_CANCELLED)
	{
		error = ERROR_CANCELLED;
		goto EXIT;
	}
	else if(ret == TRUE)
	{
		DWORD read_size;
		DWORD write_size;

		buf = (char*)malloc(BUFFER_SIZE);
		if(buf == NULL)
		{
			error = ERROR_NOT_ENOUGH_MEMORY;
			goto EXIT;
		}

		if(!ConnectNamedPipe(pipe, NULL))
		{
			error = GetLastError();
			goto EXIT;
		}
		for(;;)
		{
			if(!ReadFile(pipe, buf, BUFFER_SIZE, &read_size, NULL))
			{
				break;
			}
			WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, read_size, &write_size, NULL);
		}
		FlushFileBuffers(pipe);
		DisconnectNamedPipe(pipe);

		WaitForSingleObject(si.hProcess, INFINITE);
		ret = GetExitCodeProcess(si.hProcess, &error);
		if(ret == FALSE)
		{
			error = GetLastError();
		}
		CloseHandle(si.hProcess);
	}
	else
	{
		error = GetLastError();
	}

EXIT:
	if(buf != NULL)
	{
		free(buf);
	}
	if(params != NULL)
	{
		free(params);
	}
	if(module != NULL)
	{
		free(module);
	}

	return error;
}


DWORD uncaught_exception(JNIEnv* env, jobject thread, jthrowable throwable)
{
	BOOL      is_service        = (flags & SERVICE_START_BY_SCM);
	wchar_t*  stack_trace       = NULL;
	wchar_t*  error             = NULL;
	jclass    System            = NULL;
	jfieldID  System_err        = NULL;
	jobject   printStream       = NULL;
	jclass    PrintStream       = NULL;
	jmethodID printStream_print = NULL;
	jmethodID printStream_flush = NULL;
	jstring   jstr              = NULL;

	stack_trace = get_stack_trace(env, thread, throwable);
	if(stack_trace == NULL)
	{
		goto EXIT;
	}

	if(is_service)
	{
		write_event_log(EVENTLOG_ERROR_TYPE, stack_trace);
		goto EXIT;
	}

	error = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(error == NULL)
	{
		goto EXIT;
	}
	error[0] = L'\0';

	System = (*env)->FindClass(env, "java/lang/System");
	if(System == NULL)
	{
		swprintf_s(error, BUFFER_SIZE, _(MSG_ID_ERR_FIND_CLASS), L"java.lang.System");
		goto EXIT;
	}
	System_err = (*env)->GetStaticFieldID(env, System, "err", "Ljava/io/PrintStream;");
	if(System_err == NULL)
	{
		swprintf_s(error, BUFFER_SIZE, _(MSG_ID_ERR_GET_FIELD), "java.lang.System.err");
		goto EXIT;
	}
	printStream = (*env)->GetStaticObjectField(env, System, System_err);
	if(printStream == NULL)
	{
		swprintf_s(error, BUFFER_SIZE, _(MSG_ID_ERR_GET_FIELD), "java.lang.System.err");
		goto EXIT;
	}
	PrintStream = (*env)->FindClass(env, "java/io/PrintStream");
	if(PrintStream == NULL)
	{
		swprintf_s(error, BUFFER_SIZE, _(MSG_ID_ERR_FIND_CLASS), "java.io.PrintStream");
		goto EXIT;
	}
	printStream_print = (*env)->GetMethodID(env, PrintStream, "print", "(Ljava/lang/String;)V");
	if(printStream_print == NULL)
	{
		swprintf_s(error, BUFFER_SIZE, _(MSG_ID_ERR_GET_METHOD), "java.io.PrintStream.print(java.lang.String)");
		goto EXIT;
	}
	printStream_flush = (*env)->GetMethodID(env, PrintStream, "flush", "()V");
	if(printStream_flush == NULL)
	{
		swprintf_s(error, BUFFER_SIZE, _(MSG_ID_ERR_GET_METHOD), "java.io.PrintStream.flush()");
		goto EXIT;
	}

	jstr = to_jstring(env, stack_trace);
	(*env)->CallVoidMethod(env, printStream, printStream_print, jstr);
	(*env)->CallVoidMethod(env, printStream, printStream_flush);

	error[0] = L'\0';

EXIT:
	if(error != NULL && wcslen(error) > 0)
	{
		wcerr(error);
		wcerr(L"\r\n");
	}
	if(jstr != NULL)
	{
		(*env)->DeleteLocalRef(env, jstr);
	}
	if(printStream != NULL)
	{
		(*env)->DeleteLocalRef(env, printStream);
	}
	if(error != NULL)
	{
		free(error);
	}
	if(stack_trace != NULL)
	{
		free(stack_trace);
	}

	return ERROR_UNKNOWN;
}
