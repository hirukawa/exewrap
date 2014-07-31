#include <windows.h>
#include <process.h>
#include <shlobj.h>
#include <stdio.h>
#include <jni.h>

#include "include/jvm.h"
#include "include/notify.h"
#include "include/message.h"

void CallMainMethod(void* arglist);
char** arg_split(char* buffer, int* p_argc);

void    InitResource(LPCTSTR name);
DWORD   GetResourceSize(LPCTSTR name);
jbyte*  GetResourceBuffer(LPCTSTR name);
void    OutputError(LPCTSTR text);
void    OutputMessage(LPCTSTR text);
LPSTR	GetModuleMutexName();

typedef void (*SplashInit_t)(void);
typedef int  (*SplashLoadMemory_t)(void* pdata, int size);
SplashInit_t SplashInit;
SplashLoadMemory_t SplashLoadMemory;

void JNICALL JNI_UncaughtException(JNIEnv *env, jobject clazz, jstring message, jstring trace);

HMODULE splashscreendll;

TCHAR  cache[] = "12345678901234567890123456789012";
jbyte* buffer = NULL;
DWORD  size   = 0;

jclass mainClass = NULL;
jmethodID mainMethod = NULL;

int _main(int argc, char* argv[]) {
	int exit_code = 0;
	int i;
	BOOL useServerVM = FALSE;
	int err;
	LPTSTR ext_flags = NULL;
	MSG msg;
	LPTSTR relative_classpath = NULL;
	LPTSTR relative_extdirs = "lib";
	HANDLE synchronize_mutex_handle = NULL;
	
	if(GetResourceSize("RELATIVE_CLASSPATH") > 0) {
		relative_classpath = (LPTSTR)GetResourceBuffer("RELATIVE_CLASSPATH");
	}
	if(GetResourceSize("EXTDIRS") > 0) {
		relative_extdirs = (LPTSTR)GetResourceBuffer("EXTDIRS");
	}
	if(GetResourceSize("EXTFLAGS") > 0) {
		ext_flags = _strupr((LPTSTR)GetResourceBuffer("EXTFLAGS"));
	}
	if(ext_flags != NULL && strstr(ext_flags, "SERVER") != NULL) {
		useServerVM = TRUE;
	}

	InitializePath(relative_classpath, relative_extdirs, useServerVM);

	if(ext_flags != NULL && strstr(ext_flags, "SHARE") != NULL) {
		synchronize_mutex_handle = notify_exec(CallMainMethod, argc, argv);
		if(synchronize_mutex_handle == NULL) {
			exit_code = 0;
			goto EXIT;
		}
	}
	if(ext_flags != NULL && strstr(ext_flags, "SINGLE") != NULL) {
		if(CreateMutex(NULL, TRUE, GetModuleObjectName("SINGLE")), GetLastError() == ERROR_ALREADY_EXISTS) {
			exit_code = 0;
			goto EXIT;
		}
	}
	
	// Display Splash Screen
	int splash_size = GetResourceSize("SPLASH_IMAGE");
	void* splash_data = NULL;
	if(splash_size > 0) {
		if((splash_data = GetResourceBuffer("SPLASH_IMAGE")) != NULL) {
			if((splashscreendll = LoadLibrary("splashscreen.dll")) != NULL) {
				SplashInit = (SplashInit_t)GetProcAddress(splashscreendll, "SplashInit");
				SplashLoadMemory = (SplashLoadMemory_t)GetProcAddress(splashscreendll, "SplashLoadMemory");
				if(SplashInit != NULL && SplashLoadMemory != NULL) {
					SplashInit();
					SplashLoadMemory(splash_data, splash_size);
				}
			}
		}
	}

	LPTSTR vm_args_opt = NULL;
	if(GetResourceSize("VMARGS") > 0) {
		vm_args_opt = (LPTSTR)GetResourceBuffer("VMARGS");
	}
	CreateJavaVM(vm_args_opt, useServerVM, &err);
	switch(err) {
	case JNI_OK:
		break;
	case JNI_ERR: /* unknown error */
		OutputMessage(_(MSG_ID_ERR_CREATE_JVM_UNKNOWN));
		exit_code = MSG_ID_ERR_CREATE_JVM_UNKNOWN;
		goto EXIT;
	case JNI_EDETACHED: /* thread detached from the VM */
		OutputMessage(_(MSG_ID_ERR_CREATE_JVM_EDETACHED));
		exit_code = MSG_ID_ERR_CREATE_JVM_EDETACHED;
		goto EXIT;
	case JNI_EVERSION: /* JNI version error */
		OutputMessage(_(MSG_ID_ERR_CREATE_JVM_EVERSION));
		exit_code = MSG_ID_ERR_CREATE_JVM_EVERSION;
		goto EXIT;
	case JNI_ENOMEM: /* not enough memory */
		OutputMessage(_(MSG_ID_ERR_CREATE_JVM_ENOMEM));
		exit_code = MSG_ID_ERR_CREATE_JVM_ENOMEM;
		goto EXIT;
	case JNI_EEXIST: /* VM already created */
		OutputMessage(_(MSG_ID_ERR_CREATE_JVM_EEXIST));
		exit_code = MSG_ID_ERR_CREATE_JVM_EEXIST;
		goto EXIT;
	case JNI_EINVAL: /* invalid arguments */
		OutputMessage(_(MSG_ID_ERR_CREATE_JVM_EINVAL));
		exit_code = MSG_ID_ERR_CREATE_JVM_EINVAL;
		goto EXIT;
	case JVM_ELOADLIB:
		OutputMessage(_(MSG_ID_ERR_CREATE_JVM_ELOADLIB));
		exit_code = MSG_ID_ERR_CREATE_JVM_ELOADLIB;
		goto EXIT;
	default:
		OutputMessage(_(MSG_ID_ERR_CREATE_JVM_UNKNOWN));
		exit_code = MSG_ID_ERR_CREATE_JVM_UNKNOWN;
		goto EXIT;
	}
	
	DWORD version = GetJavaRuntimeVersion();
	LPTSTR targetVersionString = (LPTSTR)GetResourceBuffer("TARGET_VERSION");
	DWORD targetVersion = *(DWORD*)targetVersionString;
	LPTSTR message;
	if(targetVersion > version) {
		message = (LPTSTR)HeapAlloc(GetProcessHeap(), 0, 256);
		sprintf(message, _(MSG_ID_ERR_TARGET_VERSION), targetVersionString + 4);
		OutputMessage(message);
		exit_code = MSG_ID_ERR_TARGET_VERSION;
		goto EXIT;
	}
	
	// URLConnection
	jclass urlConnectionClass = (*env)->DefineClass(env, "URLConnection", NULL, GetResourceBuffer("URL_CONNECTION"), GetResourceSize("URL_CONNECTION"));
	if(urlConnectionClass == NULL) {
		message = (LPTSTR)HeapAlloc(GetProcessHeap(), 0, 256);
		sprintf(message, _(MSG_ID_ERR_DEFINE_CLASS), "URLConnection");
		OutputMessage(message);
		exit_code = MSG_ID_ERR_DEFINE_CLASS;
		goto EXIT;
	}
	// URLStreamHandler
	jclass urlStreamHandlerClass = (*env)->DefineClass(env, "URLStreamHandler", NULL, GetResourceBuffer("URL_STREAM_HANDLER"), GetResourceSize("URL_STREAM_HANDLER"));
	if(urlStreamHandlerClass == NULL) {
		message = (LPTSTR)HeapAlloc(GetProcessHeap(), 0, 256);
		sprintf(message, _(MSG_ID_ERR_DEFINE_CLASS), "URLStreamHandler");
		OutputMessage(message);
		exit_code = MSG_ID_ERR_DEFINE_CLASS;
		goto EXIT;
	}
	
	// FileLogStream
	jclass    fileLogStreamClass;
	jmethodID fileLogStreamInit;
	char      moduleFileName[MAX_PATH];
	if(ext_flags == NULL || strstr(ext_flags, "NOLOG") == NULL) {
		if((fileLogStreamClass = (*env)->DefineClass(env, "FileLogStream", NULL, GetResourceBuffer("FILELOG_STREAM"), GetResourceSize("FILELOG_STREAM"))) != NULL) {
			if((fileLogStreamInit = (*env)->GetStaticMethodID(env, fileLogStreamClass, "initialize", "(Ljava/lang/String;)V"))!= NULL) {
				GetModuleFileName(NULL, moduleFileName, MAX_PATH);
				(*env)->CallStaticVoidMethod(env, fileLogStreamClass, fileLogStreamInit, GetJString(env, moduleFileName));
				if((*env)->ExceptionCheck(env)) {
					(*env)->ExceptionClear(env);
				}
			}
		}
	}
	// UncaughtHandler (version 1.5.0 or higher only)
	if(version >= 0x01050000) {
		jclass uncaughtHandlerClass;
		jmethodID uncaughtHandlerInit;
		if((uncaughtHandlerClass = (*env)->DefineClass(env, "UncaughtHandler", NULL, GetResourceBuffer("UNCAUGHT_HANDLER"), GetResourceSize("UNCAUGHT_HANDLER"))) != NULL) {
			if((uncaughtHandlerInit = (*env)->GetStaticMethodID(env, uncaughtHandlerClass, "initialize", "()V"))!= NULL) {
				(*env)->CallStaticVoidMethod(env, uncaughtHandlerClass, uncaughtHandlerInit);
				JNINativeMethod	nm;
				nm.name = (char*)"UncaughtException";
				nm.signature = (char*)"(Ljava/lang/String;Ljava/lang/String;)V";
				nm.fnPtr = (void*)JNI_UncaughtException;
				if(((*env)->RegisterNatives(env, uncaughtHandlerClass, &nm, 1)) != 0) {
					message = (LPTSTR)HeapAlloc(GetProcessHeap(), 0, 256);
					sprintf(message, _(MSG_ID_ERR_REGISTER_NATIVE), "UncaughtException");
					OutputMessage(message);
					exit_code = MSG_ID_ERR_REGISTER_NATIVE;
					goto EXIT;
				}
			}
		}
	}
	
	// Loader
	if(GetResourceBuffer("PACK_LOADER") != NULL) {
		// PackLoader
		jclass packLoaderClass = (*env)->DefineClass(env, "PackLoader", NULL, GetResourceBuffer("PACK_LOADER"), GetResourceSize("PACK_LOADER"));
		if(packLoaderClass == NULL) {
			message = (LPTSTR)HeapAlloc(GetProcessHeap(), 0, 256);
			sprintf(message, _(MSG_ID_ERR_DEFINE_CLASS), "PackLoader");
			OutputMessage(message);
			exit_code = MSG_ID_ERR_DEFINE_CLASS;
			goto EXIT;
		}
		jmethodID packLoaderInit = (*env)->GetMethodID(env, packLoaderClass, "<init>", "()V");
		if(packLoaderInit == NULL) {
			message = (LPTSTR)HeapAlloc(GetProcessHeap(), 0, 256);
			sprintf(message, _(MSG_ID_ERR_GET_METHOD), "PackLoader#init()");
			OutputMessage(message);
			exit_code = MSG_ID_ERR_GET_METHOD;
			goto EXIT;
		}
		jobject packLoader = (*env)->NewObject(env, packLoaderClass, packLoaderInit);
		if(packLoader == NULL) {
			message = (LPTSTR)HeapAlloc(GetProcessHeap(), 0, 256);
			sprintf(message, _(MSG_ID_ERR_NEW_OBJECT), "PackLoader");
			OutputMessage(message);
			exit_code = MSG_ID_ERR_NEW_OBJECT;
			goto EXIT;
		}
		jbyteArray classesPackGz = NULL;
		jbyteArray resourcesGz = NULL;
		jbyteArray splashPath = NULL;
		jbyteArray splashImage = NULL;
		if(GetResourceBuffer("CLASSES_PACK_GZ") != NULL) {
			classesPackGz = (*env)->NewByteArray(env, GetResourceSize("CLASSES_PACK_GZ"));
			(*env)->SetByteArrayRegion(env, classesPackGz, 0, GetResourceSize("CLASSES_PACK_GZ"), GetResourceBuffer("CLASSES_PACK_GZ"));
		}
		if(GetResourceBuffer("RESOURCES_GZ") != NULL) {
			resourcesGz = (*env)->NewByteArray(env, GetResourceSize("RESOURCES_GZ"));
			(*env)->SetByteArrayRegion(env, resourcesGz, 0, GetResourceSize("RESOURCES_GZ"), GetResourceBuffer("RESOURCES_GZ"));
		}
		if(GetResourceBuffer("SPLASH_PATH") != NULL) {
			splashPath = (*env)->NewByteArray(env, GetResourceSize("SPLASH_PATH"));
			(*env)->SetByteArrayRegion(env, splashPath, 0, GetResourceSize("SPLASH_PATH"), GetResourceBuffer("SPLASH_PATH"));
		}
		if(GetResourceBuffer("SPLATH_IMAGE") != NULL) {
			splashImage = (*env)->NewByteArray(env, GetResourceSize("SPLASH_IMAGE"));
			(*env)->SetByteArrayRegion(env, splashImage, 0, GetResourceSize("SPLASH_IMAGE"), GetResourceBuffer("SPLASH_IMAGE"));
		}
		jmethodID packLoaderInitializeMethod = (*env)->GetMethodID(env, packLoaderClass, "initialize", "([B[B[B[B)Ljava/lang/Class;");
		if(packLoaderInitializeMethod == NULL) {
			message = (LPTSTR)HeapAlloc(GetProcessHeap(), 0, 256);
			sprintf(message, _(MSG_ID_ERR_GET_METHOD), "PackLoader#initialize(byte[], byte[], byte[], byte[])");
			OutputMessage(message);
			exit_code = MSG_ID_ERR_GET_METHOD;
			goto EXIT;
		}
		mainClass = (jclass)((*env)->CallObjectMethod(env, packLoader, packLoaderInitializeMethod, classesPackGz, resourcesGz, splashPath, splashImage));
	} else if(GetResourceBuffer("CLASSIC_LOADER") != NULL) {
		// ClassicLoader
		jclass classicLoaderClass = (*env)->DefineClass(env, "ClassicLoader", NULL, GetResourceBuffer("CLASSIC_LOADER"), GetResourceSize("CLASSIC_LOADER"));
		if(classicLoaderClass == NULL) {
			message = (LPTSTR)HeapAlloc(GetProcessHeap(), 0, 256);
			sprintf(message, _(MSG_ID_ERR_DEFINE_CLASS), "ClassicLoader");
			OutputMessage(message);
			exit_code = MSG_ID_ERR_DEFINE_CLASS;
			goto EXIT;
		}
		jmethodID classicLoaderInit = (*env)->GetMethodID(env, classicLoaderClass, "<init>", "()V");
		if(classicLoaderInit == NULL) {
			message = (LPTSTR)HeapAlloc(GetProcessHeap(), 0, 256);
			sprintf(message, _(MSG_ID_ERR_GET_METHOD), "ClassicLoader#init()");
			OutputMessage(message);
			exit_code = MSG_ID_ERR_GET_METHOD;
			goto EXIT;
		}
		jobject classicLoader = (*env)->NewObject(env, classicLoaderClass, classicLoaderInit);
		if(classicLoader == NULL) {
			message = (LPTSTR)HeapAlloc(GetProcessHeap(), 0, 256);
			sprintf(message, _(MSG_ID_ERR_NEW_OBJECT), "ClassicLoader");
			OutputMessage(message);
			exit_code = MSG_ID_ERR_NEW_OBJECT;
			goto EXIT;
		}
		jbyteArray jar = (*env)->NewByteArray(env, GetResourceSize("JAR"));
		(*env)->SetByteArrayRegion(env, jar, 0, GetResourceSize("JAR"), GetResourceBuffer("JAR"));
		jmethodID classicLoaderInitializeMethod = (*env)->GetMethodID(env, classicLoaderClass, "initialize", "([B)Ljava/lang/Class;");
		if(classicLoaderInitializeMethod == NULL) {
			message = (LPTSTR)HeapAlloc(GetProcessHeap(), 0, 256);
			sprintf(message, _(MSG_ID_ERR_GET_METHOD), "ClassicLoader#initialize(byte[])");
			OutputMessage(message);
			exit_code = MSG_ID_ERR_GET_METHOD;
			goto EXIT;
		}
		mainClass = (jclass)((*env)->CallObjectMethod(env, classicLoader, classicLoaderInitializeMethod, jar));
	} else {
		OutputMessage(_(MSG_ID_ERR_FIND_CLASSLOADER));
		exit_code = MSG_ID_ERR_FIND_CLASSLOADER;
		goto EXIT;
	}
	
	// Main-Class
	if(mainClass == NULL) {
		message = (LPTSTR)HeapAlloc(GetProcessHeap(), 0, 256);
		sprintf(message, "%s\r\n", _(MSG_ID_ERR_LOAD_MAIN_CLASS));
		OutputError(message);
		exit_code = MSG_ID_ERR_LOAD_MAIN_CLASS;
		goto EXIT;
	}
	mainMethod = (*env)->GetStaticMethodID(env, mainClass, "main", "([Ljava/lang/String;)V");
	if(mainMethod == NULL) {
		OutputMessage(_(MSG_ID_ERR_FIND_MAIN_METHOD));
		exit_code = MSG_ID_ERR_FIND_MAIN_METHOD;
		goto EXIT;
	}
	jobjectArray args = (*env)->NewObjectArray(env, argc - 1, (*env)->FindClass(env, "java/lang/String"), NULL);
	for(i = 1; i < argc; i++) {
		(*env)->SetObjectArrayElement(env, args, (i - 1), GetJString(env, argv[i]));
	}
	
	if(synchronize_mutex_handle != NULL) {
		ReleaseMutex(synchronize_mutex_handle);
		CloseHandle(synchronize_mutex_handle);
		synchronize_mutex_handle = NULL;
	}

	(*env)->CallStaticVoidMethod(env, mainClass, mainMethod, args);
	
	if((*env)->ExceptionCheck(env) == JNI_TRUE) {
		OutputError(NULL);
		exit_code = 1;
		goto EXIT;
	}

EXIT:
	notify_close();

	if(synchronize_mutex_handle != NULL) {
		ReleaseMutex(synchronize_mutex_handle);
		CloseHandle(synchronize_mutex_handle);
	}
	if(env != NULL) {
		DetachJavaVM();
	}
	if(jvm != NULL) {
		DestroyJavaVM();
	}
	return exit_code;
}

void CallMainMethod(void* _shared_memory_handle) {
	HANDLE shared_memory_handle = (HANDLE)_shared_memory_handle;
	char* arglist;
	char* buf;
	int argc;
	char** argv;
	int i;
	LPSTR  shared_memory_read_event_name;
	HANDLE shared_memory_read_event_handle;
	
	LPBYTE lpShared = (LPBYTE)MapViewOfFile(shared_memory_handle, FILE_MAP_READ, 0, 0, 0);
	arglist = (char*)(lpShared + sizeof(DWORD) + sizeof(DWORD));
	buf = (char*)HeapAlloc(GetProcessHeap(), 0, strlen(arglist) + 1);
	strcpy(buf, (char*)arglist);
	UnmapViewOfFile(lpShared);

	shared_memory_read_event_name = GetModuleObjectName("SHARED_MEMORY_READ");
	shared_memory_read_event_handle = OpenEvent(EVENT_MODIFY_STATE, FALSE, shared_memory_read_event_name);
	if(shared_memory_read_event_handle != NULL) {
		SetEvent(shared_memory_read_event_handle);
		CloseHandle(shared_memory_read_event_handle);
	}
	HeapFree(GetProcessHeap(), 0, shared_memory_read_event_name);

	argv = arg_split(buf, &argc);
	HeapFree(GetProcessHeap(), 0, buf);
	JNIEnv* env = AttachJavaVM();
	if(env == NULL) {
		HeapFree(GetProcessHeap(), 0, argv);
		goto EXIT;
	}
	jobjectArray args = (*env)->NewObjectArray(env, argc - 1, (*env)->FindClass(env, "java/lang/String"), NULL);
	for(i = 1; i < argc; i++) {
		(*env)->SetObjectArrayElement(env, args, (i - 1), GetJString(env, argv[i]));
	}
	HeapFree(GetProcessHeap(), 0, argv);
	
	(*env)->CallStaticVoidMethod(env, mainClass, mainMethod, args);
	if((*env)->ExceptionCheck(env) == JNI_TRUE) {
		OutputError(NULL);
	}
	DetachJavaVM();

EXIT:
	_endthread();
}

char** arg_split(char* buffer, int* p_argc) {
	*p_argc = 0;
	int i;
	for(i = 0; i < strlen(buffer); i++) {
		*p_argc += (buffer[i] == '\n')?1:0;
	}
	char** argv = (char**)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (*p_argc) * sizeof(char*));
	for(i = 0; i < *p_argc; i++) {
		argv[i] = strtok(i?NULL:buffer, "\n");
	}
	return argv;
}

void InitResource(LPCTSTR name) {
	HRSRC hrsrc;

	if((hrsrc = FindResource(NULL, name, RT_RCDATA)) == NULL) {
		size = 0;
		buffer = NULL;
		return;
	}
	size = SizeofResource(NULL, hrsrc);
	buffer = (jbyte*)LockResource(LoadResource(NULL, hrsrc));
	strcpy(cache, name);
}

DWORD GetResourceSize(LPCTSTR name) {
	if(strcmp(name, cache) != 0) {
		InitResource(name);
	}
	return size;
}

jbyte* GetResourceBuffer(LPCTSTR name) {
	if(strcmp(name, cache) != 0) {
		InitResource(name);
	}
	return buffer;
}

void OutputError(LPCTSTR text) {
	char buffer[2048];

	if((*env)->ExceptionCheck(env) != JNI_TRUE) {
		OutputMessage(text);
		return;
	}

	jthrowable throwable = (*env)->ExceptionOccurred(env);
	(*env)->ExceptionDescribe(env);
	(*env)->ExceptionClear(env);
	LPSTR message = GetShiftJIS(env, (jstring)(*env)->CallObjectMethod(env, throwable,
		(*env)->GetMethodID(env, (*env)->FindClass(env, "java/lang/Throwable"), "toString", "()Ljava/lang/String;"))
	);
	char* pos;
	if(text != NULL) {
		strcpy(buffer, text);
		strcat(buffer, "\r\n");
		strcat(buffer, message);
		OutputMessage(buffer);
	} else {
		OutputMessage(message);
	}
	HeapFree(GetProcessHeap(), 0, message);
}

void OutputMessage(LPCTSTR text) {
	char* filename;
	char buffer[MAX_PATH];
	GetModuleFileName(NULL, buffer, MAX_PATH);
	filename = strrchr(buffer, '\\') + 1;

	MessageBox(NULL, text, filename, MB_ICONEXCLAMATION | MB_APPLMODAL | MB_OK | MB_SETFOREGROUND);
}

void JNICALL JNI_UncaughtException(JNIEnv *env, jobject clazz, jstring message, jstring trace) {
	OutputMessage(GetShiftJIS(env, message));
	exit(0);
}
