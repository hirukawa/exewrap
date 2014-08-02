#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <process.h>
#include <shlobj.h>
#include <stdio.h>
#include <jni.h>

#include "include/jvm.h"
#include "include/notify.h"
#include "include/message.h"
#include "include/libc.h"

DWORD WINAPI CallMainMethod(void* arglist);
char** arg_split(char* buffer, int* p_argc);

void    InitResource(LPCTSTR name);
DWORD   GetResourceSize(LPCTSTR name);
jbyte*  GetResourceBuffer(LPCTSTR name);
void    OutputMessage(const char* text);

TCHAR  cache[] = "12345678901234567890123456789012";
jbyte* buffer = NULL;
DWORD  size   = 0;

jclass mainClass = NULL;
jmethodID mainMethod = NULL;

int main(int argc, char* argv[])
{
	int exit_code = 0;
	int i;
	BOOL useServerVM = FALSE;
	int err;
	LPTSTR ext_flags = NULL;
	LPTSTR relative_classpath = NULL;
	LPTSTR relative_extdirs = "lib";
	HANDLE synchronize_mutex_handle = NULL;
	LPTSTR vm_args_opt = NULL;
	DWORD version;
	LPTSTR targetVersionString;
	DWORD targetVersion;
	char* message = NULL;
	jclass urlConnectionClass;
	jclass urlStreamHandlerClass;
	jobjectArray args;

	message = (char*)HeapAlloc(GetProcessHeap(), 0, 1024);

	if(GetResourceSize("RELATIVE_CLASSPATH") > 0)
	{
		relative_classpath = (LPTSTR)GetResourceBuffer("RELATIVE_CLASSPATH");
	}
	if(GetResourceSize("EXTDIRS") > 0)
	{
		relative_extdirs = (LPTSTR)GetResourceBuffer("EXTDIRS");
	}
	if(GetResourceSize("EXTFLAGS") > 0)
	{
		ext_flags = lstrupr((LPTSTR)GetResourceBuffer("EXTFLAGS"));
	}
	if(ext_flags != NULL && lstrstr(ext_flags, "SERVER") != NULL)
	{
		useServerVM = TRUE;
	}

	InitializePath(relative_classpath, relative_extdirs, useServerVM);

	if(ext_flags != NULL && lstrstr(ext_flags, "SHARE") != NULL)
	{
		synchronize_mutex_handle = notify_exec(CallMainMethod, argc, argv);
		if(synchronize_mutex_handle == NULL)
		{
			exit_code = 0;
			goto EXIT;
		}
	}
	if(ext_flags != NULL && lstrstr(ext_flags, "SINGLE") != NULL)
	{
		if(CreateMutex(NULL, TRUE, GetModuleObjectName("SINGLE")), GetLastError() == ERROR_ALREADY_EXISTS)
		{
			exit_code = 0;
			goto EXIT;
		}
	}

	if(GetResourceSize("VMARGS") > 0)
	{
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

	version = GetJavaRuntimeVersion();
	targetVersionString = (LPTSTR)GetResourceBuffer("TARGET_VERSION");
	targetVersion = *(DWORD*)targetVersionString;
	if(targetVersion > version)
	{
		sprintf(message, _(MSG_ID_ERR_TARGET_VERSION), targetVersionString + 4);
		lstrcat(message, "\r\n");
		OutputMessage(message);
		exit_code = MSG_ID_ERR_TARGET_VERSION;
		goto EXIT;
	}

	// URLConnection
	urlConnectionClass = (*env)->DefineClass(env, "URLConnection", NULL, GetResourceBuffer("URL_CONNECTION"), GetResourceSize("URL_CONNECTION"));
	if(urlConnectionClass == NULL)
	{
		sprintf(message, _(MSG_ID_ERR_DEFINE_CLASS), "URLConnection");
		lstrcat(message, "\r\n");
		OutputMessage(message);
		exit_code = MSG_ID_ERR_DEFINE_CLASS;
		goto EXIT;
	}
	// URLStreamHandler
	urlStreamHandlerClass = (*env)->DefineClass(env, "URLStreamHandler", NULL, GetResourceBuffer("URL_STREAM_HANDLER"), GetResourceSize("URL_STREAM_HANDLER"));
	if(urlStreamHandlerClass == NULL)
	{
		sprintf(message, _(MSG_ID_ERR_DEFINE_CLASS), "URLStreamHandler");
		lstrcat(message, "\r\n");
		OutputMessage(message);
		exit_code = MSG_ID_ERR_DEFINE_CLASS;
		goto EXIT;
	}
	// Loader
	if(GetResourceBuffer("PACK_LOADER") != NULL)
	{
		// PackLoader
		jclass packLoaderClass;
		jmethodID packLoaderInit;
		jobject packLoader;
		jbyteArray classesPackGz = NULL;
		jbyteArray resourcesGz = NULL;
		jbyteArray splashPath = NULL;
		jbyteArray splashImage = NULL;
		jmethodID packLoaderInitializeMethod;

		packLoaderClass = (*env)->DefineClass(env, "PackLoader", NULL, GetResourceBuffer("PACK_LOADER"), GetResourceSize("PACK_LOADER"));
		if(packLoaderClass == NULL)
		{
			sprintf(message, _(MSG_ID_ERR_DEFINE_CLASS), "PackLoader");
			lstrcat(message, "\r\n");
			OutputMessage(message);
			exit_code = MSG_ID_ERR_DEFINE_CLASS;
			goto EXIT;
		}
		packLoaderInit = (*env)->GetMethodID(env, packLoaderClass, "<init>", "()V");
		if(packLoaderInit == NULL)
		{
			sprintf(message, _(MSG_ID_ERR_GET_METHOD), "PackLoader#init()");
			lstrcat(message, "\r\n");
			OutputMessage(message);
			exit_code = MSG_ID_ERR_GET_METHOD;
			goto EXIT;
		}
		packLoader = (*env)->NewObject(env, packLoaderClass, packLoaderInit);
		if(packLoader == NULL)
		{
			sprintf(message, _(MSG_ID_ERR_NEW_OBJECT), "PackLoader");
			lstrcat(message, "\r\n");
			exit_code = MSG_ID_ERR_NEW_OBJECT;
			goto EXIT;
		}
		if(GetResourceBuffer("CLASSES_PACK_GZ") != NULL)
		{
			classesPackGz = (*env)->NewByteArray(env, GetResourceSize("CLASSES_PACK_GZ"));
			(*env)->SetByteArrayRegion(env, classesPackGz, 0, GetResourceSize("CLASSES_PACK_GZ"), GetResourceBuffer("CLASSES_PACK_GZ"));
		}
		if(GetResourceBuffer("RESOURCES_GZ") != NULL)
		{
			resourcesGz = (*env)->NewByteArray(env, GetResourceSize("RESOURCES_GZ"));
			(*env)->SetByteArrayRegion(env, resourcesGz, 0, GetResourceSize("RESOURCES_GZ"), GetResourceBuffer("RESOURCES_GZ"));
		}
		if(GetResourceBuffer("SPLASH_PATH") != NULL)
		{
			splashPath = (*env)->NewByteArray(env, GetResourceSize("SPLASH_PATH"));
			(*env)->SetByteArrayRegion(env, splashPath, 0, GetResourceSize("SPLASH_PATH"), GetResourceBuffer("SPLASH_PATH"));
		}
		if(GetResourceBuffer("SPLATH_IMAGE") != NULL)
		{
			splashImage = (*env)->NewByteArray(env, GetResourceSize("SPLASH_IMAGE"));
			(*env)->SetByteArrayRegion(env, splashImage, 0, GetResourceSize("SPLASH_IMAGE"), GetResourceBuffer("SPLASH_IMAGE"));
		}
		packLoaderInitializeMethod = (*env)->GetMethodID(env, packLoaderClass, "initialize", "([B[B[B[B)Ljava/lang/Class;");
		if(packLoaderInitializeMethod == NULL)
		{
			sprintf(message, _(MSG_ID_ERR_GET_METHOD), "PackLoader#initialize(byte[], byte[], byte[], byte[])");
			lstrcat(message, "\r\n");
			OutputMessage(message);
			exit_code = MSG_ID_ERR_GET_METHOD;
			goto EXIT;
		}
		mainClass = (jclass)((*env)->CallObjectMethod(env, packLoader, packLoaderInitializeMethod, classesPackGz, resourcesGz, splashPath, splashImage));
	}
	else if(GetResourceBuffer("CLASSIC_LOADER") != NULL)
	{
		// ClassicLoader
		jclass classicLoaderClass;
		jmethodID classicLoaderInit;
		jobject classicLoader;
		jbyteArray jar;
		jmethodID classicLoaderInitializeMethod;

		classicLoaderClass = (*env)->DefineClass(env, "ClassicLoader", NULL, GetResourceBuffer("CLASSIC_LOADER"), GetResourceSize("CLASSIC_LOADER"));
		if(classicLoaderClass == NULL)
		{
			sprintf(message, _(MSG_ID_ERR_DEFINE_CLASS), "ClassicLoader");
			lstrcat(message, "\r\n");
			OutputMessage(message);
			exit_code = MSG_ID_ERR_DEFINE_CLASS;
			goto EXIT;
		}
		classicLoaderInit = (*env)->GetMethodID(env, classicLoaderClass, "<init>", "()V");
		if(classicLoaderInit == NULL)
		{
			sprintf(message, _(MSG_ID_ERR_GET_METHOD), "ClassicLoader#init()");
			lstrcat(message, "\r\n");
			OutputMessage(message);
			exit_code = MSG_ID_ERR_GET_METHOD;
			goto EXIT;
		}
		classicLoader = (*env)->NewObject(env, classicLoaderClass, classicLoaderInit);
		if(classicLoader == NULL)
		{
			sprintf(message, _(MSG_ID_ERR_NEW_OBJECT), "ClassicLoader");
			lstrcat(message, "\r\n");
			OutputMessage(message);
			exit_code = MSG_ID_ERR_NEW_OBJECT;
			goto EXIT;
		}
		jar = (*env)->NewByteArray(env, GetResourceSize("JAR"));
		(*env)->SetByteArrayRegion(env, jar, 0, GetResourceSize("JAR"), GetResourceBuffer("JAR"));
		classicLoaderInitializeMethod = (*env)->GetMethodID(env, classicLoaderClass, "initialize", "([B)Ljava/lang/Class;");
		if(classicLoaderInitializeMethod == NULL)
		{
			sprintf(message, _(MSG_ID_ERR_GET_METHOD), "ClassicLoader#initialize(byte[])");
			lstrcat(message, "\r\n");
			OutputMessage(message);
			exit_code = MSG_ID_ERR_GET_METHOD;
			goto EXIT;
		}
		mainClass = (jclass)((*env)->CallObjectMethod(env, classicLoader, classicLoaderInitializeMethod, jar));
	}
	else
	{
		sprintf(message, _(MSG_ID_ERR_FIND_CLASSLOADER));
		lstrcat(message, "\r\n");
		OutputMessage(message);
		exit_code = MSG_ID_ERR_FIND_CLASSLOADER;
		goto EXIT;
	}
	// Main-Class
	if(mainClass == NULL)
	{
		(*env)->ExceptionDescribe(env);
		exit_code = MSG_ID_ERR_LOAD_MAIN_CLASS;
		goto EXIT;
	}
	mainMethod = (*env)->GetStaticMethodID(env, mainClass, "main", "([Ljava/lang/String;)V");
	if(mainMethod == NULL)
	{
		sprintf(message, _(MSG_ID_ERR_FIND_MAIN_METHOD));
		lstrcat(message, "\r\n");
		OutputMessage(message);
		exit_code = MSG_ID_ERR_FIND_MAIN_METHOD;
		goto EXIT;
	}
	args = (*env)->NewObjectArray(env, argc - 1, (*env)->FindClass(env, "java/lang/String"), NULL);
	for(i = 1; i < argc; i++)
	{
		(*env)->SetObjectArrayElement(env, args, (i - 1), GetJString(env, argv[i]));
	}

	if(synchronize_mutex_handle != NULL)
	{
		ReleaseMutex(synchronize_mutex_handle);
		CloseHandle(synchronize_mutex_handle);
		synchronize_mutex_handle = NULL;
	}

	(*env)->CallStaticVoidMethod(env, mainClass, mainMethod, args);
	
	(*env)->ExceptionDescribe(env);
	(*env)->ExceptionClear(env);

EXIT:
	if(synchronize_mutex_handle != NULL)
	{
		ReleaseMutex(synchronize_mutex_handle);
		CloseHandle(synchronize_mutex_handle);
	}
	if(message != NULL)
	{
		HeapFree(GetProcessHeap(), 0, message);
	}
	if(env != NULL)
	{
		DetachJavaVM();
	}
	if(jvm != NULL)
	{
		DestroyJavaVM();
	}

	notify_close();

	return exit_code;
}

DWORD WINAPI CallMainMethod(void* _shared_memory_handle)
{
	HANDLE shared_memory_handle = (HANDLE)_shared_memory_handle;
	char* arglist;
	char* buf;
	int argc;
	char** argv = NULL;
	int i;
	LPSTR  shared_memory_read_event_name;
	HANDLE shared_memory_read_event_handle;
	LPBYTE lpShared = (LPBYTE)MapViewOfFile(shared_memory_handle, FILE_MAP_READ, 0, 0, 0);
	JNIEnv* env;
	jobjectArray args;

	arglist = (char*)(lpShared + sizeof(DWORD) + sizeof(DWORD));
	buf = (char*)HeapAlloc(GetProcessHeap(), 0, lstrlen(arglist) + 1);
	lstrcpy(buf, (char*)arglist);
	UnmapViewOfFile(lpShared);
	
	shared_memory_read_event_name = GetModuleObjectName("SHARED_MEMORY_READ");
	shared_memory_read_event_handle = OpenEvent(EVENT_MODIFY_STATE, FALSE, shared_memory_read_event_name);
	if(shared_memory_read_event_handle != NULL)
	{
		SetEvent(shared_memory_read_event_handle);
		CloseHandle(shared_memory_read_event_handle);
	}
	HeapFree(GetProcessHeap(), 0, shared_memory_read_event_name);

	argv = arg_split((char*)buf, &argc);
	HeapFree(GetProcessHeap(), 0, buf);

	env = AttachJavaVM();
	if(env == NULL)
	{
		goto EXIT;
	}
	args = (*env)->NewObjectArray(env, argc - 1, (*env)->FindClass(env, "java/lang/String"), NULL);
	for(i = 1; i < argc; i++)
	{
		(*env)->SetObjectArrayElement(env, args, (i - 1), GetJString(env, argv[i]));
	}

	(*env)->CallStaticVoidMethod(env, mainClass, mainMethod, args);
	if((*env)->ExceptionCheck(env) == JNI_TRUE)
	{
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
	}
	DetachJavaVM();

EXIT:
	if(argv != NULL)
	{
		HeapFree(GetProcessHeap(), 0, argv);
	}
	//_endthread();

	return 0;
}

char** arg_split(char* buffer, int* p_argc)
{
	int i;
	int buf_len = lstrlen(buffer);
	char** argv;

	*p_argc = 0;
	for(i = 0; i < buf_len; i++)
	{
		*p_argc += (buffer[i] == '\n')?1:0;
	}
	argv = (char**)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (*p_argc) * sizeof(char*));
	for(i = 0; i < *p_argc; i++)
	{
		argv[i] = lstrtok(i?NULL:buffer, "\n");
	}
	return argv;
}

void InitResource(LPCTSTR name)
{
	HRSRC hrsrc;

	if((hrsrc = FindResource(NULL, name, RT_RCDATA)) == NULL)
	{
		size = 0;
		buffer = NULL;
		return;
	}
	size = SizeofResource(NULL, hrsrc);
	buffer = (jbyte*)LockResource(LoadResource(NULL, hrsrc));
	lstrcpy(cache, name);
}

DWORD GetResourceSize(LPCTSTR name)
{
	if(lstrcmp(name, cache) != 0)
	{
		InitResource(name);
	}
	return size;
}

jbyte* GetResourceBuffer(LPCTSTR name)
{
	if(lstrcmp(name, cache) != 0)
	{
		InitResource(name);
	}
	return buffer;
}

void OutputMessage(const char* text)
{
	int size = lstrlen(text);
	DWORD written;

	WriteConsole(GetStdHandle(STD_ERROR_HANDLE), text, size, &written, NULL); 
}
