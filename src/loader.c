/* 日本語 このファイルの文字コードは Shift_JIS (MS932) です。*/

#include <windows.h>
#include <process.h>
#include <shlobj.h>
#include <stdio.h>
#include <jni.h>

#include "include/jvm.h"
#include "include/loader.h"
#include "include/eventlog.h"
#include "include/message.h"

#define BUFFER_SIZE    32768
#define MAX_LONG_PATH  32768

wchar_t*         install_security_manager(JNIEnv* env);
BOOL             load_main_class(int argc, const wchar_t* argv[], const wchar_t* utilities, LOAD_RESULT* result);
BOOL             set_splash_screen_resource(const wchar_t* splash_screen_name, const BYTE* splash_screen_image_buf, DWORD splash_screen_image_len);
wchar_t*         get_module_object_name(const wchar_t* prefix);
BYTE*            get_resource(const wchar_t* name, RESOURCE* resource);
wchar_t*         get_jni_error_message(int err, int* exit_code, wchar_t* buf, size_t len);
wchar_t*         get_exception_message(JNIEnv* env, jthrowable throwable);
wchar_t*         get_stack_trace(JNIEnv* env, jobject thread, jthrowable throwable);

static wchar_t**        split_args(wchar_t* buffer, int* p_argc);
static jint             register_native(JNIEnv* env, jclass cls, const char* name, const char* signature, void* fnPtr);
static void    JNICALL  JNI_UncaughtException(JNIEnv *env, jobject clazz, jobject thread, jthrowable throwable);
static jstring JNICALL  JNI_SetEnvironment(JNIEnv *env, jobject clazz, jstring key, jstring value);
static void    JNICALL  JNI_WriteEventLog(JNIEnv *env, jobject clazz, jint logType, jstring message);

static jclass    Loader = NULL;
static jobject   resources = NULL;
static jclass    MainClass = NULL;
static jmethodID MainClass_main = NULL;

wchar_t* install_security_manager(JNIEnv* env)
{
		// System.setSecurityManager(new SecurityManager());
		jclass    System                    = NULL;
		jmethodID System_setSecurityManager = NULL;
		jclass    SecurityManager           = NULL;
		jmethodID SecurityManager_init      = NULL;
		jobject   securityManager           = NULL;
		wchar_t*  buf                       = NULL;

		buf = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
		if(buf == NULL)
		{
			goto EXIT;
		}
		buf[0] = L'\0';

		System = (*env)->FindClass(env, "java/lang/System");
		if(System == NULL)
		{
			swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_ERR_FIND_CLASS), L"java.lang.System");
			goto EXIT;
		}
		System_setSecurityManager = (*env)->GetStaticMethodID(env, System, "setSecurityManager", "(Ljava/lang/SecurityManager;)V");
		if(System_setSecurityManager == NULL)
		{
			swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_ERR_GET_METHOD), L"java.lang.System.setSecurityManager(java.lang.SecurityManager)");
			goto EXIT;
		}
		SecurityManager = (*env)->FindClass(env, "java/lang/SecurityManager");
		if(SecurityManager == NULL)
		{
			swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_ERR_FIND_CLASS), L"java.lang.SecurityManager");
			goto EXIT;
		}
		SecurityManager_init = (*env)->GetMethodID(env, SecurityManager, "<init>", "()V");
		if(SecurityManager_init == NULL)
		{
			swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_ERR_GET_METHOD), L"java.lang.SecurityManager()");
			goto EXIT;
		}

		securityManager = (*env)->NewObject(env, SecurityManager, SecurityManager_init);
		if(securityManager == NULL)
		{
			swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_ERR_NEW_OBJECT), L"java.lang.SecurityManager()");
			goto EXIT;
		}
		(*env)->CallStaticVoidMethod(env, System, System_setSecurityManager, securityManager);

		// 例外が発生しても無視します。
		if((*env)->ExceptionCheck(env) == JNI_TRUE)
		{
			(*env)->ExceptionClear(env);
		}

EXIT:
	if(buf != NULL && wcslen(buf) == 0)
	{
		free(buf);
		buf = NULL;
	}
	return buf;
}

BOOL load_main_class(int argc, const wchar_t* argv[], const wchar_t* utilities, LOAD_RESULT* result)
{
	RESOURCE     res;
	jclass       ClassLoader;
	jmethodID    ClassLoader_getSystemClassLoader;
	jmethodID    ClassLoader_definePackage;
	jstring      packageName;
	jobject      systemClassLoader;
	jfieldID     Loader_resources;
	jmethodID    Loader_initialize;
	jclass       JarInputStream;
	jmethodID    JarInputStream_init;
	jclass       ByteBufferInputStream;
	jmethodID    ByteBufferInputStream_init;
	jclass       NativeMethods;
	jclass       URLConnection;
	jclass       URLStreamHandler;
	jclass       URLStreamHandlerFactory;
	jmethodID    URLStreamHandlerFactory_init;
	jobject      urlStreamHandlerFactory;
	jobjectArray jars;
	wchar_t*     main_class;
	UINT         console_code_page;

	// ClassLoader
	ClassLoader = (*env)->FindClass(env, "java/lang/ClassLoader");
	if(ClassLoader == NULL)
	{
		result->msg_id = MSG_ID_ERR_DEFINE_CLASS;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_DEFINE_CLASS), L"java.lang.ClassLoader");
		goto EXIT;
	}
	ClassLoader_getSystemClassLoader = (*env)->GetStaticMethodID(env, ClassLoader, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
	if(ClassLoader_getSystemClassLoader == NULL)
	{
		result->msg_id = MSG_ID_ERR_GET_METHOD;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_GET_METHOD), L"java.lang.ClassLoader.getSystemClassLoader()");
		goto EXIT;
	}
	systemClassLoader = (*env)->CallStaticObjectMethod(env, ClassLoader, ClassLoader_getSystemClassLoader);
	if(systemClassLoader == NULL)
	{
		result->msg_id = MSG_ID_ERR_NULL_OBJECT;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_NULL_OBJECT), L"java.lang.ClassLoader.getSystemClassLoader()");
		goto EXIT;
	}
	ClassLoader_definePackage = (*env)->GetMethodID(env, ClassLoader, "definePackage", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/net/URL;)Ljava/lang/Package;");
	if(ClassLoader_definePackage == NULL)
	{
		result->msg_id = MSG_ID_ERR_GET_METHOD;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_GET_METHOD), L"java.lang.ClassLoader.definePackage(java.lang.String, java.lang.String, java.lang.String, java.lang.String, java.lang.String, java.lang.String, java.lang.String, java.net.URL)");
		goto EXIT;
	}
	
	// Define package "exewrap.core"
	packageName = to_jstring(env, L"exewrap.core");
	(*env)->CallObjectMethod(env, systemClassLoader, ClassLoader_definePackage, packageName, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	
	// Loader
	Loader = (*env)->FindClass(env, "Loader");
	if(Loader == NULL)
	{
		result->msg_id = MSG_ID_ERR_FIND_CLASS;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_FIND_CLASS), L"Loader");
		goto EXIT;
	}
	Loader_resources = (*env)->GetStaticFieldID(env, Loader, "resources", "Ljava/util/Map;");
	if(Loader_resources == NULL)
	{
		result->msg_id = MSG_ID_ERR_GET_FIELD;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_GET_FIELD), L"Loader.resources");
		goto EXIT;
	}
	resources = (*env)->GetStaticObjectField(env, Loader, Loader_resources);
	if(resources == NULL)
	{
		result->msg_id = MSG_ID_ERR_GET_FIELD;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_GET_FIELD), L"Loader.resources");
		goto EXIT;
	}
	Loader_initialize = (*env)->GetStaticMethodID(env, Loader, "initialize", "([Ljava/util/jar/JarInputStream;Ljava/net/URLStreamHandlerFactory;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)Ljava/lang/Class;");
	if(Loader_initialize == NULL)
	{
		result->msg_id = MSG_ID_ERR_GET_METHOD;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_GET_METHOD), L"Loader.initialize(java.util.jar.JarInputStream[], java.net.URLStreamHandlerFactory, java.lang.String, java.lang.String, java.lang.String, int)");
		goto EXIT;
	}
	
	// JarInputStream
	JarInputStream = (*env)->FindClass(env, "java/util/jar/JarInputStream");
	if(JarInputStream == NULL)
	{
		result->msg_id = MSG_ID_ERR_FIND_CLASS;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_FIND_CLASS), L"java.util.jar.JarInputStream");
		goto EXIT;
	}
	JarInputStream_init = (*env)->GetMethodID(env, JarInputStream, "<init>", "(Ljava/io/InputStream;)V");
	if(JarInputStream_init == NULL)
	{
		result->msg_id = MSG_ID_ERR_GET_CONSTRUCTOR;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_GET_CONSTRUCTOR), L"java.util.jar.JarInputStream(java.io.InputStream)");
		goto EXIT;
	}

	// ByteBufferInputStream
	if(get_resource(L"BYTE_BUFFER_INPUT_STREAM", &res) == NULL)
	{
		result->msg_id = MSG_ID_ERR_RESOURCE_NOT_FOUND;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_RESOURCE_NOT_FOUND), L"RT_RCDATA: BYTE_BUFFER_INPUT_STREAM");
		goto EXIT;
	}
	ByteBufferInputStream = (*env)->DefineClass(env, "exewrap/core/ByteBufferInputStream", systemClassLoader, (jbyte*)res.buf, res.len);
	if(ByteBufferInputStream == NULL)
	{
		result->msg_id = MSG_ID_ERR_FIND_CLASS;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_FIND_CLASS), L"exewrap.core.ByteBufferInputStream");
		goto EXIT;
	}
	ByteBufferInputStream_init = (*env)->GetMethodID(env, ByteBufferInputStream, "<init>", "(Ljava/nio/ByteBuffer;)V");
	if(ByteBufferInputStream_init == NULL)
	{
		result->msg_id = MSG_ID_ERR_GET_CONSTRUCTOR;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_GET_CONSTRUCTOR), L"exewrap.core.ByteBufferInputStream(java.nio.ByteBuffer)");
		goto EXIT;
	}

	// NativeMethods
	if(get_resource(L"NATIVE_METHODS", &res) == NULL)
	{
		result->msg_id = MSG_ID_ERR_RESOURCE_NOT_FOUND;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_RESOURCE_NOT_FOUND), L"RT_RCDATA: NATIVE_METHODS");
		goto EXIT;
	}
	NativeMethods = (*env)->DefineClass(env, "exewrap/core/NativeMethods", systemClassLoader, (jbyte*)res.buf, res.len);
	if(NativeMethods == NULL)
	{
		result->msg_id = MSG_ID_ERR_FIND_CLASS;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_FIND_CLASS), L"exewrap.core.NativeMethods");
		goto EXIT;
	}

	#pragma warning(push)
	#pragma warning(disable:4152) // 関数ポインターからデータポインターへの変換警告を抑制します。
	if(register_native(env, NativeMethods, "WriteEventLog", "(ILjava/lang/String;)V", JNI_WriteEventLog) != 0)
	{
		result->msg_id = MSG_ID_ERR_REGISTER_NATIVE;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_REGISTER_NATIVE), L"WriteEventLog");
		goto EXIT;
	}
	if(register_native(env, NativeMethods, "UncaughtException", "(Ljava/lang/Thread;Ljava/lang/Throwable;)V", JNI_UncaughtException) != 0)
	{
		result->msg_id = MSG_ID_ERR_REGISTER_NATIVE;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_REGISTER_NATIVE), L"UncaughtException");
		goto EXIT;
	}
	if(register_native(env, NativeMethods, "SetEnvironment", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", JNI_SetEnvironment) != 0)
	{
		result->msg_id = MSG_ID_ERR_REGISTER_NATIVE;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_REGISTER_NATIVE), L"SetEnvironment");
		goto EXIT;
	}
	#pragma warning(pop)
	
	// URLConnection
	if(get_resource(L"URL_CONNECTION", &res) == NULL)
	{
		result->msg_id = MSG_ID_ERR_RESOURCE_NOT_FOUND;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_RESOURCE_NOT_FOUND), L"RT_RCDATA: URL_CONNECTION");
		goto EXIT;
	}
	URLConnection = (*env)->DefineClass(env, "exewrap/core/URLConnection", systemClassLoader, (jbyte*)res.buf, res.len);
	if(URLConnection == NULL)
	{
		result->msg_id = MSG_ID_ERR_DEFINE_CLASS;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_DEFINE_CLASS), L"exewrap.core.URLConnection");
		goto EXIT;
	}
	// URLStreamHandler
	if(get_resource(L"URL_STREAM_HANDLER", &res) == NULL)
	{
		result->msg_id = MSG_ID_ERR_RESOURCE_NOT_FOUND;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_RESOURCE_NOT_FOUND), L"RT_RCDATA: URL_STREAM_HANDLER");
		goto EXIT;
	}
	URLStreamHandler = (*env)->DefineClass(env, "exewrap/core/URLStreamHandler", systemClassLoader, (jbyte*)res.buf, res.len);
	if(URLStreamHandler == NULL)
	{
		result->msg_id = MSG_ID_ERR_DEFINE_CLASS;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_DEFINE_CLASS), L"exewrap.core.URLStreamHandler");
		goto EXIT;
	}
	// URLStreamHandlerFactory
	if(get_resource(L"URL_STREAM_HANDLER_FACTORY", &res) == NULL)
	{
		result->msg_id = MSG_ID_ERR_RESOURCE_NOT_FOUND;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_RESOURCE_NOT_FOUND), L"RT_RCDATA: URL_STREAM_HANDLER_FACTORY");
		goto EXIT;
	}
	URLStreamHandlerFactory = (*env)->DefineClass(env, "exewrap/core/URLStreamHandlerFactory", systemClassLoader, (jbyte*)res.buf, res.len);
	if(URLStreamHandlerFactory == NULL)
	{
		result->msg_id = MSG_ID_ERR_DEFINE_CLASS;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_DEFINE_CLASS), L"exewrap.core.URLStreamHandlerFactory");
		goto EXIT;
	}
	URLStreamHandlerFactory_init = (*env)->GetMethodID(env, URLStreamHandlerFactory, "<init>", "(Ljava/lang/ClassLoader;Ljava/util/Map;)V");
	if(URLStreamHandlerFactory_init == NULL)
	{
		result->msg_id = MSG_ID_ERR_GET_CONSTRUCTOR;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_GET_CONSTRUCTOR), L"exewrap.core.URLStreamHandlerFactory(java.lang.ClassLoader)");
		goto EXIT;
	}
	urlStreamHandlerFactory = (*env)->NewObject(env, URLStreamHandlerFactory, URLStreamHandlerFactory_init, systemClassLoader, resources);
	if(urlStreamHandlerFactory == NULL)
	{
		result->msg_id = MSG_ID_ERR_NEW_OBJECT;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_NEW_OBJECT), L"exewrap.core.URLStreamHandlerFactory(java.lang.ClassLoader)");
		goto EXIT;
	}

	// JarInputStream[] jars = new JarInputStream[2];
	jars = (*env)->NewObjectArray(env, 2, JarInputStream, NULL);
	if(jars == NULL)
	{
		result->msg_id = MSG_ID_ERR_NEW_OBJECT;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_NEW_OBJECT), L"JarInputStream[]");
		goto EXIT;
	}
	
	// util.jar
	{
		jobject byteBuffer;
		jobject byteBufferInputStream = NULL;
		jobject jarInputStream = NULL;
		
		if(get_resource(L"UTIL_JAR", &res) == NULL)
		{
			result->msg_id = MSG_ID_ERR_RESOURCE_NOT_FOUND;
			swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_RESOURCE_NOT_FOUND), L"RT_RCDATA: UTIL_JAR");
			goto EXIT;
		}
		byteBuffer = (*env)->NewDirectByteBuffer(env, res.buf, res.len);
		if(byteBuffer == NULL)
		{
			result->msg_id = MSG_ID_ERR_NEW_OBJECT;
			swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_NEW_OBJECT), L"NewDirectByteBuffer(JNIEnv* env, void* address, jlong capacity)");
			goto EXIT;
		}
		byteBufferInputStream = (*env)->NewObject(env, ByteBufferInputStream, ByteBufferInputStream_init, byteBuffer);
		if(byteBufferInputStream == NULL)
		{
			result->msg_id = MSG_ID_ERR_NEW_OBJECT;
			swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_NEW_OBJECT), L"exewrap.core.ByteBufferInputStream(java.nio.ByteBuffer)");
			goto EXIT;
		}
		jarInputStream = (*env)->NewObject(env, JarInputStream, JarInputStream_init, byteBufferInputStream);
		if(jarInputStream == NULL)
		{
			result->msg_id = MSG_ID_ERR_NEW_OBJECT;
			swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_NEW_OBJECT), L"java.util.jar.JarInputStream(exewrap.core.ByteBufferInputStream)");
			goto EXIT;
		}
		(*env)->SetObjectArrayElement(env, jars, 0, jarInputStream);
	}

	// user.jar
	{
		jobject  byteBuffer;
		jobject  byteBufferInputStream;
		jobject  jarInputStream;

    	if(get_resource(L"JAR", &res) == NULL)
		{
			result->msg_id = MSG_ID_ERR_RESOURCE_NOT_FOUND;
			swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_RESOURCE_NOT_FOUND), L"RT_RCDATA: JAR");
			goto EXIT;
		}
		byteBuffer = (*env)->NewDirectByteBuffer(env, res.buf, res.len);
		if(byteBuffer == NULL)
		{
			result->msg_id = MSG_ID_ERR_NEW_OBJECT;
			swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_NEW_OBJECT), L"NewDirectByteBuffer(JNIEnv* env, void* address, jlong capacity)");
			goto EXIT;
		}
		byteBufferInputStream = (*env)->NewObject(env, ByteBufferInputStream, ByteBufferInputStream_init, byteBuffer);
		if(byteBufferInputStream == NULL)
		{
			result->msg_id = MSG_ID_ERR_NEW_OBJECT;
			swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_NEW_OBJECT), L"exewrap.core.ByteBufferInputStream(java.nio.ByteBuffer)");
			goto EXIT;
		}
		// JarInputStream
		jarInputStream = (*env)->NewObject(env, JarInputStream, JarInputStream_init, byteBufferInputStream);
		if (jarInputStream == NULL)
		{
			result->msg_id = MSG_ID_ERR_NEW_OBJECT;
			swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_NEW_OBJECT), L"java.util.jar.JarInputStream(ByteBufferInputStream)");
			goto EXIT;
		}
		(*env)->SetObjectArrayElement(env, jars, 1, jarInputStream);
	}

	// call Loader.initialize
	main_class = from_utf8((char*)get_resource(L"MAIN_CLASS", NULL)); // MAIN_CLASSは定義されていない場合は main_class = NULL のまま処理を進めます。
	console_code_page = GetConsoleOutputCP();
	MainClass = (*env)->CallStaticObjectMethod(env, Loader, Loader_initialize, jars, urlStreamHandlerFactory, to_jstring(env, utilities), to_jstring(env, get_classpath()), to_jstring(env, main_class), console_code_page);
	if(MainClass == NULL)
	{
		result->msg_id = MSG_ID_ERR_LOAD_MAIN_CLASS;
		swprintf_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_LOAD_MAIN_CLASS), main_class);
		goto EXIT;
	}

	if(utilities == NULL || wcsstr(utilities, UTIL_NO_MAIN) == NULL) // NO-MAIN; が指定されている場合はmainメソッドを探しません。NO-MAIN; はサービスアプリケーションで指定されます。
	{
		MainClass_main = (*env)->GetStaticMethodID(env, MainClass, "main", "([Ljava/lang/String;)V");
		if(MainClass_main == NULL)
		{
			result->msg_id = MSG_ID_ERR_FIND_MAIN_METHOD;
			wcscpy_s(result->msg, LOAD_RESULT_MAX_MESSAGE_LENGTH, _(MSG_ID_ERR_FIND_MAIN_METHOD));
			goto EXIT;
		}
		result->MainClass_main_args = (*env)->NewObjectArray(env, argc - 1, (*env)->FindClass(env, "java/lang/String"), NULL);
		{
			int i;
			for (i = 1; i < argc; i++)
			{
				(*env)->SetObjectArrayElement(env, result->MainClass_main_args, (i - 1), to_jstring(env, argv[i]));
			}
		}
	}
	result->MainClass = MainClass;
	result->MainClass_main = MainClass_main;
	result->msg_id = 0;
	*(result->msg) = L'\0';
	return TRUE;

EXIT:

	return FALSE;
}


BOOL set_splash_screen_resource(const wchar_t* splash_screen_name, const BYTE* splash_screen_image_buf, DWORD splash_screen_image_len)
{
	BOOL       ret   = FALSE;
	jstring    name  = NULL;
	jbyteArray image = NULL;
	jclass     Map;
	jmethodID  Map_put;

	name = to_jstring(env, splash_screen_name);
	if(name == NULL)
	{
		goto EXIT;
	}

	image = (*env)->NewByteArray(env, splash_screen_image_len);
	if(image == NULL)
	{
		goto EXIT;
	}
	(*env)->SetByteArrayRegion(env, image, 0, splash_screen_image_len, (jbyte*)splash_screen_image_buf);

	Map = (*env)->FindClass(env, "java/util/Map");
	if(Map == NULL)
	{
		goto EXIT;
	}
	Map_put = (*env)->GetMethodID(env, Map, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
	if(Map_put == NULL)
	{
		goto EXIT;
	}

	(*env)->CallObjectMethod(env, resources, Map_put, name, image);

	ret = TRUE;

EXIT:
	if(image != NULL)
	{
		(*env)->DeleteLocalRef(env, image);
	}
	if(name != NULL)
	{
		(*env)->DeleteLocalRef(env, name);
	}
	if((*env)->ExceptionCheck(env) == JNI_TRUE)
	{
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
		ret = FALSE;
	}
	return ret;
}


UINT WINAPI remote_call_main_method(void* shared_memory_handle)
{
	LPBYTE       lpShared;
	wchar_t*     arglist;
	size_t       buf_len;
	wchar_t*     buf;
	int          argc;
	wchar_t**    argv = NULL;
	int          i;
	wchar_t*     shared_memory_read_event_name;
	HANDLE       shared_memory_read_event_handle;
	JNIEnv*      env  = NULL;
	jobjectArray args = NULL;

	lpShared = (LPBYTE)MapViewOfFile((HANDLE)shared_memory_handle, FILE_MAP_READ, 0, 0, 0);

	arglist = (wchar_t*)(lpShared + sizeof(DWORD) + sizeof(DWORD));
	buf_len = wcslen(arglist);
	buf = (wchar_t*)malloc((buf_len + 1) * sizeof(wchar_t));
	if(buf == NULL)
	{
		goto EXIT;
	}
	wcscpy_s(buf, buf_len + 1, arglist);
	UnmapViewOfFile(lpShared);

	argv = split_args(buf, &argc);
	free(buf);
	buf = NULL;

	env = attach_java_vm();
	if(env != NULL)
	{
		args = (*env)->NewObjectArray(env, argc - 1, (*env)->FindClass(env, "java/lang/String"), NULL);
		for (i = 1; i < argc; i++)
		{
			(*env)->SetObjectArrayElement(env, args, (i - 1), to_jstring(env, argv[i]));
		}
	}

	shared_memory_read_event_name = get_module_object_name(L"SHARED_MEMORY_READ");
	shared_memory_read_event_handle = OpenEvent(EVENT_MODIFY_STATE, FALSE, shared_memory_read_event_name);
	if(shared_memory_read_event_handle != NULL)
	{
		SetEvent(shared_memory_read_event_handle);
		CloseHandle(shared_memory_read_event_handle);
	}
	free(shared_memory_read_event_name);

	if(env == NULL)
	{
		goto EXIT;
	}

	(*env)->CallStaticVoidMethod(env, MainClass, MainClass_main, args);
	if((*env)->ExceptionCheck(env) == JNI_TRUE)
	{
		jthrowable throwable = (*env)->ExceptionOccurred(env);
		if(throwable != NULL)
		{
			jstring thread = to_jstring(env, L"main");
			uncaught_exception(env, thread, throwable);
			(*env)->DeleteLocalRef(env, thread);
			(*env)->DeleteLocalRef(env, throwable);
		}
		(*env)->ExceptionClear(env);
	}

EXIT:
	if(env != NULL)
	{
		if(args != NULL)
		{
			(*env)->DeleteLocalRef(env, args);
		}
		detach_java_vm();
	}
	if(argv != NULL)
	{
		free(argv);
	}

	return 0;
}


wchar_t* get_module_object_name(const wchar_t* prefix)
{
	wchar_t* module_filename;
	wchar_t* object_name;

	module_filename = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(module_filename == NULL)
	{
		return NULL;
	}
	object_name = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(object_name == NULL)
	{
		free(module_filename);
		return NULL;
	}

	GetModuleFileName(NULL, module_filename, MAX_PATH);
	wcscpy_s(object_name, MAX_LONG_PATH, L"EXEWRAP:");
	if(prefix != NULL)
	{
		wcscat_s(object_name, MAX_LONG_PATH, prefix);
	}
	wcscat_s(object_name, MAX_LONG_PATH, L":");
	wcscat_s(object_name, MAX_LONG_PATH, (wchar_t*)(wcsrchr(module_filename, L'\\') + 1));

	free(module_filename);
	return object_name;
}

BYTE* get_resource(const wchar_t* name, RESOURCE* resource)
{
	HRSRC hrsrc;
	BYTE* buf = NULL;
	DWORD len = 0;
	DWORD last_error = 0;

	if((hrsrc = FindResource(NULL, name, RT_RCDATA)) != NULL)
	{
		buf = (BYTE*)LockResource(LoadResource(NULL, hrsrc));
		if(buf == NULL)
		{
			last_error = GetLastError();
		}
		len = SizeofResource(NULL, hrsrc);
	}
	if(resource != NULL)
	{
		resource->buf = buf;
		resource->len = len;
	}
	if(last_error != 0)
	{
		SetLastError(last_error);
	}
	return buf;
}

wchar_t* get_jni_error_message(int error, int* exit_code, wchar_t* buf, size_t len)
{
	int dummy;

	if(exit_code == NULL)
	{
		exit_code = &dummy;
	}
	if(buf == NULL || len == 0)
	{
		len = BUFFER_SIZE;
		buf = (wchar_t*)malloc(len * sizeof(wchar_t));
		if(buf == NULL)
		{
			*exit_code = MSG_ID_ERR_CREATE_JVM_UNKNOWN;
			return NULL;
		}
	}

	switch(error) {
	case JNI_OK:
		*exit_code = 0;
		buf[0] = L'\0';
		return NULL;
	case JNI_ERR: /* unknown error */
		*exit_code = MSG_ID_ERR_CREATE_JVM_UNKNOWN;
		wcscpy_s(buf, len, _(MSG_ID_ERR_CREATE_JVM_UNKNOWN));
		break;
	case JNI_EDETACHED: /* thread detached from the VM */
		*exit_code = MSG_ID_ERR_CREATE_JVM_EDETACHED;
		wcscpy_s(buf, len, _(MSG_ID_ERR_CREATE_JVM_EDETACHED));
		break;
	case JNI_EVERSION: /* JNI version error */
		*exit_code = MSG_ID_ERR_CREATE_JVM_EVERSION;
		wcscpy_s(buf, len, _(MSG_ID_ERR_CREATE_JVM_EVERSION));
		break;
	case JNI_ENOMEM: /* not enough memory */
		*exit_code = MSG_ID_ERR_CREATE_JVM_ENOMEM;
		wcscpy_s(buf, len, _(MSG_ID_ERR_CREATE_JVM_ENOMEM));
		break;
	case JNI_EEXIST: /* VM already created */
		*exit_code = MSG_ID_ERR_CREATE_JVM_EEXIST;
		wcscpy_s(buf, len, _(MSG_ID_ERR_CREATE_JVM_EEXIST));
		break;
	case JNI_EINVAL: /* invalid arguments */
		*exit_code = MSG_ID_ERR_CREATE_JVM_EINVAL;
		wcscpy_s(buf, len, _(MSG_ID_ERR_CREATE_JVM_EINVAL));
		break;
	case JVM_ELOADLIB:
		*exit_code = MSG_ID_ERR_CREATE_JVM_ELOADLIB;
		swprintf_s(buf, len, _(MSG_ID_ERR_CREATE_JVM_ELOADLIB), get_process_architecture());
		break;
	default:
		*exit_code = MSG_ID_ERR_CREATE_JVM_UNKNOWN;
		wcscpy_s(buf, len, _(MSG_ID_ERR_CREATE_JVM_UNKNOWN));
		break;
	}
	return buf;
}


wchar_t* get_exception_message(JNIEnv* env, jthrowable throwable)
{
	jclass    Throwable            = NULL;
	jmethodID throwable_getMessage = NULL;
	jstring   message              = NULL;
	wchar_t*  buf                  = NULL;
	wchar_t*  str_message          = NULL;

	if(throwable == NULL)
	{
		goto EXIT;
	}

	buf = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(buf == NULL)
	{
		goto EXIT;
	}

	Throwable = (*env)->FindClass(env, "java/lang/Throwable");
	if(Throwable == NULL)
	{
		swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_ERR_FIND_CLASS), L"java.lang.Throwable");
		goto EXIT;
	}
	throwable_getMessage = (*env)->GetMethodID(env, Throwable, "getMessage", "()Ljava/lang/String;");
	if(throwable_getMessage == NULL)
	{
		swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_ERR_GET_METHOD), L"java.lang.Throwable.getMessage()");
		goto EXIT;
	}

	message = (jstring)(*env)->CallObjectMethod(env, throwable, throwable_getMessage);
	str_message = from_jstring(env, message);
	
EXIT:
	if(message != NULL)
	{
		(*env)->DeleteLocalRef(env, message);
	}
	if(buf != NULL)
	{
		free(buf);
	}
	return str_message;
}


wchar_t* get_stack_trace(JNIEnv* env, jobject thread, jthrowable throwable)
{
	jclass    Thread                    = NULL;
	jmethodID Thread_currentThread      = NULL;
	jmethodID thread_getName            = NULL;
	jstring   thread_name               = NULL;
	jclass    StringWriter              = NULL;
	jmethodID StringWriter_init         = NULL;
	jmethodID stringWriter_toString     = NULL;
	jobject   stringWriter              = NULL;
	jclass    PrintWriter               = NULL;
	jmethodID PrintWriter_init          = NULL;
	jmethodID printWriter_flush         = NULL;
	jobject   printWriter               = NULL;
	jclass    Throwable                 = NULL;
	jmethodID throwable_printStackTrace = NULL;
	jstring   stack_trace               = NULL;
	wchar_t*  buf                       = NULL;
	wchar_t*  str_thread_name           = NULL;
	wchar_t*  str_stack_trace           = NULL;
	size_t    len;

	buf = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(buf == NULL)
	{
		goto EXIT;
	}
		
	Thread = (*env)->FindClass(env, "java/lang/Thread");
	if(Thread == NULL)
	{
		swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_ERR_FIND_CLASS), "java.lang.Thread");
		goto EXIT;
	}
	Thread_currentThread = (*env)->GetStaticMethodID(env, Thread, "currentThread", "()Ljava/lang/Thread;");
	if(Thread_currentThread == NULL)
	{
		swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_ERR_GET_METHOD), "java.lang.Thread.currentThread()");
		goto EXIT;
	}
	thread_getName = (*env)->GetMethodID(env, Thread, "getName", "()Ljava/lang/String;");
	if(thread_getName == NULL)
	{
		swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_ERR_GET_METHOD), "java.lang.Thread.getName()");
		goto EXIT;
	}

	if(thread != NULL)
	{
		thread_name = (*env)->CallObjectMethod(env, thread, thread_getName);
	}
	else
	{
		thread = (*env)->CallStaticObjectMethod(env, Thread, Thread_currentThread);
		if(thread != NULL)
		{
			thread_name = (*env)->CallObjectMethod(env, thread, thread_getName);
			(*env)->DeleteLocalRef(env, thread);
			thread = NULL;
		}
	}

	if(throwable != NULL)
	{
		StringWriter = (*env)->FindClass(env, "java/io/StringWriter");
		if(StringWriter == NULL)
		{
			swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_ERR_FIND_CLASS), L"java.io.StringWriter");
			goto EXIT;
		}
		StringWriter_init = (*env)->GetMethodID(env, StringWriter, "<init>", "()V");
		if(StringWriter_init == NULL)
		{
			swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_ERR_GET_CONSTRUCTOR), L"java.io.StringWriter()");
			goto EXIT;
		}
		stringWriter_toString = (*env)->GetMethodID(env, StringWriter, "toString", "()Ljava/lang/String;");
		if(stringWriter_toString == NULL)
		{
			swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_ERR_GET_METHOD), L"java.io.StringWriter.toString()");
			goto EXIT;
		}
		PrintWriter = (*env)->FindClass(env, "java/io/PrintWriter");
		if(PrintWriter == NULL)
		{
			swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_ERR_FIND_CLASS), L"java.io.PrintWriter");
			goto EXIT;
		}
		PrintWriter_init = (*env)->GetMethodID(env, PrintWriter, "<init>", "(Ljava/io/Writer;)V");
		if(PrintWriter_init == NULL)
		{
			swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_ERR_GET_CONSTRUCTOR), L"java.io.PrintWriter(java.io.Writer)");
			goto EXIT;
		}
		printWriter_flush = (*env)->GetMethodID(env, PrintWriter, "flush", "()V");
		if(printWriter_flush == NULL)
		{
			swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_ERR_GET_METHOD), L"java.io.PrintWriter.flush()");
			goto EXIT;
		}
		Throwable = (*env)->FindClass(env, "java/lang/Throwable");
		if(Throwable == NULL)
		{
			swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_ERR_FIND_CLASS), L"java.lang.Throwable");
			goto EXIT;
		}
		throwable_printStackTrace = (*env)->GetMethodID(env, Throwable, "printStackTrace", "(Ljava/io/PrintWriter;)V");
		if(throwable_printStackTrace == NULL)
		{
			swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_ERR_GET_METHOD), L"java.lang.Throwable.printStackTrace(java.io.PrintWriter)");
			goto EXIT;
		}

		stringWriter = (*env)->NewObject(env, StringWriter, StringWriter_init);
		if(stringWriter == NULL)
		{
			swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_ERR_NEW_OBJECT), L"java.io.StringWriter()");
			goto EXIT;
		}
		printWriter = (*env)->NewObject(env, PrintWriter, PrintWriter_init, stringWriter);
		if(printWriter == NULL)
		{
			swprintf_s(buf, BUFFER_SIZE, _(MSG_ID_ERR_NEW_OBJECT), L"java.io.PrintWriter(java.io.Writer)");
			goto EXIT;
		}
		(*env)->CallVoidMethod(env, throwable, throwable_printStackTrace, printWriter);
		(*env)->CallVoidMethod(env, printWriter, printWriter_flush);
		stack_trace = (jstring)(*env)->CallObjectMethod(env, stringWriter, stringWriter_toString);
	}

	len = 32;
	if(thread_name != NULL)
	{
		str_thread_name = from_jstring(env, thread_name);
		if(str_thread_name != NULL)
		{
			len += wcslen(str_thread_name);
		}
	}
	if(stack_trace != NULL)
	{
		str_stack_trace = from_jstring(env, stack_trace);
		if(str_stack_trace != NULL)
		{
			len += wcslen(str_stack_trace);
		}
	}

	free(buf);
	buf = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
	if(buf == NULL)
	{
		goto EXIT;
	}
	buf[0] = L'\0';

	if(str_thread_name != NULL)
	{
		wcscat_s(buf, len + 1, L"Exception in thread \"");
		wcscat_s(buf, len + 1, str_thread_name);
		wcscat_s(buf, len + 1, L"\" ");
	}
	if(str_stack_trace != NULL)
	{
		wcscat_s(buf, len + 1, str_stack_trace);
	}
	
EXIT:
	if(printWriter != NULL)
	{
		(*env)->DeleteLocalRef(env, printWriter);
	}
	if(stringWriter != NULL)
	{
		(*env)->DeleteLocalRef(env, stringWriter);
	}
	if(thread_name != NULL)
	{
		(*env)->DeleteLocalRef(env, thread_name);
	}
	if(str_stack_trace != NULL)
	{
		free(str_stack_trace);
	}
	if(str_thread_name != NULL)
	{
		free(str_thread_name);
	}
	return buf;
}


static wchar_t** split_args(wchar_t* buffer, int* p_argc)
{
	int        i;
	int        len;
	wchar_t**  argv;
	wchar_t*   context;

	len = (int)wcslen(buffer);

	*p_argc = 0;
	for(i = 0; i < len; i++) {
		*p_argc += ((buffer[i] == L'\n') ? 1 : 0);
	}
	argv = (wchar_t**)calloc(*p_argc, sizeof(wchar_t*));
	for(i = 0; i < *p_argc; i++)
	{
		argv[i] = wcstok_s(((i == 0) ? buffer : NULL), L"\n", &context);
	}
	return argv;
}


static jint register_native(JNIEnv* env, jclass cls, const char* name, const char* signature, void* fnPtr)
{
	JNINativeMethod nm;

	nm.name = (char*)name;
	nm.signature = (char*)signature;
	nm.fnPtr = fnPtr;

	return (*env)->RegisterNatives(env, cls, &nm, 1);
}


void JNICALL JNI_UncaughtException(JNIEnv *env, jobject clazz, jobject thread, jthrowable throwable)
{
	DWORD exit_code;

	UNREFERENCED_PARAMETER(clazz);

	exit_code = uncaught_exception(env, thread, throwable);
	ExitProcess(exit_code);
}


jstring JNICALL JNI_SetEnvironment(JNIEnv *env, jobject clazz, jstring key, jstring value)
{
	wchar_t* _key       = NULL;
	wchar_t* _value     = NULL;
	DWORD    len;
	wchar_t* buf        = NULL;
	jstring  prev_value = NULL;
	BOOL     result;

	UNREFERENCED_PARAMETER(clazz);

	_key = from_jstring(env, key);
	_value = from_jstring(env, value);

	len = GetEnvironmentVariable(_key, NULL, 0);
	if(len > 0)
	{
		buf = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
		if(buf == NULL)
		{
			goto EXIT;
		}
		len = GetEnvironmentVariable(_key, buf, len + 1);
		if(len > 0) {
			prev_value = to_jstring(env, buf);
		}
	}

	result = SetEnvironmentVariable(_key, _value);
	if(result == 0)
	{
		if(prev_value != NULL)
		{
			(*env)->DeleteLocalRef(env, prev_value);
			prev_value = NULL;
		}
	}

EXIT:
	if(buf != NULL)
	{
		free(buf);
	}
	if(_value != NULL)
	{
		free(_value);
	}
	if(_key != NULL)
	{
		free(_key);
	}
	return prev_value;
}


void JNICALL JNI_WriteEventLog(JNIEnv* env, jobject clazz, jint logType, jstring message)
{
	WORD     type;
	wchar_t* str;

	UNREFERENCED_PARAMETER(clazz);
	
	switch (logType)
	{
	case 0:
		type = EVENTLOG_INFORMATION_TYPE;
		break;
	case 1:
		type = EVENTLOG_WARNING_TYPE;
		break;
	case 2:
		type = EVENTLOG_ERROR_TYPE;
		break;
	default:
		type = EVENTLOG_INFORMATION_TYPE;
	}
	str = from_jstring(env, message);
	write_event_log(type, str);
	free(str);
}
