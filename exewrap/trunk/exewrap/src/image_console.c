/* このファイルの文字コードは Shift_JIS (MS932) です。*/

#include <windows.h>
#include <process.h>
#include <shlobj.h>
#include <stdio.h>
#include <jni.h>

#include "include/jvm.h"
#include "include/loader.h"
#include "include/notify.h"
#include "include/message.h"
#include "include/wcout.h"

#define BUFFER_SIZE   32768
#define MAX_LONG_PATH 32768
#define ERROR_UNKNOWN ((DWORD)STG_E_UNKNOWN)

DWORD uncaught_exception(JNIEnv* env, jobject thread, jthrowable throwable);

static void cleanup(void);
static void exit_process(DWORD last_error, const wchar_t* append);

int wmain(int argc, wchar_t* argv[])
{
	SYSTEMTIME  startup;
	int         error;
	wchar_t*    relative_classpath = NULL;
	wchar_t*    relative_extdirs   = NULL;
	wchar_t*    ext_flags          = NULL;
	wchar_t*    vm_args_opt        = NULL;
	wchar_t*    utilities          = NULL;
	BOOL        use_server_vm;
	BOOL        use_side_by_side_jre;
	BOOL        is_security_manager_required = FALSE;
	HANDLE      synchronize_mutex_handle = NULL;
	RESOURCE    res;
	LOAD_RESULT result;

	GetSystemTime(&startup);

	utilities = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(utilities == NULL)
	{
		exit_process(ERROR_NOT_ENOUGH_MEMORY, L"malloc");
	}
	utilities[0] = L'\0';

	result.msg = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(result.msg == NULL)
	{
		exit_process(ERROR_NOT_ENOUGH_MEMORY, L"malloc");
	}

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

	if(ext_flags != NULL)
	{
		if(wcsstr(ext_flags, L"SHARE") != NULL)
		{
			synchronize_mutex_handle = notify_exec(remote_call_main_method, argc, argv);
			if(synchronize_mutex_handle == NULL)
			{
				result.msg_id = NO_ERROR;
				goto EXIT;
			}
		}
		if(wcsstr(ext_flags, L"SINGLE") != NULL)
		{
			if(CreateMutex(NULL, TRUE, get_module_object_name(L"SINGLE")), GetLastError() == ERROR_ALREADY_EXISTS)
			{
				exit_process(ERROR_BUSY, NULL);
			}
		}
		if(wcsstr(ext_flags, L"CD_APPDIR") != NULL)
		{
			// 拡張フラグ CD_APPDIR が指定されている場合、実行ファイルのあるフォルダーをカレントディレクトリに設定します。
			// これによりJavaのシステムプロパティ user.dir にも実行ファイルのあるフォルダーが設定されることになります。
			wchar_t* app_dir = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
			if(app_dir == NULL) {
				exit_process(ERROR_NOT_ENOUGH_MEMORY, L"malloc");
			}
			GetModuleFileName(NULL, app_dir, MAX_LONG_PATH);
			*(wcsrchr(app_dir, L'\\')) = L'\0';
			SetCurrentDirectory(app_dir);
			free(app_dir);
		}
	}
	if(ext_flags == NULL || wcsstr(ext_flags, L"NOENCODINGFIX") == NULL)
	{
		wcscat_s(utilities, BUFFER_SIZE, UTIL_ENCODING_FIX);
	}
	if(ext_flags == NULL || wcsstr(ext_flags, L"IGNORE_UNCAUGHT_EXCEPTION") == NULL)
	{
		wcscat_s(utilities, BUFFER_SIZE, UTIL_UNCAUGHT_EXCEPTION_HANDLER);
	}
	if(ext_flags != NULL)
	{
		free(ext_flags);
		ext_flags = NULL;
	}
	//コンソールアプリケーションではイベントログ出力を有効化しません。
	//イベントログの出力にはレジストリにメッセージファイルを登録する必要があるためです。
	//wcscat_s(utilities, BUFFER_SIZE, UTIL_EVENT_LOG_HANDLER);

	vm_args_opt = from_utf8((char*)get_resource(L"VMARGS", NULL));
	create_java_vm(vm_args_opt, use_server_vm, use_side_by_side_jre, &is_security_manager_required, &error);
	if(error != JNI_OK)
	{
		get_jni_error_message(error, &result.msg_id, result.msg, BUFFER_SIZE);
		wcerr(result.msg);
		wcerr(L"\r\n");
		ExitProcess(ERROR_UNKNOWN);
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
			wcerrf(_(MSG_ID_ERR_TARGET_VERSION), target_version_string);
			wcerr(L"\r\n");
			ExitProcess(ERROR_BAD_ENVIRONMENT);
		}
		free(target_version);
	}

	if(load_main_class(argc, argv, utilities, &result) == FALSE)
	{
		wcerr(result.msg);
		wcerr(L"\r\n");

		//例外が発生している場合はスタックトレースを出力します。
		if((*env)->ExceptionCheck(env) == JNI_TRUE)
		{
			(*env)->ExceptionDescribe(env);
			(*env)->ExceptionClear(env);
		}

		ExitProcess(ERROR_INVALID_DATA);
	}
	if(synchronize_mutex_handle != NULL)
	{
		ReleaseMutex(synchronize_mutex_handle);
		CloseHandle(synchronize_mutex_handle);
		synchronize_mutex_handle = NULL;
	}

	if(is_security_manager_required)
	{
		wchar_t* msg = install_security_manager(env);
		if(msg != NULL)
		{
			wcerr(msg);
			wcerr(L"\r\n");
			ExitProcess(ERROR_UNKNOWN);
		}
	}

	if((*env)->ExceptionCheck(env) == JNI_TRUE)
	{
		(*env)->ExceptionClear(env);
	}
	(*env)->CallStaticVoidMethod(env, result.MainClass, result.MainClass_main, result.MainClass_main_args);

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

EXIT:
	if(synchronize_mutex_handle != NULL)
	{
		ReleaseMutex(synchronize_mutex_handle);
		CloseHandle(synchronize_mutex_handle);
	}
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
		destroy_java_vm();
	}

	notify_close();

	return result.msg_id;
}

static void cleanup()
{
}

static void exit_process(DWORD last_error, const wchar_t* append)
{
	wchar_t* message;

	message = get_error_message(last_error);
	if(message != NULL)
	{
		wcerr(message);
		if(append != NULL)
		{
			wcerr(L" (");
			wcerr(append);
			wcerr(L")");
		}
	}
	else if(append != NULL)
	{
		wcerr(append);
	}
	if(message != NULL || append != NULL)
	{
		wcerr(L"\r\n");
	}

	if(message != NULL)
	{
		free(message);
	}

	cleanup();
	ExitProcess(last_error);
}


DWORD uncaught_exception(JNIEnv* env, jobject thread, jthrowable throwable)
{
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
