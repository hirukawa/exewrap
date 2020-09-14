/* このファイルの文字コードは Shift_JIS (MS932) です。*/

#include <windows.h>
#include <stdio.h>
#include <shlobj.h>
#include <jni.h>

#define JVM_ELOADLIB  (+1)

#define VM_SEARCH_APPDIR    (0x00000001)
#define VM_SEARCH_PARENTDIR (0x00000002)
#define VM_SEARCH_JAVAHOME  (0x00000004)
#define VM_SEARCH_REGISTRY  (0x00000008)
#define VM_SEARCH_PATHENV   (0x00000010)
#define VM_SEARCH_JARASSOC  (0x00000020)
#define VM_SEARCH_ALL       (0xFFFFFFFF)

#define BUFFER_SIZE   32768
#define MAX_LONG_PATH 32768

int      get_process_architecture(void);
int      get_platform_architecture(void);
BOOL     initialize_path(const wchar_t* relative_classpath, const wchar_t* relative_extdirs, BOOL use_server_vm, DWORD vm_search_locations);
JNIEnv*  create_java_vm(const wchar_t* vm_args_opt, BOOL use_server_vm, DWORD vm_search_locations, BOOL* is_security_manager_required, int* error);
jint     destroy_java_vm(void);
JNIEnv*  attach_java_vm(void);
jint     detach_java_vm(void);
BOOL     set_application_properties(SYSTEMTIME* startup);
wchar_t* get_class_path(void);
void     get_java_runtime_version(const wchar_t* version_string, DWORD* major, DWORD* minor, DWORD* build, DWORD* revision);
wchar_t* get_java_version_string(DWORD major, DWORD minor, DWORD build, DWORD revision);
wchar_t* get_module_version(wchar_t* buf, size_t size);

char*    to_platform_encoding(const wchar_t* str);
char*    to_utf8(const wchar_t* str);
wchar_t* from_utf8(const char* utf8);
jstring  to_jstring(JNIEnv* env, const wchar_t* str);
wchar_t* from_jstring(JNIEnv* env, jstring jstr);

static BOOL     find_java_vm(wchar_t* output, const wchar_t* jre, BOOL use_server_vm);
static wchar_t* find_java_home_from_registry(const wchar_t* _subkey, wchar_t* output);
static BOOL     add_path_env(const wchar_t* path);
static void     add_dll_directory(const wchar_t* path);
static BOOL     is_directory(const wchar_t* path);
static wchar_t* get_sub_dirs(const wchar_t* dir);
static BOOL     add_sub_dirs(wchar_t* buf, size_t buf_size, const wchar_t* dir, size_t* size);
static wchar_t* get_jars(const wchar_t* dir);
static BOOL     add_jars(wchar_t* buf, size_t buf_size, const wchar_t* dir, size_t* size);
static int      get_java_vm_bits(const wchar_t* jvmpath);
static wchar_t* urldecode(wchar_t *dst, size_t dst_size, const wchar_t *src);

typedef PVOID (WINAPI* Kernel32_AddDllDirectory)(PCWSTR);
typedef jint (WINAPI* JNIGetDefaultJavaVMInitArgs)(JavaVMInitArgs*);
typedef jint (WINAPI* JNICreateJavaVM)(JavaVM**, void**, JavaVMInitArgs*);

JavaVM* jvm = NULL;
JNIEnv* env = NULL;
BOOL    is_add_dll_directory_supported = FALSE;
BOOL    is_security_manager_required   = FALSE;

static BOOL     path_initialized = FALSE;
static wchar_t* binpath = NULL;
static wchar_t* jvmpath = NULL;
static wchar_t* libpath = NULL;

static HMODULE jvmdll = NULL;
static HMODULE kernel32 = NULL;
static Kernel32_AddDllDirectory _AddDllDirectory = NULL;


/* このプロセスのアーキテクチャ(32ビット/64ビット)を返します。
 * 64ビットOSで32ビットプロセスを実行している場合、この関数は32を返します。
 * 戻り値として32ビットなら 32 を返します。64ビットなら 64 を返します。
 */
int get_process_architecture()
{
	return sizeof(int*) * 8;
}


/* OSのアーキテクチャ(32ビット/64ビット)を返します。
 * 64ビットOSで32ビットプロセスを実行している場合、この関数は64を返します。
 * 戻り値として32ビットなら 32 を返します。64ビットなら 64 を返します。
 */
int get_platform_architecture()
{
	wchar_t buf[256];
	if(GetEnvironmentVariable(L"PROCESSOR_ARCHITECTURE", buf, 256))
	{
		if(wcsstr(buf, L"64") != NULL)
		{
			return 64;
		}
		else if(GetEnvironmentVariable(L"PROCESSOR_ARCHITEW6432", buf, 256))
		{
			if(wcsstr(buf, L"64") != NULL)
			{
				return 64;
			}
		}
	}
	return 32;
}


JNIEnv* create_java_vm(const wchar_t* vm_args_opt, BOOL use_server_vm, DWORD vm_search_locations, BOOL* is_security_manager_required, int* error)
{
	JNIGetDefaultJavaVMInitArgs GetDefaultJavaVMInitArgs;
	JNICreateJavaVM             CreateJavaVM;
	JavaVMOption                options[256];
	JavaVMInitArgs              vm_args;
	char*                       char_buf  = NULL;
	wchar_t*                    wchar_buf = NULL;
	int                         err       = 0;

	vm_args.version = JNI_VERSION_1_2;
	vm_args.options = options;
	vm_args.nOptions = 0;
	vm_args.ignoreUnrecognized = 1;

	if(!path_initialized)
	{
		initialize_path(NULL, L"lib", use_server_vm, vm_search_locations);
	}

	if(jvmdll == NULL)
	{
		if(_AddDllDirectory != NULL)
		{
			jvmdll = LoadLibraryEx(L"jvm.dll", NULL, 0x00001000); // LOAD_LIBRARY_SEARCH_DEFAULT_DIRS (0x00001000)
		}
		else
		{
			// Windows XP以前はAddDllDirectory関数がなくDLL参照ディレクトリを細かく制御することができません。
			// Windows XP以前ではPATH環境変数からもDLLを参照できるように従来のライブラリ参照方法を使用します。
			jvmdll = LoadLibrary(L"jvm.dll");
		}
		if(jvmdll == NULL)
		{
			err = JVM_ELOADLIB;
			goto EXIT;
		}
	}
	
	GetDefaultJavaVMInitArgs = (JNIGetDefaultJavaVMInitArgs)GetProcAddress(jvmdll, "JNI_GetDefaultJavaVMInitArgs");
	CreateJavaVM = (JNICreateJavaVM)GetProcAddress(jvmdll, "JNI_CreateJavaVM");

	char_buf = (char*)malloc(BUFFER_SIZE * 2);
	if(char_buf == NULL)
	{
		err = JNI_ENOMEM;
		goto EXIT;
	}
	wchar_buf = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(wchar_buf == NULL)
	{
		err = JNI_ENOMEM;
		goto EXIT;
	}

	if(GetModuleFileName(NULL, wchar_buf, BUFFER_SIZE) != 0)
	{
		char*  str;
		size_t len;

		str = to_platform_encoding(wchar_buf);
		strcpy_s(char_buf, BUFFER_SIZE * 2, "-Djava.class.path=");
		strcat_s(char_buf, BUFFER_SIZE * 2, str);
		free(str);

		len = strlen(char_buf);
		str = (char*)malloc(len + 1);
		if(str == NULL)
		{
			err = JNI_ENOMEM;
			goto EXIT;
		}
		strcpy_s(str, len + 1, char_buf);
		options[vm_args.nOptions++].optionString = str;
	}

	if(libpath != NULL)
	{
		char*  str;
		size_t len;

		str = to_platform_encoding(libpath);
		strcpy_s(char_buf, BUFFER_SIZE * 2, "-Djava.library.path=");
		strcat_s(char_buf, BUFFER_SIZE * 2, str);
		free(str);

		len = strlen(char_buf);
		str = (char*)malloc(len + 1);
		if(str == NULL)
		{
			err = JNI_ENOMEM;
			goto EXIT;
		}
		strcpy_s(str, len + 1, char_buf);
		options[vm_args.nOptions++].optionString = str;
	}

	if(strcpy_s(char_buf, BUFFER_SIZE * 2, "-Djava.system.class.loader=Loader") == 0)
	{
		char*  str;
		size_t len;

		len = strlen(char_buf);
		str = (char*)malloc(len + 1);
		if(str == NULL)
		{
			err = JNI_ENOMEM;
			goto EXIT;
		}
		strcpy_s(str, len + 1, char_buf);
		options[vm_args.nOptions++].optionString = str;
	}

	if(strcpy_s(char_buf, BUFFER_SIZE * 2, "-XX:-UseSharedSpaces") == 0)
	{
		char*  str;
		size_t len;

		len = strlen(char_buf);
		str = (char*)malloc(len + 1);
		if(str == NULL)
		{
			err = JNI_ENOMEM;
			goto EXIT;
		}
		strcpy_s(str, len + 1, char_buf);
		options[vm_args.nOptions++].optionString = str;
	}

	if(GetModuleFileName(NULL, wchar_buf, BUFFER_SIZE) != 0)
	{
		wchar_t* p = wcsrchr(wchar_buf, L'\\');
		wchar_t* app_path = wchar_buf;
		wchar_t* app_name = p + 1;
		char*    str;
		size_t   len;

		*p = L'\0';

		str = to_platform_encoding(app_path);
		strcpy_s(char_buf, BUFFER_SIZE * 2, "-Djava.application.path=");
		strcat_s(char_buf, BUFFER_SIZE * 2, str);
		free(str);

		len = strlen(char_buf);
		str = (char*)malloc(len + 1);
		if(str == NULL)
		{
			err = JNI_ENOMEM;
			goto EXIT;
		}
		strcpy_s(str, len + 1, char_buf);
		options[vm_args.nOptions++].optionString = str;

		str = to_platform_encoding(app_name);
		strcpy_s(char_buf, BUFFER_SIZE * 2, "-Djava.application.name=");
		strcat_s(char_buf, BUFFER_SIZE * 2, str);
		free(str);

		len = strlen(char_buf);
		str = (char*)malloc(len + 1);
		if(str == NULL)
		{
			err = JNI_ENOMEM;
			goto EXIT;
		}
		strcpy_s(str, len + 1, char_buf);
		options[vm_args.nOptions++].optionString = str;
	}
	
	if(GetModuleFileName(NULL, wchar_buf, BUFFER_SIZE) != 0)
	{
		wchar_t* policy_file = wchar_buf;
		wchar_t* p;

		if((p = wcsrchr(policy_file, L'.')) != NULL)
		{
			*p = L'\0';
			wcscat_s(policy_file, BUFFER_SIZE, L".policy");
			if(GetFileAttributes(policy_file) != INVALID_FILE_ATTRIBUTES)
			{
				char*  str;
				size_t len;

				// -Djava.security.manager でセキュリティマネージャーをインストールすると、
				// CreateJavaVM 関数を呼び出したときに Loader クラスを見つけられなくなります。(ClassNotFoundException)
				// この問題を回避するために -Djava.security.manager を指定せずにJavaVM作成後にコードによってセキュリティマネージャーをインストールします。
				// is_security_manager_required はセキュリティマネージャーのインストールが必要であることを示すフラグです。
				if(is_security_manager_required != NULL)
				{
					*is_security_manager_required = TRUE;
				}

				str = to_platform_encoding(policy_file);
				strcpy_s(char_buf, BUFFER_SIZE * 2, "-Djava.security.policy=");
				strcat_s(char_buf, BUFFER_SIZE * 2, str);
				free(str);

				len = strlen(char_buf);
				str = (char*)malloc(len + 1);
				if(str == NULL)
				{
					err = JNI_ENOMEM;
					goto EXIT;
				}
				strcpy_s(str, len + 1, char_buf);
				options[vm_args.nOptions++].optionString = str;
			}
		}
	}
	
	if(vm_args_opt != NULL)
	{
		int       argc = 0;
		wchar_t** argv = NULL;

		argv = CommandLineToArgvW(vm_args_opt, &argc);
		if(argv != NULL && argc > 0)
		{
			int i;
			for(i = 0; i < argc; i++)
			{
				char* opt = to_platform_encoding(argv[i]);
				options[vm_args.nOptions++].optionString = opt;
			}
		}
		LocalFree(argv);
	}
	
	//read .exe.vmoptions
	//同じVM引数が複数回指定された場合は後優先になるので、.exe.vmoptionsを優先するために最後に追加します。
	//.exe.vmoptions の文字コードはプラットフォームエンコーディングである必要があります。
	//.exe.vmoptions は32KB以下である必要があります。
	if(GetModuleFileName(NULL, wchar_buf, BUFFER_SIZE) != 0)
	{
		wchar_t* vm_opt_file = wchar_buf;

		wcscat_s(vm_opt_file, MAX_LONG_PATH, L".vmoptions");
		if(GetFileAttributes(vm_opt_file) != INVALID_FILE_ATTRIBUTES)
		{
			HANDLE hFile = CreateFile(vm_opt_file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if(hFile != INVALID_HANDLE_VALUE)
			{
				char* ptr = char_buf;
				DWORD size = BUFFER_SIZE * 2;
				DWORD read_size;

				while(size > 0)
				{
					BOOL ret = ReadFile(hFile, ptr, size, &read_size, NULL);
					if(ret == FALSE) // ERROR
					{
						break;
					}
					if(read_size == 0) // END OF FILE
					{
						ptr = NULL;
						break;
					}
					ptr += read_size;
					size -= read_size;
				}
				if(ptr == NULL) // READ SUCCESSFUL
				{
					size_t line_len;
					char*  line = ptr = char_buf;
					while(*ptr != '\0')
					{
						if(*ptr == '\r' || *ptr == '\n')
						{
							*ptr = '\0';
							line_len = strlen(line);
							if(line_len > 0)
							{
								char* str = malloc(line_len + 1);
								if(str == NULL)
								{
									CloseHandle(hFile);
									err = JNI_ENOMEM;
									goto EXIT;
								}
								strcpy_s(str, line_len + 1, line);
								options[vm_args.nOptions++].optionString = str;
							}
							line = ptr + 1;
						}
						ptr++;
					}
				}
				CloseHandle(hFile);
			}
		}
	}
	
	/*
	// FIXME: debug
	{
		int i;
		for(i = 0; i < vm_args.nOptions; i++)
		{
			printf("[%d] %s\n\n", i, options[i].optionString);
		}
	}
	*/
	
	GetDefaultJavaVMInitArgs(&vm_args);
	err = CreateJavaVM(&jvm, (void**)&env, &vm_args);

EXIT:
	if(err != JNI_OK)
	{
		jvm = NULL;
		env = NULL;
	}
	if(wchar_buf != NULL)
	{
		free(wchar_buf);
	}
	if(char_buf != NULL)
	{
		free(char_buf);
	}

	while(vm_args.nOptions > 0)
	{
		free(options[vm_args.nOptions - 1].optionString);
		vm_args.nOptions--;
	}

	if(error != NULL)
	{
		*error = err;
	}
	return env;
}


jint destroy_java_vm()
{
	jint ret = JNI_ERR;

	if(jvm != NULL)
	{
		ret = (*jvm)->DestroyJavaVM(jvm);
		jvm = NULL;
	}

	if(libpath != NULL)
	{
		free(libpath);
		libpath = NULL;
	}

	return ret;
}


JNIEnv* attach_java_vm()
{
	JNIEnv* env = NULL;

	if(jvm != NULL)
	{
		if((*jvm)->AttachCurrentThread(jvm, (void**)&env, NULL) != JNI_OK)
		{
			env = NULL;
		}
	}

	return env;
}


jint detach_java_vm()
{
	jint ret = JNI_ERR;

	if(jvm != NULL)
	{
		ret = (*jvm)->DetachCurrentThread(jvm);
	}

	return ret;
}


void get_java_runtime_version(const wchar_t* version_string, DWORD* major, DWORD* minor, DWORD* build, DWORD* revision)
{
	wchar_t*  buffer  = NULL;
	wchar_t*  version = NULL;

	if(major != NULL)
	{
		*major = 0;
	}
	if(minor != NULL)
	{
		*minor = 0;
	}
	if(build != NULL)
	{
		*build = 0;
	}
	if(revision != NULL)
	{
		*revision = 0;
	}

	if(version_string == NULL)
	{
		jclass    System;
		jmethodID System_getProperty;

		System = (*env)->FindClass(env, "java/lang/System");
		if(System != NULL)
		{
			System_getProperty = (*env)->GetStaticMethodID(env, System, "getProperty", "(Ljava/lang/String;)Ljava/lang/String;");
			if(System_getProperty != NULL)
			{
				jstring  param = to_jstring(env, L"java.runtime.version");
				jstring  ret = (jstring)((*env)->CallStaticObjectMethod(env, System, System_getProperty, param));
				version = buffer = from_jstring(env, ret);
			}
		}
	}
	else
	{
		size_t len = wcslen(version_string);
		buffer = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
		if(buffer == NULL)
		{
			goto EXIT;
		}
		wcscpy_s(buffer, len + 1, version_string);
		version = buffer;
	}


	//数字が出てくるまでスキップ
	while(*version != L'\0' && !(L'0' <= *version && *version <= L'9'))
	{
		version++;
	}
	if(*version == L'\0')
	{
		goto EXIT;
	}
	if(major != NULL)
	{
		*major = _wtoi(version);
	}
	
	//数字以外が出てくるまでスキップ
	while(*version != L'\0' && (L'0' <= *version && *version <= L'9'))
	{
		version++;
	}
	if(*version == L'\0')
	{
		goto EXIT;
	}
	//数字が出てくるまでスキップ
	while(*version != L'\0' && !(L'0' <= *version && *version <= L'9'))
	{
		version++;
	}
	if(*version == L'\0')
	{
		goto EXIT;
	}
	if(minor != NULL)
	{
		*minor = _wtoi(version);
	}
	
	//数字以外が出てくるまでスキップ
	while(*version != L'\0' && (L'0' <= *version && *version <= L'9'))
	{
		version++;
	}
	if(*version == L'\0')
	{
		goto EXIT;
	}
	//数字が出てくるまでスキップ
	while(*version != L'\0' && !(L'0' <= *version && *version <= L'9'))
	{
		version++;
	}
	if(*version == L'\0')
	{
		goto EXIT;
	}
	if(build != NULL)
	{
		*build = _wtoi(version);
	}
	
	//数字以外が出てくるまでスキップ
	while(*version != L'\0' && (L'0' <= *version && *version <= L'9'))
	{
		version++;
	}
	if(*version == L'\0')
	{
		goto EXIT;
	}
	//数字が出てくるまでスキップ
	while(*version != L'\0' && !(L'0' <= *version && *version <= L'9'))
	{
		version++;
	}
	if(*version == L'\0')
	{
		goto EXIT;
	}
	if(revision != NULL)
	{
		*revision = _wtoi(version);
	}

EXIT:
	if(buffer != NULL)
	{
		free(buffer);
	}
}


wchar_t* get_java_version_string(DWORD major, DWORD minor, DWORD build, DWORD revision)
{
	int      len = 128;
	wchar_t* buf = (wchar_t*)malloc(128 * sizeof(wchar_t));

	if(buf == NULL)
	{
		return NULL;
	}

	//JDK9-
	if(major >= 5)
	{
		if(minor == 0 && build == 0 && revision == 0)
		{
			swprintf_s(buf, len, L"Java %d", major);
		}
		else if(build == 0 && revision == 0)
		{
			swprintf_s(buf, len, L"Java %d.%d", major, minor);
		}
		else if(revision == 0)
		{
			swprintf_s(buf, len, L"Java %d.%d.%d", major, minor, build);
		}
		else
		{
			swprintf_s(buf, len, L"Java %d.%d.%d", major, minor, build, revision);
		}
		return buf;
	}
	
	//1.7, 1.8
	if(major == 1 && (minor == 7 || minor == 8))
	{
		if(build == 0 )
		{
			if(revision == 0)
			{
				swprintf_s(buf, len, L"Java %d", minor);
			}
			else
			{
				swprintf_s(buf, len, L"Java %du%d", minor, revision);
			}
		}
		else
		{
			swprintf_s(buf, len, L"Java %d.%d.%d.%d", major, minor, build, revision);
		}
		return buf;
	}
	
	//1.5, 1.6
	if(major == 1 && (minor == 5 || minor == 6))
	{
		if(build == 0)
		{
			if(revision == 0)
			{
				swprintf_s(buf, len, L"Java %d.%d", minor, build);
			}
			else
			{
				swprintf_s(buf, len, L"Java %d.%d.%d", minor, build, revision);
			}
		}
		else
		{
				swprintf_s(buf, len, L"Java %d.%d.%d.%d", major, minor, build, revision);
		}
		return buf;
	}

	//1.2, 1.3, 1.4
	if(major == 1 && (minor == 2 || minor == 3 || minor == 4))
	{
		if(revision == 0)
		{
			if(build == 0)
			{
				swprintf_s(buf, len, L"Java2 %d.%d", major, minor);
			}
			else
			{
				swprintf_s(buf, len, L"Java2 %d.%d.%d", major, minor, build);
			}
		}
		else
		{
			swprintf_s(buf, len, L"Java2 %d.%d.%d.%d", major, minor, build, revision);
		}
		return buf;
	}

	//1.0, 1.1
	if(major == 1 && (minor == 0 || minor == 1))
	{
		if(revision == 0)
		{
			if(build == 0)
			{
				swprintf_s(buf, len, L"Java %d.%d", major, minor);
			}
			else
			{
				swprintf_s(buf, len, L"Java %d.%d.%d", major, minor, build);
			}
		}
		else
		{
			swprintf_s(buf, len, L"Java %d.%d.%d.%d", major, minor, build, revision);
		}
		return buf;
	}

	//other
	if(revision == 0)
	{
		swprintf_s(buf, len, L"Java %d.%d.%d", major, minor, build);
	}
	else
	{
		swprintf_s(buf, len, L"Java %d.%d.%d.%d", major, minor, build, revision);
	}
	return buf;
}


BOOL set_application_properties(SYSTEMTIME* startup)
{
	BOOL      succeeded = FALSE;
	wchar_t*  buffer = NULL;
	jclass    System;
	jmethodID System_setProperty;
	jstring   key;
	jstring   value;

	if(env == NULL)
	{
		goto EXIT;
	}
	buffer = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(buffer == NULL)
	{
		goto EXIT;
	}
	System = (*env)->FindClass(env, "java/lang/System");
	if(System == NULL)
	{
		goto EXIT;
	}
	System_setProperty = (*env)->GetStaticMethodID(env, System, "setProperty", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
	if(System_setProperty == NULL)
	{
		goto EXIT;
	}

	if(startup != NULL)
	{
		swprintf_s(buffer, BUFFER_SIZE, L"%u-%02u-%02uT%02u:%02u:%02u.%03uZ",
				startup->wYear, startup->wMonth, startup->wDay,
				startup->wHour, startup->wMinute, startup->wSecond,
				startup->wMilliseconds);
		key = to_jstring(env, L"java.application.startup");
		value = to_jstring(env, buffer);
		(*env)->CallStaticObjectMethod(env, System, System_setProperty, key, value);
	}

	// Loaderクラスのコンストラクタ内で実行ファイルのパスを参照する必要があるため、
	// java.application.path と java.application.name は create_java_vm ないで起動時パラメーターとして指定します。
	/*
	if(GetModuleFileName(NULL, buffer, BUFFER_SIZE) != 0)
	{
		wchar_t* p = wcsrchr(buffer, L'\\');
		wchar_t* app_path = buffer;
		wchar_t* app_name = p + 1;

		*p = L'\0';

		key = to_jstring(env, L"java.application.path");
		value = to_jstring(env, app_path);
		(*env)->CallStaticObjectMethod(env, System, System_setProperty, key, value);

		key = to_jstring(env, L"java.application.name");
		value = to_jstring(env, app_name);
		(*env)->CallStaticObjectMethod(env, System, System_setProperty, key, value);
	}
	*/

	if(get_module_version(buffer, BUFFER_SIZE) != NULL)
	{
		key = to_jstring(env, L"java.application.version");
		value = to_jstring(env, buffer);
		(*env)->CallStaticObjectMethod(env, System, System_setProperty, key, value);
	}

	succeeded = TRUE;

EXIT:
	if(buffer != NULL)
	{
		free(buffer);
	}
	return succeeded;
}


BOOL initialize_path(const wchar_t* relative_classpath, const wchar_t* relative_extdirs, BOOL use_server_vm, DWORD vm_search_locations)
{
	wchar_t* module_path = NULL;
	wchar_t* buffer      = NULL;
	wchar_t* search      = NULL;
	wchar_t* jdk         = NULL;
	WIN32_FIND_DATA fd;
	HANDLE hSearch;
	BOOL found = FALSE;
	wchar_t* token;
	wchar_t* context;

	path_initialized = FALSE;

	if(kernel32 == NULL)
	{
		kernel32 = LoadLibrary(L"kernel32");
	}
	if(kernel32 != NULL && _AddDllDirectory == NULL)
	{
		_AddDllDirectory = (Kernel32_AddDllDirectory)GetProcAddress(kernel32, "AddDllDirectory");
		if(_AddDllDirectory != NULL)
		{
			is_add_dll_directory_supported = TRUE;
		}
	}

	if(binpath == NULL)
	{
		binpath = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
		if(binpath == NULL)
		{
			goto EXIT;
		}
	}
	if(jvmpath == NULL)
	{
		jvmpath = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
		if(jvmpath == NULL)
		{
			goto EXIT;
		}
	}
	if(libpath == NULL)
	{
		libpath = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
		if(libpath == NULL)
		{
			goto EXIT;
		}
	}


	buffer = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(buffer == NULL)
	{
		goto EXIT;
	}
	search = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(search == NULL)
	{
		goto EXIT;
	}
	jdk = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(jdk == NULL)
	{
		goto EXIT;
	}

	module_path = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(module_path == NULL)
	{
		goto EXIT;
	}
	if(GetModuleFileName(NULL, module_path, MAX_LONG_PATH) == 0)
	{
		goto EXIT;
	}
	*(wcsrchr(module_path, L'\\')) = L'\0';

	jvmpath[0] = L'\0';

	// Find local JDK
	if((vm_search_locations & VM_SEARCH_APPDIR) && jvmpath[0] == L'\0')
	{
		wcscpy_s(buffer, BUFFER_SIZE, module_path);
		wcscpy_s(search, MAX_LONG_PATH, buffer);
		wcscat_s(search, MAX_LONG_PATH, L"\\jdk*");
		hSearch = FindFirstFile(search, &fd);
		if(hSearch != INVALID_HANDLE_VALUE)
		{
			found = FALSE;
			do
			{
				if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					wcscat_s(buffer, BUFFER_SIZE, L"\\");
					wcscat_s(buffer, BUFFER_SIZE, fd.cFileName);
					found = TRUE;
				}
			} while (!found && FindNextFile(hSearch, &fd));
			FindClose(hSearch);
			if(found)
			{
				//JDK9以降、jvm.dllの場所が変更になりました。
				//JDK8までは jdk-8u152/jre/bin/server/jvm.dll のようにjdkの中にjreがありましたが、
				//JDK9以降は jdk-9/bin/server/jvm.dll のようにjreフォルダなしで直接binが配置されるようになりました。
				//そのため、jdk に bin/server/jvm.dll があるか探してなければ従来通り、jre/bin/server/jvm.dll を探します。
				if(find_java_vm(jvmpath, buffer, use_server_vm))
				{
					int bits = get_java_vm_bits(jvmpath);
					if(bits == get_process_architecture())
					{
						if(buffer[wcslen(buffer) - 1] != L'\\')
						{
							wcscat_s(buffer, BUFFER_SIZE, L"\\");
						}
						SetEnvironmentVariable(L"JAVA_HOME", buffer);
						wcscpy_s(binpath, MAX_LONG_PATH, buffer);
						wcscat_s(binpath, MAX_LONG_PATH, L"bin");
					}
					else
					{
						jvmpath[0] = L'\0';
					}
				}
				else
				{
					//jdk直下には bin/server/jvm.dll がなかったので、jre を連結して、
					//jre/bin/server/jvm.dll を探します。
					wcscpy_s(jdk, MAX_LONG_PATH, buffer);
					wcscat_s(buffer, BUFFER_SIZE, L"\\jre");
					if(find_java_vm(jvmpath, buffer, use_server_vm))
					{
						int bits = get_java_vm_bits(jvmpath);
						if(bits == get_process_architecture())
						{
							//jdk/jreが見つかってもJAVA_HOMEに設定するのはjdkディレクトリです。
							if(jdk[wcslen(jdk) - 1] != L'\\')
							{
								wcscat_s(jdk, MAX_LONG_PATH, L"\\");
							}
							SetEnvironmentVariable(L"JAVA_HOME", jdk);
							wcscpy_s(binpath, MAX_LONG_PATH, jdk);
							wcscat_s(binpath, MAX_LONG_PATH, L"bin");
						}
						else
						{
							jvmpath[0] = L'\0';
						}
					}
				}
			}
		}
	}

	// Find local JRE
	if((vm_search_locations & VM_SEARCH_APPDIR) && jvmpath[0] == L'\0')
	{
		wcscpy_s(buffer, BUFFER_SIZE, module_path);
		wcscpy_s(search, MAX_LONG_PATH, buffer);
		wcscat_s(search, MAX_LONG_PATH, L"\\jre*");
		hSearch = FindFirstFile(search, &fd);
		if(hSearch != INVALID_HANDLE_VALUE)
		{
			found = FALSE;
			do
			{
				if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					wcscat_s(buffer, BUFFER_SIZE, L"\\");
					wcscat_s(buffer, BUFFER_SIZE, fd.cFileName);
					found = TRUE;
				}
			} while (!found && FindNextFile(hSearch, &fd));
			FindClose(hSearch);
			if(found)
			{
				if(find_java_vm(jvmpath, buffer, use_server_vm))
				{
					int bits = get_java_vm_bits(jvmpath);
					if(bits == get_process_architecture())
					{
						if(buffer[wcslen(buffer) - 1] != L'\\')
						{
							wcscat_s(buffer, BUFFER_SIZE, L"\\");
						}
						SetEnvironmentVariable(L"JAVA_HOME", buffer);
						wcscpy_s(binpath, MAX_LONG_PATH, buffer);
						wcscat_s(binpath, MAX_LONG_PATH, L"bin");
					}
					else
					{
						jvmpath[0] = L'\0';
					}
				}
			}
		}
	}

	// Find local JDK (from parent folder)
	if((vm_search_locations & VM_SEARCH_PARENTDIR) && jvmpath[0] == L'\0')
	{
		wcscpy_s(buffer, BUFFER_SIZE, module_path);
		wcscpy_s(search, MAX_LONG_PATH, buffer);
		wcscat_s(search, MAX_LONG_PATH, L"\\..\\jdk*");
		hSearch = FindFirstFile(search, &fd);
		if(hSearch != INVALID_HANDLE_VALUE)
		{
			found = FALSE;
			do
			{
				if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					wcscat_s(buffer, BUFFER_SIZE, L"\\..\\");
					wcscat_s(buffer, BUFFER_SIZE, fd.cFileName);
					found = TRUE;
				}
			} while (!found && FindNextFile(hSearch, &fd));
			FindClose(hSearch);
			if(found)
			{
				if(find_java_vm(jvmpath, buffer, use_server_vm))
				{
					int bits = get_java_vm_bits(jvmpath);
					if(bits == get_process_architecture())
					{
						if(buffer[wcslen(buffer) - 1] != L'\\')
						{
							wcscat_s(buffer, BUFFER_SIZE, L"\\");
						}
						SetEnvironmentVariable(L"JAVA_HOME", buffer);
						wcscpy_s(binpath, MAX_LONG_PATH, buffer);
						wcscat_s(binpath, MAX_LONG_PATH, L"bin");
					}
					else
					{
						jvmpath[0] = L'\0';
					}
				}
			}
		}
	}

	// Find local JRE (from parent folder)
	if((vm_search_locations & VM_SEARCH_PARENTDIR) && jvmpath[0] == L'\0')
	{
		wcscpy_s(buffer, BUFFER_SIZE, module_path);
		wcscpy_s(search, MAX_LONG_PATH, buffer);
		wcscat_s(search, MAX_LONG_PATH, L"\\..\\jre*");
		hSearch = FindFirstFile(search, &fd);
		if(hSearch != INVALID_HANDLE_VALUE)
		{
			found = FALSE;
			do
			{
				if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					wcscat_s(buffer, BUFFER_SIZE, L"\\..\\");
					wcscat_s(buffer, BUFFER_SIZE, fd.cFileName);
					found = TRUE;
				}
			} while (!found && FindNextFile(hSearch, &fd));
			FindClose(hSearch);
			if(found)
			{
				if(find_java_vm(jvmpath, buffer, use_server_vm))
				{
					int bits = get_java_vm_bits(jvmpath);
					if(bits == get_process_architecture())
					{
						if(buffer[wcslen(buffer) - 1] != L'\\')
						{
							wcscat_s(buffer, BUFFER_SIZE, L"\\");
						}
						SetEnvironmentVariable(L"JAVA_HOME", buffer);
						wcscpy_s(binpath, MAX_LONG_PATH, buffer);
						wcscat_s(binpath, MAX_LONG_PATH, L"bin");
					}
					else
					{
						jvmpath[0] = L'\0';
					}
				}
			}
		}
	}

	// Find JDK/JRE from JAVA_HOME
	if((vm_search_locations & VM_SEARCH_JAVAHOME) && jvmpath[0] == L'\0')
	{
		buffer[0] = L'\0';
		if(GetEnvironmentVariable(L"JAVA_HOME", buffer, MAX_LONG_PATH) > 0)
		{
            // 下位に jre フォルダーがあるか確認します。
            if(buffer[wcslen(buffer) - 1] == L'\\')
            {
                wcscat_s(buffer, BUFFER_SIZE, L"jre");
            }
            else
            {
                wcscat_s(buffer, BUFFER_SIZE, L"\\jre");
            }
            // 下位に jre フォルダーがなければ元のフォルダーに戻します。
            if(!is_directory(buffer))
            {
                *(wcsrchr(buffer, L'\\')) = L'\0';
            }
            if(is_directory(buffer))
            {
                if(find_java_vm(jvmpath, buffer, use_server_vm))
                {
                    int bits = get_java_vm_bits(jvmpath);
                    if(bits == get_process_architecture())
                    {
                        if(buffer[wcslen(buffer) - 1] != L'\\')
                        {
                            wcscat_s(buffer, BUFFER_SIZE, L"\\");
                        }
                        SetEnvironmentVariable(L"JAVA_HOME", buffer);
                        wcscpy_s(binpath, MAX_LONG_PATH, buffer);
                        wcscat_s(binpath, MAX_LONG_PATH, L"bin");
                    }
                    else
                    {
                        jvmpath[0] = L'\0';
                    }
                }
            }
		}
	}

	// Find JDK/JRE from registry
	if((vm_search_locations & VM_SEARCH_REGISTRY) && jvmpath[0] == L'\0')
	{
        wchar_t* subkeys_native[] =
        {
            L"SOFTWARE\\JavaSoft\\JDK", //Java9-
            L"SOFTWARE\\JavaSoft\\JRE", //Java9-
            L"SOFTWARE\\JavaSoft\\Java Development Kit",
            L"SOFTWARE\\JavaSoft\\Java Runtime Environment",
            NULL
        };

        wchar_t* subkeys_wow[] =
        {
            L"SOFTWARE\\Wow6432Node\\JavaSoft\\JDK", //Java9-
            L"SOFTWARE\\Wow6432Node\\JavaSoft\\JRE", //Java9-
            L"SOFTWARE\\Wow6432Node\\JavaSoft\\Java Development Kit",
            L"SOFTWARE\\Wow6432Node\\JavaSoft\\Java Runtime Environment",
            NULL
        };

        wchar_t** subkeys = subkeys_native;
        int i = 0;

        //32ビットプロセスを64ビットOSで実行している場合はWowレジストリから検索します。
        if(get_process_architecture() == 32 && get_platform_architecture() == 64)
        {
            subkeys = subkeys_wow;
        }

        while(subkeys[i] != NULL)
        {
            if(find_java_home_from_registry(subkeys[i], buffer) != NULL)
            {
                // JDK/JREをアンインストールしてもレジストリが残ってしまうディストリビューションがあるようです。
                // ディレクトリが存在する場合は次の工程に進みます。
                if(is_directory(buffer))
                {
                    // 下位に jre フォルダーがあるか確認します。
                    if(buffer[wcslen(buffer) - 1] == L'\\')
                    {
                        wcscat_s(buffer, BUFFER_SIZE, L"jre");
                    }
                    else
                    {
                        wcscat_s(buffer, BUFFER_SIZE, L"\\jre");
                    }
                    // 下位に jre フォルダーがなければ元のフォルダーに戻します。
                    if(!is_directory(buffer))
                    {
                        *(wcsrchr(buffer, L'\\')) = L'\0';
                    }
                    if(is_directory(buffer))
                    {
                        if(find_java_vm(jvmpath, buffer, use_server_vm))
                        {
                            int bits = get_java_vm_bits(jvmpath);
                            if(bits == get_process_architecture())
                            {
                                if(buffer[wcslen(buffer) - 1] != L'\\')
                                {
                                    wcscat_s(buffer, BUFFER_SIZE, L"\\");
                                }
                                SetEnvironmentVariable(L"JAVA_HOME", buffer);
                                wcscpy_s(binpath, MAX_LONG_PATH, buffer);
                                wcscat_s(binpath, MAX_LONG_PATH, L"bin");
                                break; // JavaVM found!!
                            }
                            else
                            {
                                jvmpath[0] = L'\0';
                            }
                        }
                    }
                }
            }
            i++;
        }
	}

	// Find java.exe from PATH environment
	if((vm_search_locations & VM_SEARCH_PATHENV) && jvmpath[0] == L'\0')
	{
		if(GetEnvironmentVariable(L"PATH", buffer, BUFFER_SIZE))
		{
			wchar_t* p;
			wchar_t* end;

			if(buffer[wcslen(buffer) - 1] != L';')
			{
				wcscat_s(buffer, BUFFER_SIZE, L";");
			}
			end = buffer + wcslen(buffer);

			while((p = wcsrchr(buffer, L';')) != NULL)
			{
				*p = L'\0';
			}
			p = buffer;
			while(p < end)
			{
				size_t len = wcslen(p);

				if(len > 0)
				{
					wcscpy_s(search, MAX_LONG_PATH, p);
					if(search[wcslen(search) - 1] != L'\\')
					{
						wcscat_s(search, MAX_LONG_PATH, L"\\");
					}
					wcscat_s(search, MAX_LONG_PATH, L"java.exe");
					if(GetFileAttributes(search) != INVALID_FILE_ATTRIBUTES)
					{
						wchar_t* p2;

						if((p2 = wcsrchr(search, L'\\')) != NULL)
						{
							*p2 = L'\0'; // remove filename(java.exe)
						}
						if((p2 = wcsrchr(search, L'\\')) != NULL)
						{
							*p2 = L'\0'; // remove dirname(bin)
						}
						if(find_java_vm(jvmpath, search, use_server_vm))
						{
							int bits = get_java_vm_bits(jvmpath);
							if(bits == get_process_architecture())
							{
								if(search[wcslen(search) - 1] != L'\\')
								{
									wcscat_s(search, MAX_LONG_PATH, L"\\");
								}
								SetEnvironmentVariable(L"JAVA_HOME", search);
								wcscpy_s(binpath, MAX_LONG_PATH, search);
								wcscat_s(binpath, MAX_LONG_PATH, L"bin");
								break;
							}
							else
							{
								jvmpath[0] = L'\0';
							}
						}
						//JDK8以前はjdk-8u152/jre/bin/server/jvm.dll のようにjdkの中にjreがありました。これも確認します。
						wcscat_s(search, MAX_LONG_PATH, L"\\jre");
						if(find_java_vm(jvmpath, search, use_server_vm))
						{
							int bits = get_java_vm_bits(jvmpath);
							if(bits == get_process_architecture())
							{
								if(search[wcslen(search) - 1] != L'\\')
								{
									wcscat_s(search, BUFFER_SIZE, L"\\");
								}
								SetEnvironmentVariable(L"JAVA_HOME", search);
								wcscpy_s(binpath, MAX_LONG_PATH, search);
								wcscat_s(binpath, MAX_LONG_PATH, L"bin");
								break;
							}
							else
							{
								jvmpath[0] = L'\0';
							}
						}
					}
				}
				p += len + 1;
			}
		}
	}

	// Find .jar association from registry
	if((vm_search_locations & VM_SEARCH_JARASSOC) && jvmpath[0] == L'\0')
	{
		HKEY   key1 = NULL;
		HKEY   key2 = NULL;
		DWORD  size;

		wcscpy_s(search, MAX_LONG_PATH, L".jar");
		if(RegOpenKeyEx(HKEY_CLASSES_ROOT, search, 0, KEY_READ, &key1) == ERROR_SUCCESS)
		{
			size = BUFFER_SIZE;
			memset(buffer, 0x00, BUFFER_SIZE * sizeof(wchar_t));
			if(RegQueryValueEx(key1, NULL, NULL, NULL, (LPBYTE)buffer, &size) == ERROR_SUCCESS)
			{
				wcscpy_s(search, MAX_LONG_PATH, buffer);
				wcscat_s(search, MAX_LONG_PATH, L"\\shell\\open\\command");
				if(RegOpenKeyEx(HKEY_CLASSES_ROOT, search, 0, KEY_READ, &key2) == ERROR_SUCCESS)
				{
					size = BUFFER_SIZE;
					memset(buffer, 0x00, BUFFER_SIZE * sizeof(wchar_t));
					if(RegQueryValueEx(key2, NULL, NULL, NULL, (LPBYTE)buffer, &size) == ERROR_SUCCESS)
					{
						wchar_t* file = NULL;
						wchar_t* p;
						while((p = wcsrchr(buffer, L'.')) != NULL)
						{
							if(wcslen(p) >= 4)
							{
								*(p + 4) = L'\0';
							}
							if(_wcsicmp(p, L".EXE") == 0)
							{
								file = buffer + ((buffer[0] == L'"') ? 1 : 0);
								break;
							}
							*p = '\0';
						}
						if(file != NULL)
						{
							if((p = wcsrchr(file, L'\\')) != NULL)
							{
								*p = L'\0'; // remove filename(javaw.exe)
							}
							if((p = wcsrchr(file, L'\\')) != NULL)
							{
								*p = L'\0'; // remove dirname(bin)
							}
							// 下位に jre フォルダーがあるか確認します。
							wcscat_s(file, BUFFER_SIZE - 1, L"\\jre");
							// 下位に jre フォルダーがなければ元のフォルダーに戻します。
							if(!is_directory(file))
							{
								*(wcsrchr(file, L'\\')) = L'\0';
							}
							if(is_directory(file))
							{
								if(find_java_vm(jvmpath, file, use_server_vm))
								{
									int bits = get_java_vm_bits(jvmpath);
									if(bits == get_process_architecture())
									{
										if(file[wcslen(file) - 1] != L'\\')
										{
											wcscat_s(file, BUFFER_SIZE - 1, L"\\");
										}
										SetEnvironmentVariable(L"JAVA_HOME", file);
										wcscpy_s(binpath, MAX_LONG_PATH, file);
										wcscat_s(binpath, MAX_LONG_PATH, L"bin");
									}
									else
									{
										jvmpath[0] = L'\0';
									}
								}
							}
						}
					}
				}
			}
		}
		if(key2 != NULL)
		{
			RegCloseKey(key2);
		}
		if(key1 != NULL)
		{
			RegCloseKey(key1);
		}
	}


	GetModuleFileName(NULL, buffer, BUFFER_SIZE);
	wcscpy_s(libpath, BUFFER_SIZE, L".;");
	wcscat_s(libpath, BUFFER_SIZE, jvmpath);
	wcscat_s(libpath, BUFFER_SIZE, L";");
	wcscat_s(libpath, BUFFER_SIZE, binpath);
	wcscat_s(libpath, BUFFER_SIZE, L";");

	if(relative_classpath != NULL)
	{
		wchar_t* p    = buffer;
		wchar_t* path = search;

		wcscpy_s(p, BUFFER_SIZE, relative_classpath);
		while((token = wcstok_s(p, L" ", &context)) != NULL)
		{
			token = urldecode(search, MAX_LONG_PATH, token);
			if(token != NULL)
			{
			    wcscpy_s(path, BUFFER_SIZE, L"");
				if(wcsstr(token, L":") == NULL) // パスに : を含んでいない場合は相対パスと見なしてmodule_pathを付加します。
				{
					wcscat_s(path, BUFFER_SIZE, module_path);
					wcscat_s(path, BUFFER_SIZE, L"\\");
				}
				wcscat_s(path, BUFFER_SIZE, token);
				if(is_directory(path))
				{
					add_path_env(path);
					add_dll_directory(path);
				}
			}
			p = NULL;
		}
	}

	if(relative_extdirs != NULL)
	{
		wchar_t* extdirs = buffer;
		wchar_t* extdir  = search;

		wcscpy_s(extdirs, BUFFER_SIZE, relative_extdirs);
		while((token = wcstok_s(extdirs, L";", &context)) != NULL)
		{
			extdirs = NULL;

			if(wcslen(token) == 0)
			{
				continue;
			}

			extdir[0] = L'\0';
			if(wcsstr(token, L":") == NULL) // パスに : を含んでいない場合は相対パスと見なしてmodule_pathを付加します。
			{
				wcscpy_s(extdir, MAX_LONG_PATH, module_path);
				wcscat_s(extdir, MAX_LONG_PATH, L"\\");
			}
			wcscat_s(extdir, MAX_LONG_PATH, token);
			if(is_directory(extdir))
			{
				wchar_t* dirs = get_sub_dirs(extdir);
				if(dirs != NULL)
				{
					wchar_t* dir = dirs;
					while(*dir)
					{
						add_path_env(dir);
						add_dll_directory(dir);

						dir += wcslen(dir) + 1;
					}
					free(dirs);
				}
			}
		}
	}

	if(GetEnvironmentVariable(L"PATH", buffer, BUFFER_SIZE))
	{
		wcscat_s(libpath, BUFFER_SIZE, buffer);
	}

	add_path_env(binpath);
	add_dll_directory(binpath);
	
	add_path_env(jvmpath);
	add_dll_directory(jvmpath);

	path_initialized = TRUE;

EXIT:
	if(module_path != NULL)
	{
		free(module_path);
	}
	if(jdk != NULL)
	{
		free(jdk);
	}
	if(search != NULL)
	{
		free(search);
	}
	if(buffer != NULL)
	{
		free(buffer);
	}
	return path_initialized;
}


/* 指定したJRE BINディレクトリで client\jvm.dll または server\jvm.dll を検索します。
 * jvm.dll の見つかったディレクトリを output に格納します。
 * jvm.dll が見つからなかった場合は output に client のパスを格納します。
 * use_server_vm が TRUE の場合、Server VM を優先検索します。
 * jvm.dll が見つかった場合は TRUE, 見つからなかった場合は FALSE を返します。
 */
BOOL find_java_vm(wchar_t* output, const wchar_t* jre, BOOL use_server_vm)
{
	BOOL     found = FALSE;
	wchar_t* path = NULL;
	wchar_t* buf  = NULL;

	path = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(path == NULL)
	{
		goto EXIT;
	}

	buf = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(buf == NULL)
	{
		goto EXIT;
	}

	if(use_server_vm)
	{
		wcscpy_s(path, MAX_LONG_PATH, jre);
		wcscat_s(path, MAX_LONG_PATH, L"\\bin\\server");
		wcscpy_s(buf, MAX_LONG_PATH, path);
		wcscat_s(buf, MAX_LONG_PATH, L"\\jvm.dll");
		if(GetFileAttributes(buf) == INVALID_FILE_ATTRIBUTES)
		{
			wcscpy_s(path, MAX_LONG_PATH, jre);
			wcscat_s(path, MAX_LONG_PATH, L"\\bin\\client");
			wcscpy_s(buf, MAX_LONG_PATH, path);
			wcscat_s(buf, MAX_LONG_PATH, L"\\jvm.dll");
			if(GetFileAttributes(buf) == INVALID_FILE_ATTRIBUTES)
			{
				path[0] = L'\0';
			}
		}
	}
	else
	{
		wcscpy_s(path, MAX_LONG_PATH, jre);
		wcscat_s(path, MAX_LONG_PATH, L"\\bin\\client");
		wcscpy_s(buf, MAX_LONG_PATH, path);
		wcscat_s(buf, MAX_LONG_PATH, L"\\jvm.dll");
		if(GetFileAttributes(buf) == INVALID_FILE_ATTRIBUTES)
		{
			wcscpy_s(path, MAX_LONG_PATH, jre);
			wcscat_s(path, MAX_LONG_PATH, L"\\bin\\server");
			wcscpy_s(buf, MAX_LONG_PATH, path);
			wcscat_s(buf, MAX_LONG_PATH, L"\\jvm.dll");
			if(GetFileAttributes(buf) == INVALID_FILE_ATTRIBUTES)
			{
				path[0] = L'\0';
			}
		}
	}
	if(path[0] != L'\0')
	{
		wcscpy_s(output, MAX_LONG_PATH, path);
		found = TRUE;
	}

EXIT:
	if(buf != NULL)
	{
		free(buf);
	}
	if(path != NULL)
	{
		free(path);
	}
	return found;
}


wchar_t* find_java_home_from_registry(const wchar_t* _subkey, wchar_t* output)
{
	HKEY     key    = NULL;
	DWORD    size;
	wchar_t* subkey = NULL;
	wchar_t* buf    = NULL;

	if(output != NULL)
	{
		output[0] = L'\0';
	}

	subkey = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(subkey == NULL)
	{
		goto EXIT;
	}
	wcscpy_s(subkey, MAX_LONG_PATH, _subkey);

	buf = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(buf == NULL)
	{
		goto EXIT;
	}

	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ, &key) != ERROR_SUCCESS)
	{
		goto EXIT;
	}
	size = MAX_LONG_PATH;
	memset(buf, 0x00, MAX_LONG_PATH * sizeof(wchar_t));
	if(RegQueryValueEx(key, L"CurrentVersion", NULL, NULL, (LPBYTE)buf, &size) != ERROR_SUCCESS)
	{
		goto EXIT;
	}
	RegCloseKey(key);
	key = NULL;

	wcscat_s(subkey, MAX_LONG_PATH, L"\\");
	wcscat_s(subkey, MAX_LONG_PATH, buf);

	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ, &key) != ERROR_SUCCESS)
	{
		goto EXIT;
	}

	size = MAX_LONG_PATH;
	memset(buf, 0x00, MAX_LONG_PATH * sizeof(wchar_t));
	if(RegQueryValueEx(key, L"JavaHome", NULL, NULL, (LPBYTE)buf, &size) != ERROR_SUCCESS)
	{
		goto EXIT;
	}
	RegCloseKey(key);
	key = NULL;

	wcscpy_s(output, MAX_LONG_PATH, buf);

EXIT:
	if(key != NULL)
	{
		RegCloseKey(key);
	}
	if(buf != NULL)
	{
		free(buf);
	}
	if(subkey != NULL)
	{
		free(subkey);
	}

	return (output[0] != L'\0' ? output : NULL);
}

static BOOL add_path_env(const wchar_t* path)
{
	BOOL succeeded = FALSE;
	wchar_t* buf = NULL;
	wchar_t* old = NULL;

	if(path == NULL)
	{
		goto EXIT;
	}
	buf = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(buf == NULL)
	{
		goto EXIT;
	}
	old = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(old == NULL)
	{
		goto EXIT;
	}

	GetEnvironmentVariable(L"PATH", old, BUFFER_SIZE);
	wcscpy_s(buf, MAX_LONG_PATH, path);
	wcscat_s(buf, MAX_LONG_PATH, L";");
	wcscat_s(buf, MAX_LONG_PATH, old);
	SetEnvironmentVariable(L"PATH", buf);
	succeeded = TRUE;

EXIT:
	if(old != NULL)
	{
		free(old);
	}
	if(buf != NULL)
	{
		free(buf);
	}
	return succeeded;
}


static void add_dll_directory(const wchar_t* path)
{
	if(_AddDllDirectory != NULL)
	{
		_AddDllDirectory(path);
	}
}


wchar_t* get_module_version(wchar_t* buf, size_t size)
{
	HRSRC hrsrc;
	VS_FIXEDFILEINFO* verInfo;

	hrsrc = FindResource(NULL, MAKEINTRESOURCE(VS_VERSION_INFO), RT_VERSION);
	if(hrsrc == NULL)
	{
		*buf = L'\0';
		return NULL;
	}

	verInfo = (VS_FIXEDFILEINFO*)((char*)LockResource(LoadResource(NULL, hrsrc)) + 40);
	swprintf_s(buf, size, L"%d.%d.%d.%d",
		verInfo->dwFileVersionMS >> 16,
		verInfo->dwFileVersionMS & 0xFFFF,
		verInfo->dwFileVersionLS >> 16,
		verInfo->dwFileVersionLS & 0xFFFF
	);

	return buf;
}


static BOOL is_directory(const wchar_t* path)
{
	DWORD attr = GetFileAttributes(path);
	return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
}


static wchar_t* get_sub_dirs(const wchar_t* dir)
{
	size_t   size;
	wchar_t* buf;

	size = wcslen(dir) + 2;
	if(add_sub_dirs(NULL, 0, dir, &size) == FALSE)
	{
		return NULL;
	}
	buf = (wchar_t*)calloc(size, sizeof(wchar_t));
	if(buf == NULL)
	{
		return NULL;
	}
	wcscpy_s(buf, size, dir);
	if(add_sub_dirs(buf + wcslen(dir) + 1, size - wcslen(dir) - 1, dir, NULL) == FALSE)
	{
		free(buf);
		return NULL;
	}
	return buf;
}


static BOOL add_sub_dirs(wchar_t* buf, size_t buf_size, const wchar_t* dir, size_t* size)
{
	BOOL succeeded = FALSE;
	WIN32_FIND_DATA fd;
	HANDLE hSearch;
	wchar_t* search = NULL;
	wchar_t* child = NULL;

	search = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(search == NULL)
	{
		goto EXIT;
	}
	child = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(child == NULL)
	{
		goto EXIT;
	}

	wcscpy_s(search, MAX_LONG_PATH, dir);
	wcscat_s(search, MAX_LONG_PATH, L"\\*");

	hSearch = FindFirstFile(search, &fd);
	if(hSearch != INVALID_HANDLE_VALUE)
	{
		do
		{
			if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
				{
					continue;
				}

				wcscpy_s(child, MAX_LONG_PATH, dir);
				wcscat_s(child, MAX_LONG_PATH, L"\\");
				wcscat_s(child, MAX_LONG_PATH, fd.cFileName);
				if(size != NULL)
				{
					*size = *size + wcslen(child) + 1;
				}
				if(buf != NULL)
				{
					wcscpy_s(buf, buf_size, child);
					buf += wcslen(child) + 1;
					buf_size -= wcslen(child) + 1;
				}
				add_sub_dirs(buf, buf_size, child, size);
			}
		} while (FindNextFile(hSearch, &fd));
		FindClose(hSearch);
	}
	succeeded = TRUE;

EXIT:
	if(child != NULL)
	{
		free(child);
	}
	if(search != NULL)
	{
		free(search);
	}

	return succeeded;
}


static wchar_t* get_jars(const wchar_t* dir)
{
	size_t   size;
	wchar_t* buf;

	size = wcslen(dir) + 2;
	if(add_jars(NULL, 0, dir, &size) == FALSE)
	{
		return NULL;
	}
	buf = (wchar_t*)calloc(size, sizeof(wchar_t));
	if(buf == NULL)
	{
		return NULL;
	}
	if(add_jars(buf, size, dir, NULL) == FALSE)
	{
		free(buf);
		return NULL;
	}
	return buf;
}


static BOOL add_jars(wchar_t* buf, size_t buf_size, const wchar_t* dir, size_t* size)
{
	BOOL succeeded = FALSE;
	WIN32_FIND_DATA fd;
	HANDLE hSearch;
	wchar_t* search = NULL;
	wchar_t* child = NULL;

	search = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(search == NULL)
	{
		goto EXIT;
	}
	child = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(child == NULL)
	{
		goto EXIT;
	}

	wcscpy_s(search, MAX_LONG_PATH, dir);
	wcscat_s(search, MAX_LONG_PATH, L"\\*");
	
	hSearch = FindFirstFile(search, &fd);
	if(hSearch != INVALID_HANDLE_VALUE)
	{
		do
		{
			if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
				{
					continue;
				}

				wcscpy_s(child, MAX_LONG_PATH, dir);
				wcscat_s(child, MAX_LONG_PATH, L"\\");
				wcscat_s(child, MAX_LONG_PATH, fd.cFileName);
				add_jars(buf, buf_size, child, size);
			}
			else
			{
				size_t len = wcslen(fd.cFileName);
				if(len >= 4)
				{
					wchar_t* ext = fd.cFileName + len - 4;
					if(_wcsicmp(ext, L".JAR") == 0)
					{
						wcscpy_s(child, MAX_LONG_PATH, dir);
						wcscat_s(child, MAX_LONG_PATH, L"\\");
						wcscat_s(child, MAX_LONG_PATH, fd.cFileName);
						if(size != NULL)
						{
							*size = *size + wcslen(child) + 1;
						}
						if(buf != NULL)
						{
							wcscpy_s(buf, buf_size, child);
							buf += wcslen(child) + 1;
							buf_size -= wcslen(child) + 1;
						}
					}
				}
			}
		} while (FindNextFile(hSearch, &fd));
		FindClose(hSearch);
	}
	succeeded = TRUE;

EXIT:
	if(child != NULL)
	{
		free(child);
	}
	if(search != NULL)
	{
		free(search);
	}
	return succeeded;
}


int get_java_vm_bits(const wchar_t* jvmpath)
{
	int bits = 0;
	wchar_t* file = NULL;
	HANDLE hFile = NULL;
	BYTE*  buf = NULL;
	DWORD size = 512;
	DWORD read_size;
	UINT  i;

	file = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
	if(file == NULL)
	{
		goto EXIT;
	}
	swprintf_s(file, MAX_LONG_PATH, L"%s\\jvm.dll", jvmpath);

	buf = (BYTE*)malloc(size);
	if(buf == NULL)
	{
		goto EXIT;
	}

	hFile = CreateFile(file, GENERIC_READ, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile == INVALID_HANDLE_VALUE)
	{
		goto EXIT;
	}

	if(ReadFile(hFile, buf, size, &read_size, NULL) == 0)
	{
		goto EXIT;
	}

	for(i = 0; i < size - 6; i++)
	{
		if(buf[i + 0] == 'P' && buf[i + 1] == 'E' && buf[i + 2] == 0x00 && buf[i + 3] == 0x00)
		{
			if (buf[i + 4] == 0x4C && buf[i + 5] == 0x01)
			{
				bits = 32;
				goto EXIT;
			}
			if (buf[i + 4] == 0x64 && buf[i + 5] == 0x86)
			{
				bits = 64;
				goto EXIT;
			}
		}
	}

EXIT:
	if(hFile != NULL)
	{
		CloseHandle(hFile);
	}
	if(buf != NULL)
	{
		free(buf);
	}
	if(file != NULL)
	{
		free(file);
	}
	return bits;
}


static wchar_t* urldecode(wchar_t *dst, size_t dst_size, const wchar_t *src)
{
	wchar_t* result   = NULL;
	char*    src_utf8 = NULL;
	char*    dst_utf8 = NULL;
	char*    src_ptr;
	char*    dst_ptr;
	char     a;
	char     b;
	wchar_t* buf = NULL;

	src_ptr = src_utf8 = to_utf8(src);
	if(src_utf8 == NULL)
	{
		goto EXIT;
	}
	dst_ptr = dst_utf8 = (char*)malloc(strlen(src_utf8) + 1);
	if(dst_utf8 == NULL)
	{
		goto EXIT;
	}

	while(*src_ptr)
	{
		if(*src_ptr == '%' && (a = src_ptr[1]) != '\0' && (b = src_ptr[2]) != '\0' && isxdigit(a) && isxdigit(b))
		{
			if(a >= 'a')
			{
				a -= 'a'-'A';
			}
			
			if(a >= 'A')
			{
				a -= ('A' - 10);
			}
			else
			{
				a -= '0';
			}
			
			if(b >= 'a')
			{
				b -= 'a'-'A';
			}
			
			if(b >= 'A')
			{
				b -= ('A' - 10);
			}
			else
			{
				b -= '0';
			}
			
			*dst_ptr++ = 16 * a + b;
			src_ptr += 3;
		}
		else if(*src_ptr == '+')
		{
			*dst_ptr++ = ' ';
			src_ptr++;
		}
		else
		{
			*dst_ptr++ = *src_ptr++;
		}
	}
	*dst_ptr++ = '\0';

	buf = from_utf8(dst_utf8);
	wcscpy_s(dst, dst_size, buf);
	result = dst;

EXIT:
	if(buf != NULL)
	{
		free(buf);
	}
	if(dst_utf8 != NULL)
	{
		free(dst_utf8);
	}
	if(src_utf8 != NULL)
	{
		free(src_utf8);
	}
	return result;
}


char* to_platform_encoding(const wchar_t* str)
{
	int    mb_size;
	char*  mb_buf;

	if(str == NULL)
	{
		return NULL;
	}

	mb_size = WideCharToMultiByte(CP_ACP, 0, str, -1, NULL, 0, NULL, NULL);
	if(mb_size == 0)
	{
		return NULL;
	}
	mb_buf = (char*)malloc(mb_size);
	if(mb_buf == NULL)
	{
		return NULL;
	}
	if(WideCharToMultiByte(CP_ACP, 0, str, -1, mb_buf, mb_size, NULL, NULL) == 0)
	{
		free(mb_buf);
		return NULL;
	}
	return mb_buf;
}


char* to_utf8(const wchar_t* str)
{
	int    mb_size;
	char*  mb_buf;

	if(str == NULL)
	{
		return NULL;
	}

	mb_size = WideCharToMultiByte(CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL);
	if(mb_size == 0)
	{
		return NULL;
	}
	mb_buf = (char*)malloc(mb_size);
	if(mb_buf == NULL)
	{
		return NULL;
	}
	if(WideCharToMultiByte(CP_UTF8, 0, str, -1, mb_buf, mb_size, NULL, NULL) == 0)
	{
		free(mb_buf);
		return NULL;
	}
	return mb_buf;
}


wchar_t* from_utf8(const char* utf8)
{
	int      wc_size;
	wchar_t* wc_buf;

	if(utf8 == NULL)
	{
		return NULL;
	}

	wc_size = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
	if(wc_size == 0)
	{
		return NULL;
	}
	wc_buf = (wchar_t*)malloc(wc_size * sizeof(wchar_t));
	if(wc_buf == NULL)
	{
		return NULL;
	}
	if(MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wc_buf, wc_size) == 0)
	{
		free(wc_buf);
		return NULL;
	}
	return wc_buf;
}


jstring to_jstring(JNIEnv* env, const wchar_t* str)
{
	jstring jstr;

	if(str == NULL)
	{
		return NULL;
	}
	jstr = (*env)->NewString(env, (jchar*)str, (jsize)wcslen(str));
	return jstr;
}


wchar_t* from_jstring(JNIEnv* env, jstring jstr)
{
	const jchar* unicode;
	size_t       wc_size;
	wchar_t*     wc_buf;

	if(jstr == NULL)
	{
		return NULL;
	}
	unicode = (*env)->GetStringChars(env, jstr, NULL);
	if(unicode == NULL)
	{
		return NULL;
	}
	wc_size = wcslen(unicode) + 1;
	wc_buf = malloc(wc_size * sizeof(wchar_t));
	if(wc_buf != NULL)
	{
		wcscpy_s(wc_buf, wc_size, unicode);
	}
	(*env)->ReleaseStringChars(env, jstr, unicode);
	return wc_buf;
}
