#ifndef _LOADER_H_
#define _LOADER_H_

#include <windows.h>
#include <jni.h>

#define UTIL_UNCAUGHT_EXCEPTION_HANDLER "UncaughtExceptionHandler;"
#define UTIL_FILE_LOG_STREAM            "FileLogStream;"
#define UTIL_EVENT_LOG_STREAM           "EventLogStream;"
#define UTIL_EVENT_LOG_HANDLER          "EventLogHandler;"

typedef struct _RESOURCE {
	BYTE* buf;
	DWORD len;
} RESOURCE;

typedef struct _LOAD_RESULT {
	jclass       MainClass;
	jmethodID    MainClass_main;
	jobjectArray MainClass_main_args;
	int          msg_id;
	char*        msg;
} LOAD_RESULT;

#ifdef __cplusplus
extern "C" {
#endif

extern BOOL         LoadMainClass(int argc, char* argv[], char* utilities, LOAD_RESULT* result);
extern BOOL         SetSplashScreenResource(char* splash_screen_name, BYTE* splash_screen_image_buf, DWORD splash_screen_image_len);
extern DWORD WINAPI RemoteCallMainMethod(void* _shared_memory_handle);
extern char*        ToString(JNIEnv* env, jobject object, char* buf);
extern char*        GetModuleObjectName(const char* prefix);
extern BYTE*        GetResource(LPCTSTR name, RESOURCE* resource);
extern char*        GetWinErrorMessage(DWORD err, int* exit_code, char* buf);
extern char*        GetJniErrorMessage(int err, int* exit_code, char* buf);
extern void JNICALL JNI_WriteEventLog(JNIEnv *env, jobject clazz, jint logType, jstring message);
extern void JNICALL JNI_UncaughtException(JNIEnv *env, jobject clazz, jstring thread, jstring message, jstring trace);

#ifdef __cplusplus
}
#endif

#endif
