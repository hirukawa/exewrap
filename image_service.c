#include <windows.h>
#include <process.h>
#include <shlobj.h>
#include <stdio.h>
#include <jni.h>

#include "include/jvm.h"
#include "include/eventlog.h"
#include "include/service.h"
#include "include/startup.h"
#include "include/message.h"

BOOL WINAPI HandlerRoutine(DWORD dwCtrlType);
void    InitResource(LPCTSTR name);
DWORD   GetResourceSize(LPCTSTR name);
jbyte*  GetResourceBuffer(LPCTSTR name);

void JNICALL JNI_WriteEventLog(JNIEnv *env, jobject clazz, jint logType, jstring message);
void JNICALL JNI_UncaughtException(JNIEnv *env, jobject clazz, jstring message, jstring trace);

TCHAR  cache[] = "12345678901234567890123456789012";
jbyte* buffer = NULL;
DWORD  size   = 0;

static char      service_name[1024];
static jclass    mainClass;
static jmethodID stopMethod;

int isService = FALSE;
int isRunning = FALSE;

int _main(int argc, char* argv[]) {
	if((argc >= 2) && (_stricmp(argv[1], "-service") == 0)) {
		isService = TRUE;
	}
	service_main(argc, argv);
}

int service_install(int argc, char* argv[]) {
	if(GetResourceSize("SVCDESC") > 0) {
		SetServiceDescription((LPTSTR)GetResourceBuffer("SVCDESC"));
	}
	InstallEventLog();
}

int service_remove() {
	RemoveEventLog();
}

int service_start(int argc, char* argv[]) {
	int exit_code = 0;
	int i;
	BOOL useServerVM = TRUE;
	int err;
	LPTSTR ext_flags = NULL;
	LPTSTR vm_args_opt = NULL;
	LPTSTR relative_classpath = NULL;
	LPTSTR relative_extdirs = "lib";
	char message[1024];

	if(GetResourceSize("RELATIVE_CLASSPATH") > 0) {
		relative_classpath = (LPTSTR)GetResourceBuffer("RELATIVE_CLASSPATH");
	}
	if(GetResourceSize("EXTDIRS") > 0) {
		relative_extdirs = (LPTSTR)GetResourceBuffer("EXTDIRS");
	}
	if(GetResourceSize("EXTFLAGS") > 0) {
		ext_flags = _strupr((LPTSTR)GetResourceBuffer("EXTFLAGS"));
	}
	if(ext_flags != NULL && strstr(ext_flags, "CLIENT") != NULL) {
		useServerVM = FALSE;
	}

	InitializePath(relative_classpath, relative_extdirs, useServerVM);

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
		exit_code = MSG_ID_ERR_TARGET_VERSION;
		goto EXIT;
	}

	// URLConnection
	jclass urlConnectionClass = (*env)->DefineClass(env, "URLConnection", NULL, GetResourceBuffer("URL_CONNECTION"), GetResourceSize("URL_CONNECTION"));
	if(urlConnectionClass == NULL) {
		fprintf(stderr, _(MSG_ID_ERR_DEFINE_CLASS), "URLConnection");
		exit_code = MSG_ID_ERR_DEFINE_CLASS;
		goto EXIT;
	}
	// URLStreamHandler
	jclass urlStreamHandlerClass = (*env)->DefineClass(env, "URLStreamHandler", NULL, GetResourceBuffer("URL_STREAM_HANDLER"), GetResourceSize("URL_STREAM_HANDLER"));
	if(urlStreamHandlerClass == NULL) {
		fprintf(stderr, _(MSG_ID_ERR_DEFINE_CLASS), "URLStreamHandler");
		exit_code = MSG_ID_ERR_DEFINE_CLASS;
		goto EXIT;
	}
	//EventLogStream Windowsサービスとして起動している場合、出力をイベントログに切り替えます。
	jclass eventLogStreamClass = NULL;
	if(isService) {
		jmethodID eventLogStreamInit;
		if((eventLogStreamClass = (*env)->DefineClass(env, "EventLogStream", NULL, GetResourceBuffer("EVENTLOG_STREAM"), GetResourceSize("EVENTLOG_STREAM"))) != NULL) {
			if(ext_flags == NULL || strstr(ext_flags, "NOLOG") == NULL) {
				if((eventLogStreamInit = (*env)->GetStaticMethodID(env, eventLogStreamClass, "initialize", "()V")) != NULL) {
					(*env)->CallStaticVoidMethod(env, eventLogStreamClass, eventLogStreamInit);
					JNINativeMethod	nm;
					nm.name = (char*)"WriteEventLog";
					nm.signature = (char*)"(ILjava/lang/String;)V";
					nm.fnPtr = (void*)JNI_WriteEventLog;
					if(((*env)->RegisterNatives(env, eventLogStreamClass, &nm, 1)) != 0) {
						fprintf(stderr, _(MSG_ID_ERR_REGISTER_NATIVE), "WriteEventLog");
						exit_code = MSG_ID_ERR_REGISTER_NATIVE;
						goto EXIT;
					}
				}
			}
		}
	}
	//EventLogHandler (version 1.4.0 or higher only) Windowsサービスとして起動している場合、java.util.Logger.getLogger("eventlog") でイベントログに出力できるようにします。
	if(version >= 0x01040000) {
		jclass eventLogHandlerClass = NULL;
		if(isService) {
			jmethodID eventLogHandlerInit;
			if((eventLogHandlerClass = (*env)->DefineClass(env, "EventLogHandler", NULL, GetResourceBuffer("EVENTLOG_HANDLER"), GetResourceSize("EVENTLOG_HANDLER"))) != NULL) {
				if((eventLogHandlerInit = (*env)->GetStaticMethodID(env, eventLogHandlerClass, "initialize", "()V")) != NULL) {
					(*env)->CallStaticVoidMethod(env, eventLogHandlerClass, eventLogHandlerInit);
					JNINativeMethod nm;
					nm.name = (char*)"WriteEventLog";
					nm.signature = (char*)"(ILjava/lang/String;)V";
					nm.fnPtr = (char*)JNI_WriteEventLog;
					if(((*env)->RegisterNatives(env, eventLogHandlerClass, &nm, 1)) != 0) {
						fprintf(stderr, _(MSG_ID_ERR_REGISTER_NATIVE), "WriteEventLog");
						exit_code = MSG_ID_ERR_REGISTER_NATIVE;
						goto EXIT;
					}
				}
			}
		}
	}

	// UncaughtHandler (version 1.5.0 or higher only)
	if(version >= 0x01050000) {
		jclass uncaughtHandlerClass;
		jmethodID uncaughtHandlerInit;
		if((uncaughtHandlerClass = (*env)->DefineClass(env, "UncaughtHandler", NULL, GetResourceBuffer("UNCAUGHT_HANDLER"), GetResourceSize("UNCAUGHT_HANDLER"))) != NULL) {
			if((uncaughtHandlerInit = (*env)->GetStaticMethodID(env, uncaughtHandlerClass, "initialize", "()V")) != NULL) {
				(*env)->CallStaticVoidMethod(env, uncaughtHandlerClass, uncaughtHandlerInit);
				JNINativeMethod	nm;
				nm.name = (char*)"UncaughtException";
				nm.signature = (char*)"(Ljava/lang/String;Ljava/lang/String;)V";
				nm.fnPtr = (void*)JNI_UncaughtException;
				if(((*env)->RegisterNatives(env, uncaughtHandlerClass, &nm, 1)) != 0) {
					fprintf(stderr, _(MSG_ID_ERR_REGISTER_NATIVE), "UncaughtException");
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
			fprintf(stderr, _(MSG_ID_ERR_DEFINE_CLASS), "PackLoader");
			exit_code = MSG_ID_ERR_DEFINE_CLASS;
			goto EXIT;
		}
		jmethodID packLoaderInit = (*env)->GetMethodID(env, packLoaderClass, "<init>", "()V");
		if(packLoaderInit == NULL) {
			fprintf(stderr, _(MSG_ID_ERR_GET_METHOD), "PackLoader#init()");
			exit_code = MSG_ID_ERR_GET_METHOD;
			goto EXIT;
		}
		jobject packLoader = (*env)->NewObject(env, packLoaderClass, packLoaderInit);
		if(packLoader == NULL) {
			fprintf(stderr, _(MSG_ID_ERR_NEW_OBJECT), "PackLoader");
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
			exit_code = MSG_ID_ERR_GET_METHOD;
			goto EXIT;
		}
		mainClass = (jclass)((*env)->CallObjectMethod(env, packLoader, packLoaderInitializeMethod, classesPackGz, resourcesGz, splashPath, splashImage));
	} else if(GetResourceBuffer("CLASSIC_LOADER") != NULL) {
		// ClassicLoader
		jclass classicLoaderClass = (*env)->DefineClass(env, "ClassicLoader", NULL, GetResourceBuffer("CLASSIC_LOADER"), GetResourceSize("CLASSIC_LOADER"));
		if(classicLoaderClass == NULL) {
			fprintf(stderr, _(MSG_ID_ERR_DEFINE_CLASS), "ClassicLoader");
			exit_code = MSG_ID_ERR_DEFINE_CLASS;
			goto EXIT;
		}
		jmethodID classicLoaderInit = (*env)->GetMethodID(env, classicLoaderClass, "<init>", "()V");
		if(classicLoaderInit == NULL) {
			fprintf(stderr, _(MSG_ID_ERR_GET_METHOD), "ClassicLoader#init()");
			exit_code = MSG_ID_ERR_GET_METHOD;
			goto EXIT;
		}
		jobject classicLoader = (*env)->NewObject(env, classicLoaderClass, classicLoaderInit);
		if(classicLoader == NULL) {
			fprintf(stderr, _(MSG_ID_ERR_NEW_OBJECT), "ClassicLoader");
			exit_code = MSG_ID_ERR_NEW_OBJECT;
			goto EXIT;
		}
		jbyteArray jar = (*env)->NewByteArray(env, GetResourceSize("JAR"));
		(*env)->SetByteArrayRegion(env, jar, 0, GetResourceSize("JAR"), GetResourceBuffer("JAR"));
		jmethodID classicLoaderInitializeMethod = (*env)->GetMethodID(env, classicLoaderClass, "initialize", "([B)Ljava/lang/Class;");
		if(classicLoaderInitializeMethod == NULL) {
			fprintf(stderr, _(MSG_ID_ERR_GET_METHOD), "ClassicLoader#initialize(byte[])");
			exit_code = MSG_ID_ERR_GET_METHOD;
			goto EXIT;
		}
		mainClass = (jclass)((*env)->CallObjectMethod(env, classicLoader, classicLoaderInitializeMethod, jar));
	} else {
		fprintf(stderr, _(MSG_ID_ERR_FIND_CLASSLOADER));
		exit_code = MSG_ID_ERR_FIND_CLASSLOADER;
		goto EXIT;
	}
	// Main-Class
	if(mainClass == NULL) {
		(*env)->ExceptionDescribe(env);
		exit_code = MSG_ID_ERR_LOAD_MAIN_CLASS;
		goto EXIT;
	}
	jmethodID startMethod = (*env)->GetStaticMethodID(env, mainClass, "start", "([Ljava/lang/String;)V");
	if(startMethod == NULL) {
		strcpy(message, _(MSG_ID_ERR_FIND_METHOD_SERVICE_START));
		if(isService) {
			WriteEventLog(EVENTLOG_ERROR_TYPE, message);
		} else {
			fprintf(stderr, "%s\n", message);
		}
		exit_code = MSG_ID_ERR_FIND_METHOD_SERVICE_START;
		goto EXIT;
	}
	stopMethod = (*env)->GetStaticMethodID(env, mainClass, "stop", "()V");
	if(stopMethod == NULL) {
		strcpy(message, _(MSG_ID_ERR_FIND_METHOD_SERVICE_STOP));
		if(isService) {
			WriteEventLog(EVENTLOG_ERROR_TYPE, message);
		} else {
			fprintf(stderr, "%s\n", message);
		}
		exit_code = MSG_ID_ERR_FIND_METHOD_SERVICE_STOP;
		goto EXIT;
	}

	jobjectArray args;
	if(isService && argc > 2) {
		args = (*env)->NewObjectArray(env, argc - 2, (*env)->FindClass(env, "java/lang/String"), NULL);
		for(i = 2; i < argc; i++) {
			(*env)->SetObjectArrayElement(env, args, (i - 2), GetJString(env, argv[i]));
		}
	} else if(!isService && argc > 1) {
		args = (*env)->NewObjectArray(env, argc - 1, (*env)->FindClass(env, "java/lang/String"), NULL);
		for(i = 1; i < argc; i++) {
			(*env)->SetObjectArrayElement(env, args, (i - 1), GetJString(env, argv[i]));
		}
	} else {
		args = (*env)->NewObjectArray(env, 0, (*env)->FindClass(env, "java/lang/String"), NULL);
	}

	strcpy(service_name, argv[0]);
	sprintf(message, _(MSG_ID_SUCCESS_SERVICE_START), service_name);
	if(isService) {
		WriteEventLog(EVENTLOG_INFORMATION_TYPE, message);
	} else {
		fprintf(stdout, "%s\n", message);
	}

	// JavaVM が CTRL_SHUTDOWN_EVENT を受け取って終了してしまわないように、ハンドラを登録して先取りします。
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)HandlerRoutine, TRUE);
	// シャットダウン時にダイアログが表示されないようにします。
	SetProcessShutdownParameters(0x4FF, SHUTDOWN_NORETRY);

	(*env)->CallStaticVoidMethod(env, mainClass, startMethod, args);

	if((*env)->ExceptionCheck(env) == JNI_TRUE) {
		if(isService) {
			jthrowable throwable = (*env)->ExceptionOccurred(env);
			(*env)->ExceptionClear(env);
			jmethodID getStackTraceMethod = (*env)->GetStaticMethodID(env, eventLogStreamClass, "getStackTrace", "(Ljava/lang/Throwable;)Ljava/lang/String;");
			jstring s = (jstring)(*env)->CallStaticObjectMethod(env, eventLogStreamClass, getStackTraceMethod, throwable);
			sprintf(message, _(MSG_ID_ERR_SERVICE_ABORT), service_name);
			strcat(message, "\r\n\r\n");
			strcat(message, GetShiftJIS(env, s));
			WriteEventLog(EVENTLOG_ERROR_TYPE, message);
			exit_code = 0;
			goto EXIT;
		} else {
			fprintf(stderr, _(MSG_ID_ERR_SERVICE_ABORT), service_name);
			fprintf(stderr, "\r\n\r\n");
			(*env)->ExceptionDescribe(env);
			(*env)->ExceptionClear(env);
		}
	} else {
		sprintf(message, _(MSG_ID_SUCCESS_SERVICE_STOP), service_name);
		if(isService) {
			WriteEventLog(EVENTLOG_INFORMATION_TYPE, message);
		} else {
			fprintf(stdout, "%s\n", message);
		}
	}

EXIT:
	if(env != NULL) {
		DetachJavaVM();
	}
	//デーモンではないスレッド(たとえばSwing)が残っていると待機状態になってしまうため、
	//サービスでは、DestroyJavaVM() を実行しないようにしています。
	//if(jvm != NULL) {
	//	DestroyJavaVM();
	//}

	return exit_code;
}

int service_stop() {
	JNIEnv* env = AttachJavaVM();
	(*env)->CallStaticVoidMethod(env, mainClass, stopMethod);
	DetachJavaVM();
	return 0;
}

int service_error() {

}

int main_start(int argc, char* argv[]) {
	jclass mainClass = NULL;
	jmethodID mainMethod = NULL;
	int exit_code;
	int i;
	BOOL useServerVM = TRUE;
	int err;
	LPTSTR ext_flags = NULL;
	LPTSTR vm_args_opt = NULL;
	LPTSTR relative_classpath = NULL;
	LPTSTR relative_extdirs = "lib";

	if(GetResourceSize("RELATIVE_CLASSPATH") > 0) {
		relative_classpath = (LPTSTR)GetResourceBuffer("RELATIVE_CLASSPATH");
	}
	if(GetResourceSize("EXTDIRS") > 0) {
		relative_extdirs = (LPTSTR)GetResourceBuffer("EXTDIRS");
	}
	if(GetResourceSize("EXTFLAGS") > 0) {
		ext_flags = _strupr((LPTSTR)GetResourceBuffer("EXTFLAGS"));
	}
	if(ext_flags != NULL && strstr(ext_flags, "CLIENT") != NULL) {
		useServerVM = FALSE;
	}

	InitializePath(relative_classpath, relative_extdirs, useServerVM);

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
	}

	DWORD version = GetJavaRuntimeVersion();
	LPTSTR targetVersionString = (LPTSTR)GetResourceBuffer("TARGET_VERSION");
	DWORD targetVersion = *(DWORD*)targetVersionString;
	if(targetVersion > version) {
		fprintf(stderr, _(MSG_ID_ERR_TARGET_VERSION), targetVersionString + 4);
		exit_code = MSG_ID_ERR_TARGET_VERSION;
		goto EXIT;
	}

	// URLConnection
	jclass urlConnectionClass = (*env)->DefineClass(env, "URLConnection", NULL, GetResourceBuffer("URL_CONNECTION"), GetResourceSize("URL_CONNECTION"));
	if(urlConnectionClass == NULL) {
		fprintf(stderr, _(MSG_ID_ERR_DEFINE_CLASS), "URLConnection");
		exit_code = MSG_ID_ERR_DEFINE_CLASS;
		goto EXIT;
	}
	// URLStreamHandler
	jclass urlStreamHandlerClass = (*env)->DefineClass(env, "URLStreamHandler", NULL, GetResourceBuffer("URL_STREAM_HANDLER"), GetResourceSize("URL_STREAM_HANDLER"));
	if(urlStreamHandlerClass == NULL) {
		fprintf(stderr, _(MSG_ID_ERR_DEFINE_CLASS), "URLStreamHandler");
		exit_code = MSG_ID_ERR_DEFINE_CLASS;
		goto EXIT;
	}
	// Loader
	if(GetResourceBuffer("PACK_LOADER") != NULL) {
		// PackLoader
		jclass packLoaderClass = (*env)->DefineClass(env, "PackLoader", NULL, GetResourceBuffer("PACK_LOADER"), GetResourceSize("PACK_LOADER"));
		if(packLoaderClass == NULL) {
			fprintf(stderr, _(MSG_ID_ERR_DEFINE_CLASS), "PackLoader");
			exit_code = MSG_ID_ERR_DEFINE_CLASS;
			goto EXIT;
		}
		jmethodID packLoaderInit = (*env)->GetMethodID(env, packLoaderClass, "<init>", "()V");
		if(packLoaderInit == NULL) {
			fprintf(stderr, _(MSG_ID_ERR_GET_METHOD), "PackLoader#init()");
			exit_code = MSG_ID_ERR_GET_METHOD;
			goto EXIT;
		}
		jobject packLoader = (*env)->NewObject(env, packLoaderClass, packLoaderInit);
		if(packLoader == NULL) {
			fprintf(stderr, _(MSG_ID_ERR_NEW_OBJECT), "PackLoader");
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
			exit_code = MSG_ID_ERR_GET_METHOD;
			goto EXIT;
		}
		mainClass = (jclass)((*env)->CallObjectMethod(env, packLoader, packLoaderInitializeMethod, classesPackGz, resourcesGz, splashPath, splashImage));
	} else if(GetResourceBuffer("CLASSIC_LOADER") != NULL) {
		// ClassicLoader
		jclass classicLoaderClass = (*env)->DefineClass(env, "ClassicLoader", NULL, GetResourceBuffer("CLASSIC_LOADER"), GetResourceSize("CLASSIC_LOADER"));
		if(classicLoaderClass == NULL) {
			fprintf(stderr, _(MSG_ID_ERR_DEFINE_CLASS), "ClassicLoader");
			exit_code = MSG_ID_ERR_DEFINE_CLASS;
			goto EXIT;
		}
		jmethodID classicLoaderInit = (*env)->GetMethodID(env, classicLoaderClass, "<init>", "()V");
		if(classicLoaderInit == NULL) {
			fprintf(stderr, _(MSG_ID_ERR_GET_METHOD), "ClassicLoader#init()");
			exit_code = MSG_ID_ERR_GET_METHOD;
			goto EXIT;
		}
		jobject classicLoader = (*env)->NewObject(env, classicLoaderClass, classicLoaderInit);
		if(classicLoader == NULL) {
			fprintf(stderr, _(MSG_ID_ERR_NEW_OBJECT), "ClassicLoader");
			exit_code = MSG_ID_ERR_NEW_OBJECT;
			goto EXIT;
		}
		jbyteArray jar = (*env)->NewByteArray(env, GetResourceSize("JAR"));
		(*env)->SetByteArrayRegion(env, jar, 0, GetResourceSize("JAR"), GetResourceBuffer("JAR"));
		jmethodID classicLoaderInitializeMethod = (*env)->GetMethodID(env, classicLoaderClass, "initialize", "([B)Ljava/lang/Class;");
		if(classicLoaderInitializeMethod == NULL) {
			fprintf(stderr, _(MSG_ID_ERR_GET_METHOD), "ClassicLoader#initialize(byte[])");
			exit_code = MSG_ID_ERR_GET_METHOD;
			goto EXIT;
		}
		mainClass = (jclass)((*env)->CallObjectMethod(env, classicLoader, classicLoaderInitializeMethod, jar));
	} else {
		fprintf(stderr, _(MSG_ID_ERR_FIND_CLASSLOADER));
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
		exit_code = MSG_ID_ERR_FIND_MAIN_METHOD;
		goto EXIT;
	}
	jobjectArray args = (*env)->NewObjectArray(env, argc - 1, (*env)->FindClass(env, "java/lang/String"), NULL);
	for(i = 1; i < argc; i++) {
		(*env)->SetObjectArrayElement(env, args, (i - 1), GetJString(env, argv[i]));
	}
	(*env)->CallStaticVoidMethod(env, mainClass, mainMethod, args);
	
	(*env)->ExceptionDescribe(env);
	(*env)->ExceptionClear(env);

EXIT:
	if(env != NULL) {
		DetachJavaVM();
	}
	if(jvm != NULL) {
		DestroyJavaVM();
	}
	return exit_code;
}


BOOL WINAPI HandlerRoutine(DWORD dwCtrlType) {
	static int ctrl_c = 0;

	switch(dwCtrlType) {
		case CTRL_C_EVENT:
			if(ctrl_c++ == 0) { //初回は終了処理を試みます。
				printf(_(MSG_ID_CTRL_SERVICE_STOP), "CTRL_C");
				printf("\r\n");
				service_stop();
				return TRUE;
			} else {
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
			service_stop();
			return TRUE;

		case CTRL_LOGOFF_EVENT:
			if(isService == FALSE) {
				printf(_(MSG_ID_CTRL_SERVICE_STOP), "CTRL_LOGOFF");
				printf("\r\n");
				service_stop();
			}
			return TRUE;

		case CTRL_SHUTDOWN_EVENT:
			if(isService == FALSE) {
				printf(_(MSG_ID_CTRL_SERVICE_STOP), "CTRL_SHUTDOWN");
				printf("\r\n");
				service_stop();
			}
			return TRUE;
	}
	return FALSE;
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

void JNICALL JNI_WriteEventLog(JNIEnv *env, jobject clazz, jint logType, jstring message) {
	WORD nType = EVENTLOG_INFORMATION_TYPE;
	switch(logType) {
		case 0: nType = EVENTLOG_INFORMATION_TYPE; break;
		case 1: nType = EVENTLOG_WARNING_TYPE;     break;
		case 2: nType = EVENTLOG_ERROR_TYPE;       break;
	}
	WriteEventLog(nType, GetShiftJIS(env, message));
}

void JNICALL JNI_UncaughtException(JNIEnv *env, jobject clazz, jstring message, jstring trace) {
	char buffer[2048];
	sprintf(buffer, _(MSG_ID_ERR_SERVICE_ABORT), service_name);
	strcat(buffer, "\r\n\r\n");
	strcat(buffer, GetShiftJIS(env, trace));
	if(isService) {
		WriteEventLog(EVENTLOG_ERROR_TYPE, buffer);
	} else {
		fprintf(stderr, "%s\r\n", buffer);
	}
	exit(0);
}
