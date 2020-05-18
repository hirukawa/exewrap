/* このファイルの文字コードは Shift_JIS (MS932) です。*/

#ifndef _LOADER_H_
#define _LOADER_H_

#include <Windows.h>
#include <jni.h>

#define UTIL_ENCODING_FIX               L"ENCODING-FIX;"
#define UTIL_UNCAUGHT_EXCEPTION_HANDLER L"UncaughtExceptionHandler;"
#define UTIL_FILE_LOG_STREAM            L"FileLogStream;"
#define UTIL_EVENT_LOG_HANDLER          L"EventLogHandler;"

#define LOAD_RESULT_MAX_MESSAGE_LENGTH 32678


typedef struct _RESOURCE {
	BYTE* buf;
	DWORD len;
} RESOURCE;

typedef struct _LOAD_RESULT {
	jclass       MainClass;
	jmethodID    MainClass_main;
	jobjectArray MainClass_main_args;
	int          msg_id;
	wchar_t*     msg;
} LOAD_RESULT;


#ifdef __cplusplus
extern "C" {
#endif

extern wchar_t*    install_security_manager(JNIEnv* env);
extern BOOL        load_main_class(int argc, const wchar_t* argv[], const wchar_t* utilities, LOAD_RESULT* result);
extern BOOL        set_splash_screen_resource(const wchar_t* splash_screen_name, const BYTE* splash_screen_image_buf, DWORD splash_screen_image_len);
extern wchar_t*    get_module_object_name(const wchar_t* prefix);
extern BYTE*       get_resource(const wchar_t* name, RESOURCE* resource);
extern wchar_t*    get_jni_error_message(int error, int* exit_code, wchar_t* buf, size_t len);
extern wchar_t*    get_exception_message(JNIEnv* env, jthrowable throwable);
extern wchar_t*    get_stack_trace(JNIEnv* env, jstring thread, jthrowable throwable);
extern UINT WINAPI remote_call_main_method(const void* shared_memory_handle);
extern DWORD       uncaught_exception(JNIEnv* env, jobject thread, jthrowable throwable);


#ifdef __cplusplus
}
#endif


#endif /* _LOADER_H_ */
