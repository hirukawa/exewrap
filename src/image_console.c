#include <windows.h>
#include <process.h>
#include <shlobj.h>
#include <stdio.h>
#include <jni.h>

#include "include/jvm.h"
#include "include/notify.h"
#include "include/startup.h"
#include "include/message.h"

void CallMainMethod(void* arglist);
char** arg_split(char* buffer, int* p_argc);

void    InitResource(LPCTSTR name);
DWORD   GetResourceSize(LPCTSTR name);
jbyte*  GetResourceBuffer(LPCTSTR name);

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

	LPTSTR vm_args_opt = NULL;
	if(GetResourceSize("VMARGS") > 0) {
		vm_args_opt = (LPTSTR)GetResourceBuffer("VMARGS");
	}
	CreateJavaVM(vm_args_opt, useServerVM, &err);
	switch(err) {
	case JNI_OK:
		break;
	case JNI_ERR: /* unknown error */
		fprintf(stderr, _(MSG_ID_ERR_CREATE_JVM_UNKNOWN));
		exit_code = MSG_ID_ERR_CREATE_JVM_UNKNOWN;
		goto EXIT;
	case JNI_EDETACHED: /* thread detached from the VM */
		fprintf(stderr, _(MSG_ID_ERR_CREATE_JVM_EDETACHED));
		exit_code = MSG_ID_ERR_CREATE_JVM_EDETACHED;
		goto EXIT;
	case JNI_EVERSION: /* JNI version error */
		fprintf(stderr, _(MSG_ID_ERR_CREATE_JVM_EVERSION));
		exit_code = MSG_ID_ERR_CREATE_JVM_EVERSION;
		goto EXIT;
	case JNI_ENOMEM: /* not enough memory */
		fprintf(stderr, _(MSG_ID_ERR_CREATE_JVM_ENOMEM));
		exit_code = MSG_ID_ERR_CREATE_JVM_ENOMEM;
		goto EXIT;
	case JNI_EEXIST: /* VM already created */
		fprintf(stderr, _(MSG_ID_ERR_CREATE_JVM_EEXIST));
		exit_code = MSG_ID_ERR_CREATE_JVM_EEXIST;
		goto EXIT;
	case JNI_EINVAL: /* invalid arguments */
		fprintf(stderr, _(MSG_ID_ERR_CREATE_JVM_EINVAL));
		exit_code = MSG_ID_ERR_CREATE_JVM_EINVAL;
		goto EXIT;
	case JVM_ELOADLIB:
		fprintf(stderr, _(MSG_ID_ERR_CREATE_JVM_ELOADLIB));
		exit_code = MSG_ID_ERR_CREATE_JVM_ELOADLIB;
		goto EXIT;
	default:
		fprintf(stderr, _(MSG_ID_ERR_CREATE_JVM_UNKNOWN));
		exit_code = MSG_ID_ERR_CREATE_JVM_UNKNOWN;
		goto EXIT;
	}

	DWORD version = GetJavaRuntimeVersion();
	LPTSTR targetVersionString = (LPTSTR)GetResourceBuffer("TARGET_VERSION");
	DWORD targetVersion = *(DWORD*)targetVersionString;
	if(targetVersion > version) {
		fprintf(stderr, _(MSG_ID_ERR_TARGET_VERSION), targetVersionString + 4);
		fprintf(stderr, "\r\n");
		exit_code = MSG_ID_ERR_TARGET_VERSION;
		goto EXIT;
	}

	// URLConnection
	jclass urlConnectionClass = (*env)->DefineClass(env, "URLConnection", NULL, GetResourceBuffer("URL_CONNECTION"), GetResourceSize("URL_CONNECTION"));
	if(urlConnectionClass == NULL) {
		fprintf(stderr, _(MSG_ID_ERR_DEFINE_CLASS), "URLConnection");
		fprintf(stderr, "\r\n");
		exit_code = MSG_ID_ERR_DEFINE_CLASS;
		goto EXIT;
	}
	// URLStreamHandler
	jclass urlStreamHandlerClass = (*env)->DefineClass(env, "URLStreamHandler", NULL, GetResourceBuffer("URL_STREAM_HANDLER"), GetResourceSize("URL_STREAM_HANDLER"));
	if(urlStreamHandlerClass == NULL) {
		fprintf(stderr, _(MSG_ID_ERR_DEFINE_CLASS), "URLStreamHandler");
		fprintf(stderr, "\r\n");
		exit_code = MSG_ID_ERR_DEFINE_CLASS;
		goto EXIT;
	}
	// Loader
	if(GetResourceBuffer("PACK_LOADER") != NULL) {
		// PackLoader
		jclass packLoaderClass = (*env)->DefineClass(env, "PackLoader", NULL, GetResourceBuffer("PACK_LOADER"), GetResourceSize("PACK_LOADER"));
		if(packLoaderClass == NULL) {
			fprintf(stderr, _(MSG_ID_ERR_DEFINE_CLASS), "PackLoader");
			fprintf(stderr, "\r\n");
			exit_code = MSG_ID_ERR_DEFINE_CLASS;
			goto EXIT;
		}
		jmethodID packLoaderInit = (*env)->GetMethodID(env, packLoaderClass, "<init>", "()V");
		if(packLoaderInit == NULL) {
			fprintf(stderr, _(MSG_ID_ERR_GET_METHOD), "PackLoader#init()");
			fprintf(stderr, "\r\n");
			exit_code = MSG_ID_ERR_GET_METHOD;
			goto EXIT;
		}
		jobject packLoader = (*env)->NewObject(env, packLoaderClass, packLoaderInit);
		if(packLoader == NULL) {
			fprintf(stderr, _(MSG_ID_ERR_NEW_OBJECT), "PackLoader");
			fprintf(stderr, "\r\n");
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
			fprintf(stderr, _(MSG_ID_ERR_GET_METHOD), "PackLoader#initialize(byte[], byte[], byte[], byte[])");
			fprintf(stderr, "\r\n");
			exit_code = MSG_ID_ERR_GET_METHOD;
			goto EXIT;
		}
		mainClass = (jclass)((*env)->CallObjectMethod(env, packLoader, packLoaderInitializeMethod, classesPackGz, resourcesGz, splashPath, splashImage));
	} else if(GetResourceBuffer("CLASSIC_LOADER") != NULL) {
		// ClassicLoader
		jclass classicLoaderClass = (*env)->DefineClass(env, "ClassicLoader", NULL, GetResourceBuffer("CLASSIC_LOADER"), GetResourceSize("CLASSIC_LOADER"));
		if(classicLoaderClass == NULL) {
			fprintf(stderr, _(MSG_ID_ERR_DEFINE_CLASS), "ClassicLoader");
			fprintf(stderr, "\r\n");
			exit_code = MSG_ID_ERR_DEFINE_CLASS;
			goto EXIT;
		}
		jmethodID classicLoaderInit = (*env)->GetMethodID(env, classicLoaderClass, "<init>", "()V");
		if(classicLoaderInit == NULL) {
			fprintf(stderr, _(MSG_ID_ERR_GET_METHOD), "ClassicLoader#init()");
			fprintf(stderr, "\r\n");
			exit_code = MSG_ID_ERR_GET_METHOD;
			goto EXIT;
		}
		jobject classicLoader = (*env)->NewObject(env, classicLoaderClass, classicLoaderInit);
		if(classicLoader == NULL) {
			fprintf(stderr, _(MSG_ID_ERR_NEW_OBJECT), "ClassicLoader");
			fprintf(stderr, "\r\n");
			exit_code = MSG_ID_ERR_NEW_OBJECT;
			goto EXIT;
		}
		jbyteArray jar = (*env)->NewByteArray(env, GetResourceSize("JAR"));
		(*env)->SetByteArrayRegion(env, jar, 0, GetResourceSize("JAR"), GetResourceBuffer("JAR"));
		jmethodID classicLoaderInitializeMethod = (*env)->GetMethodID(env, classicLoaderClass, "initialize", "([B)Ljava/lang/Class;");
		if(classicLoaderInitializeMethod == NULL) {
			fprintf(stderr, _(MSG_ID_ERR_GET_METHOD), "ClassicLoader#initialize(byte[])");
			fprintf(stderr, "\r\n");
			exit_code = MSG_ID_ERR_GET_METHOD;
			goto EXIT;
		}
		mainClass = (jclass)((*env)->CallObjectMethod(env, classicLoader, classicLoaderInitializeMethod, jar));
	} else {
		fprintf(stderr, _(MSG_ID_ERR_FIND_CLASSLOADER));
		fprintf(stderr, "\r\n");
		exit_code = MSG_ID_ERR_FIND_CLASSLOADER;
		goto EXIT;
	}
	// Main-Class
	if(mainClass == NULL) {
		(*env)->ExceptionDescribe(env);
		exit_code = MSG_ID_ERR_LOAD_MAIN_CLASS;
		goto EXIT;
	}
	mainMethod = (*env)->GetStaticMethodID(env, mainClass, "main", "([Ljava/lang/String;)V");
	if(mainMethod == NULL) {
		fprintf(stderr, _(MSG_ID_ERR_FIND_MAIN_METHOD));
		fprintf(stderr, "\r\n");
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
	
	(*env)->ExceptionDescribe(env);
	(*env)->ExceptionClear(env);

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

	argv = arg_split((char*)buf, &argc);
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
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
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
