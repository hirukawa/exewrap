/* このファイルの文字コードは Shift_JIS (MS932) です。*/

#include <windows.h>
#include <shlobj.h>
#include <jni.h>

#include "include/jvm.h"
#include "include/loader.h"
#include "include/icon.h"
#include "include/message.h"
#include "include/wcout.h"

#define BUFFER_SIZE             32768
#define MAX_LONG_PATH           32768
#define ERROR_UNKNOWN           ((DWORD)STG_E_UNKNOWN)
#define DEFAULT_VERSION         L"0.0"
#define DEFAULT_PRODUCT_VERSION L"0.0"

static void      cleanup(void);
static void      exit_process(DWORD last_error, const wchar_t* append);
static wchar_t** parse_opt(int argc, const wchar_t* argv[]);
static DWORD     get_version_revision(const wchar_t* filename);
static void      create_exe_file(const wchar_t* filename, BYTE* image_buf, DWORD image_len, BOOL is_reverse);
static void      append_exe_file(const wchar_t* filename, BYTE* image_buf, DWORD image_len);
static void      get_target_java_runtime_version(const wchar_t* version, DWORD* major, DWORD* minor, DWORD* build, DWORD* revision);
static void      set_resource(const wchar_t* filename, const wchar_t* rsc_name, const wchar_t* rsc_type, BYTE* rsc_data, DWORD rsc_size);
static BYTE*     get_jar_buf(const wchar_t* jar_file, DWORD* jar_len);
static BOOL      set_application_icon_file(const wchar_t* filename, const wchar_t* icon_file);
static BOOL      set_application_icon(const wchar_t* filename, const BYTE* icon_bytes);
static wchar_t*  set_version_info(const wchar_t* filename, const wchar_t* version_number, DWORD previous_revision, const wchar_t* file_description, const wchar_t* copyright, const wchar_t* company_name, const wchar_t* product_name, const wchar_t* product_version, const wchar_t* original_filename, const wchar_t* jar_file);
static wchar_t*  get_exewrap_version_string(void);

static BOOL      g_is_dirty_exe = FALSE;
static wchar_t*  g_exe_file = NULL;

int wmain(int argc, wchar_t* argv[])
{
	SYSTEMTIME startup;
	wchar_t**  opt      = NULL;
	wchar_t*   buf      = NULL;
	wchar_t*   jar_file = NULL;
	wchar_t*   exe_file = NULL;
	int        architecture_bits = 0;
	wchar_t    image_name[32];
	wchar_t    manifest_name[32];
	RESOURCE   res;
	BYTE*      image_buf;
	DWORD      image_len;
	DWORD      previous_revision = 0;
	DWORD      target_version_major;
	DWORD      target_version_minor;
	DWORD      target_version_build;
	DWORD      target_version_revision;
	wchar_t*   target_version_string;
	char*      utf8;
	BOOL       is_java_enabled = FALSE;
	BYTE*      jar_buf;
	DWORD      jar_len;
	wchar_t*   ext_flags = NULL;
	wchar_t*   vmargs = NULL;
	wchar_t*   version_number;
	wchar_t*   file_description;
	wchar_t*   copyright;
	wchar_t*   company_name;
	wchar_t*   product_name;
	wchar_t*   product_version;
	wchar_t*   original_filename;
	wchar_t*   new_version;
	BOOL       contains_visualvm_display_name = FALSE;
	BOOL       is_icon_set = FALSE;

	GetSystemTime(&startup);
	
	opt = parse_opt(argc, argv);
	if(opt == NULL)
	{
		exit_process(ERROR_NOT_ENOUGH_MEMORY, L"parse_opt");
	}

	if((argc < 2) || ((opt['j'] == NULL) && (opt[0] == NULL)))
	{
		wchar_t* exewrap_version_string = get_exewrap_version_string();
		int bits = get_process_architecture();

		if(wcsrchr(argv[0], L'\\') > 0)
		{
			exe_file = wcsrchr(argv[0], L'\\') + 1;
		}
		else
		{
			exe_file = argv[0];
		}
				
		wcoutf( L"exewrap %ls for %ls (%d-bit) \r\n"
				L"Native executable java application wrapper.\r\n"
				L"Copyright (C) 2005-2020 HIRUKAWA Ryo. All rights reserved.\r\n"
				L"\r\n"
				L"Usage: %ls <options> <jar-file>\r\n"
				L"Options:\r\n"
				L"  -g                  \t Create Window application.\r\n"
				L"  -s                  \t Create Windows Service application.\r\n"
				L"  -A <architecture>   \t Select exe-file architecture. (default %ls)\r\n"
				L"  -t <version>        \t Set target java runtime version. (default 1.5)\r\n"
				L"  -M <main-class>     \t Set main-class.\r\n"
				L"  -L <ext-dirs>       \t Set ext-dirs.\r\n"
				L"  -e <ext-flags>      \t Set extended flags.\r\n"
				L"  -a <vm-args>        \t Set Java VM arguments.\r\n"
				L"  -b <vm-args>        \t This arguments will be applied when the service was started without the SCM.\r\n"
				L"  -i <icon-file>      \t Set application icon.\r\n"
				L"  -P <privilege>      \t Set privilege. (asInvoker | highestAvailable | requireAdministrator)\r\n"
				L"  -v <version>        \t Set version number.\r\n"
				L"  -d <description>    \t Set file description.\r\n"
				L"  -c <copyright>      \t Set copyright.\r\n"
				L"  -C <company-name>   \t Set company name.\r\n"
				L"  -p <product-name>   \t Set product name.\r\n"
				L"  -V <product-version>\t Set product version.\r\n"
				L"  -j <jar-file>       \t Input jar-file.\r\n"
				L"  -o <exe-file>       \t Output exe-file.\r\n"
			, exewrap_version_string, (bits == 64 ? L"x64" : L"x86"), bits, exe_file, (bits == 64 ? L"x64" : L"x86"));

		return NO_ERROR;
	}
	else
	{
		wchar_t* exewrap_version_string = get_exewrap_version_string();
		int bits = get_process_architecture();

		wcoutf(L"exewrap %ls for %ls (%d-bit) \r\n", exewrap_version_string, (bits == 64 ? L"x64" : L"x86"), bits);
	}

	buf = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(buf == NULL)
	{
		exit_process(ERROR_NOT_ENOUGH_MEMORY, L"malloc");
	}
	
	jar_file = (wchar_t*)calloc(MAX_LONG_PATH, sizeof(wchar_t));
	if(jar_file == NULL)
	{
		exit_process(ERROR_NOT_ENOUGH_MEMORY, L"malloc");
	}

	exe_file = (wchar_t*)calloc(MAX_LONG_PATH, sizeof(wchar_t));
	if(exe_file == NULL)
	{
		exit_process(ERROR_NOT_ENOUGH_MEMORY, L"malloc");
	}

	if(opt['j'] != NULL && *opt['j'] != L'-' && *opt['j'] != L'\0')
	{
		wcscpy_s(jar_file, MAX_LONG_PATH, opt['j']);
	}
	else
	{
		wcscpy_s(jar_file, MAX_LONG_PATH, opt[0]);
	}
	if(wcslen(jar_file) > 4)
	{
		wcscpy_s(buf, MAX_LONG_PATH, jar_file + wcslen(jar_file) - 4);
		if(_wcsicmp(buf, L".JAR"))
		{
			exit_process(ERROR_BAD_PATHNAME, jar_file);
		}
	}
	
	if(opt['o'] && *opt['o'] != L'-' && *opt['o'] != L'\0')
	{
		wcscpy_s(exe_file, MAX_LONG_PATH, opt['o']);
	}
	else
	{
		wcscpy_s(exe_file, MAX_LONG_PATH, jar_file);
		exe_file[wcslen(exe_file) - 4] = L'\0';
		wcscat_s(exe_file, MAX_LONG_PATH, L".exe");
	}

	if(GetFullPathName(exe_file, MAX_LONG_PATH, buf, NULL) == 0)
	{
		exit_process(ERROR_BAD_PATHNAME, exe_file);
	}
	wcscpy_s(exe_file, MAX_LONG_PATH, buf);

	if(opt['A'])
	{
		if(wcsstr(opt['A'], L"86") != NULL)
		{
			architecture_bits = 32;
		}
		else if(wcsstr(opt['A'], L"64") != NULL)
		{
			architecture_bits = 64;
		}
	}
	if(architecture_bits == 0)
	{
		architecture_bits = get_process_architecture();
	}
	wcoutf(L"Architecture: %ls\r\n", (architecture_bits == 64 ? L"x64 (64-bit)" : L"x86 (32-bit)"));

	if(opt['g'])
	{
		swprintf_s(image_name, 32, L"IMAGE_GUI_%d", architecture_bits);
	}
	else if(opt['s'])
	{
		swprintf_s(image_name, 32, L"IMAGE_SERVICE_%d", architecture_bits);
	}
	else
	{
		swprintf_s(image_name, 32, L"IMAGE_CONSOLE_%d", architecture_bits);
	}

	if(opt['P'])
	{
		if(_wcsicmp(opt['P'], L"asInvoker") == 0)
		{
			swprintf_s(manifest_name, 32, L"%ls%ls", image_name, L"_MF_INVOKER");
			wcout(L"Privilege: asInvoker\r\n");
		}
		else if(_wcsicmp(opt['P'], L"highestAvailable") == 0)
		{
			swprintf_s(manifest_name, 32, L"%ls%ls", image_name, L"_MF_HIGHEST");
			wcout(L"Privilege: highestAvailable\n\n");
		}
		else if(_wcsicmp(opt['P'], L"requireAdministrator") == 0)
		{
			swprintf_s(manifest_name, 32, L"%ls%ls", image_name, L"_MF_ADMIN");
			wcout(L"Privilege: requireAdministrator\r\n");
		}
		else
		{
			exit_process(ERROR_BAD_ARGUMENTS, opt['P']);
		}
	} else {
		swprintf_s(manifest_name, 32, L"%ls%ls", image_name, L"_MF_DEFAULT");
	}
	
	if(get_resource(image_name, &res) == NULL)
	{
		exit_process(GetLastError(), image_name);
	}
	image_buf = res.buf;
	image_len = res.len;
	
	previous_revision = get_version_revision(exe_file);
	DeleteFile(exe_file);

	create_exe_file(exe_file, image_buf, image_len, TRUE);


	if(get_resource(manifest_name, &res) == NULL)
	{
		exit_process(GetLastError(), manifest_name);
	}
	set_resource(exe_file, CREATEPROCESS_MANIFEST_RESOURCE_ID, RT_MANIFEST, res.buf, res.len);

	if(opt['t'])
	{
		get_target_java_runtime_version(opt['t'], &target_version_major, &target_version_minor, &target_version_build, &target_version_revision);
	}
	else
	{
		// default target version 1.5
		get_target_java_runtime_version(L"1.5", &target_version_major, &target_version_minor, &target_version_build, &target_version_revision);
	}
	target_version_string = get_java_version_string(target_version_major, target_version_minor, target_version_build, target_version_revision);
	wcoutf(L"Target: %ls (%d.%d.%d.%d)\r\n", target_version_string, target_version_major, target_version_minor, target_version_build, target_version_revision);
	swprintf_s(buf, BUFFER_SIZE, L"%d.%d.%d.%d", target_version_major, target_version_minor, target_version_build, target_version_revision);
	utf8 = to_utf8(buf);
	set_resource(exe_file, L"TARGET_VERSION", RT_RCDATA, (BYTE*)utf8, (DWORD)(strlen(utf8) + 1));
	free(utf8);
	free(target_version_string);
	
	if(opt['L'] != NULL && *opt['L'] != L'-' && *opt['L'] != L'\0')
	{
		if(wcscmp(opt['L'], L";") == 0)
		{
			// セミコロンのみが指定されている場合はEXTDIRSリソース自体を設定しません。
		}
		else
		{
			utf8 = to_utf8(opt['L']);
			set_resource(exe_file, L"EXTDIRS", RT_RCDATA, (BYTE*)utf8, (DWORD)(strlen(utf8) + 1));
			free(utf8);
		}
	}
	else
	{
		// 引数 -L が指定されていない場合は既定のディレクトリ lib を EXTDIRS に設定します。
		utf8 = to_utf8(L"lib");
		set_resource(exe_file, L"EXTDIRS", RT_RCDATA, (BYTE*)utf8, (DWORD)(strlen(utf8) + 1));
		free(utf8);
	}

	is_java_enabled = create_java_vm(NULL, FALSE, TRUE, NULL, NULL) != NULL;
	if(is_java_enabled)
	{
		LOAD_RESULT result;
		jbyteArray  buf;
		jstring     jstr;
		jclass      JarProcessor;
		jmethodID   JarProcessor_init;
		jobject     jarProcessor;
		jmethodID   jarProcessor_getMainClass;
		jmethodID   jarProcessor_getClassPath;
		jmethodID   jarProcessor_getSplashScreenName;
		jmethodID   jarProcessor_getSplashScreenImage;
		jmethodID   jarProcessor_getBytes;
		wchar_t*    main_class;
		wchar_t*    class_path;
		wchar_t*    splash_screen_name;
		BYTE*       splash_screen_image;
		BYTE*       bytes;

		result.msg = (wchar_t*)malloc(LOAD_RESULT_MAX_MESSAGE_LENGTH * sizeof(wchar_t));
		if(result.msg == NULL)
		{
			exit_process(ERROR_NOT_ENOUGH_MEMORY, L"malloc");
		}

		if(load_main_class(argc, argv, NULL, &result) == FALSE)
		{
			wcerrf(L"ERROR: load_main_class: tool.jar exewrap.tool.JarProcessor: %ls\r\n", result.msg);

			//例外が発生している場合はスタックトレースを出力します。
			if((*env)->ExceptionCheck(env) == JNI_TRUE)
			{
				(*env)->ExceptionDescribe(env);
				(*env)->ExceptionClear(env);
			}

			cleanup();
			ExitProcess(ERROR_INVALID_DATA);
		}
		JarProcessor = result.MainClass;

		jar_buf = get_jar_buf(jar_file, &jar_len);
		if(jar_buf == NULL)
		{
			exit_process(ERROR_INVALID_DATA, jar_file);
		}
		buf = (*env)->NewByteArray(env, jar_len);
		if(buf == NULL)
		{
			exit_process(ERROR_NOT_ENOUGH_MEMORY, L"NewByteArray");
		}
		(*env)->SetByteArrayRegion(env, buf, 0, jar_len, (jbyte*)jar_buf);

		JarProcessor_init = (*env)->GetMethodID(env, JarProcessor, "<init>", "([BZ)V");
		if(JarProcessor_init == NULL)
		{
			wcerrf(_(MSG_ID_ERR_GET_CONSTRUCTOR), L"exewrap.tool.JarProcessor(byte[], boolean)");
			wcerr(L"\r\n");
			cleanup();
			ExitProcess(ERROR_INVALID_FUNCTION);
		}
		jarProcessor = (*env)->NewObject(env, JarProcessor, JarProcessor_init, buf, (jboolean)(opt['g'] != NULL ? JNI_TRUE : JNI_FALSE));
		if(jarProcessor == NULL)
		{
			wcerrf(_(MSG_ID_ERR_NEW_OBJECT), L"exewrap.tool.JarProcessor(byte[], boolean)");
			wcerr(L"\r\n");
			cleanup();
			ExitProcess(ERROR_INVALID_FUNCTION);
		}
		jarProcessor_getMainClass = (*env)->GetMethodID(env, JarProcessor, "getMainClass", "()Ljava/lang/String;");
		if(jarProcessor_getMainClass == NULL)
		{
			wcerrf(_(MSG_ID_ERR_GET_METHOD), L"exewrap.tool.JarProcessor.getMainClass()");
			wcerr(L"\r\n");
			cleanup();
			ExitProcess(ERROR_INVALID_FUNCTION);
		}
		jarProcessor_getClassPath = (*env)->GetMethodID(env, JarProcessor, "getClassPath", "()Ljava/lang/String;");
		if(jarProcessor_getClassPath == NULL)
		{
			wcerrf(_(MSG_ID_ERR_GET_METHOD), L"exewrap.tool.JarProcessor.getClassPath()");
			wcerr(L"\r\n");
			cleanup();
			ExitProcess(ERROR_INVALID_FUNCTION);
		}
		jarProcessor_getSplashScreenName = (*env)->GetMethodID(env, JarProcessor, "getSplashScreenName", "()Ljava/lang/String;");
		if(jarProcessor_getSplashScreenName == NULL)
		{
			wcerrf(_(MSG_ID_ERR_GET_METHOD), L"exewrap.tool.JarProcessor.getSplashScreenName()");
			wcerr(L"\r\n");
			cleanup();
			ExitProcess(ERROR_INVALID_FUNCTION);
		}
		jarProcessor_getSplashScreenImage = (*env)->GetMethodID(env, JarProcessor, "getSplashScreenImage", "()[B");
		if(jarProcessor_getSplashScreenImage == NULL)
		{
			wcerrf(_(MSG_ID_ERR_GET_METHOD), L"exewrap.tool.JarProcessor.getSplashScreenImage()");
			wcerr(L"\r\n");
			cleanup();
			ExitProcess(ERROR_INVALID_FUNCTION);
		}
		jarProcessor_getBytes = (*env)->GetMethodID(env, JarProcessor, "getBytes", "()[B");
		if(jarProcessor_getBytes == NULL)
		{
			wcerrf(_(MSG_ID_ERR_GET_METHOD), L"exewrap.tool.JarProcessor.getBytes()");
			wcerr(L"\r\n");
			cleanup();
			ExitProcess(ERROR_INVALID_FUNCTION);
		}

		if(opt['M'])
		{
			main_class = opt['M'];
		}
		else
		{
			jstr = (*env)->CallObjectMethod(env, jarProcessor, jarProcessor_getMainClass);
			main_class = from_jstring(env, jstr);
		}
		if(main_class == NULL)
		{
			wcerr(_(MSG_ID_ERR_FIND_MAIN_CLASS));
			wcerr(L"\r\n");
			cleanup();
			ExitProcess(ERROR_INVALID_DATA);
		}
		utf8 = to_utf8(main_class);
		set_resource(exe_file, L"MAIN_CLASS", RT_RCDATA, (BYTE*)utf8, (DWORD)(strlen(utf8) + 1));
		free(utf8);

		jstr = (*env)->CallObjectMethod(env, jarProcessor, jarProcessor_getClassPath);
		class_path = from_jstring(env, jstr);
		if(class_path != NULL)
		{
			utf8 = to_utf8(class_path);
			set_resource(exe_file, L"CLASS_PATH", RT_RCDATA, (BYTE*)utf8, (DWORD)(strlen(utf8) + 1));
			free(utf8);
		}

		jstr = (*env)->CallObjectMethod(env, jarProcessor, jarProcessor_getSplashScreenName);
		splash_screen_name = from_jstring(env, jstr);
		if(splash_screen_name != NULL)
		{
			utf8 = to_utf8(splash_screen_name);
			set_resource(exe_file, L"SPLASH_SCREEN_NAME", RT_RCDATA, (BYTE*)utf8, (DWORD)(strlen(utf8) + 1));
			free(utf8);
		}
		buf = (*env)->CallObjectMethod(env, jarProcessor, jarProcessor_getSplashScreenImage);
		if(buf != NULL)
		{
			jboolean isCopy = 0;
			jint len;
			splash_screen_image = (BYTE*)((*env)->GetByteArrayElements(env, buf, &isCopy));
			len = (*env)->GetArrayLength(env, buf);
			set_resource(exe_file, L"SPLASH_SCREEN_IMAGE", RT_RCDATA, splash_screen_image, (DWORD)len);
			(*env)->ReleaseByteArrayElements(env, buf, (jbyte*)splash_screen_image, 0);
		}

		buf = (*env)->CallObjectMethod(env, jarProcessor, jarProcessor_getBytes);
		if(buf != NULL)
		{
			jboolean isCopy;
			jint len;
			bytes  = (BYTE*)((*env)->GetByteArrayElements(env, buf, &isCopy));
			len = (*env)->GetArrayLength(env, buf);
			set_resource(exe_file, L"JAR", RT_RCDATA, bytes, (DWORD)len);
			(*env)->ReleaseByteArrayElements(env, buf, (jbyte*)bytes, 0);
		}
	}
	else
	{
		jar_buf = get_jar_buf(jar_file, &jar_len);
		if(jar_buf == NULL)
		{
			// get_jar_buf 内でエラーが発生した場合、そのまま exit_process が呼ばれるため、通常ここに到達することはありません。
			exit_process(ERROR_UNKNOWN, L"get_jar_buf");
		}
		set_resource(exe_file, L"JAR", RT_RCDATA, jar_buf, jar_len);
		wcerrf(L"JavaVM (%d-bit) not found.\r\n", get_process_architecture());
	}

	ext_flags = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(ext_flags == NULL)
	{
		exit_process(ERROR_NOT_ENOUGH_MEMORY, L"malloc");
	}
	ext_flags[0] = L'\0';
	if (opt['e'] != NULL && *opt['e'] != L'-' && *opt['e'] != L'\0')
	{
		wcscat_s(ext_flags, BUFFER_SIZE, opt['e']);
	}
	utf8 = to_utf8(ext_flags);
	set_resource(exe_file, L"EXTFLAGS", RT_RCDATA, (BYTE*)utf8, (DWORD)(strlen(utf8) + 1));
	free(utf8);
	free(ext_flags);

	vmargs = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(vmargs == NULL)
	{
		exit_process(ERROR_NOT_ENOUGH_MEMORY, L"malloc");
	}
	vmargs[0] = L'\0';

	if(opt['a'] != NULL && *opt['a'] != L'\0')
	{
		wcscat_s(vmargs, BUFFER_SIZE, opt['a']);

		if(wcsstr(opt['a'], L"-Dvisualvm.display.name="))
		{
			contains_visualvm_display_name = TRUE;
		}
	}
	if(!contains_visualvm_display_name)
	{
		wchar_t* visualvm_display_name = (wchar_t*)malloc(MAX_LONG_PATH * sizeof(wchar_t));
		if(visualvm_display_name == NULL)
		{
			exit_process(ERROR_NOT_ENOUGH_MEMORY, L"malloc");
		}
		wcscpy_s(visualvm_display_name, MAX_LONG_PATH, L"-Dvisualvm.display.name=");
		wcscat_s(visualvm_display_name, MAX_LONG_PATH, wcsrchr(exe_file, L'\\') + 1);

		if(wcslen(vmargs) > 0)
		{
			wcscat_s(vmargs, BUFFER_SIZE, L" ");
		}
		wcscat_s(vmargs, BUFFER_SIZE, visualvm_display_name);
	}
	if(wcslen(vmargs) > 0)
	{
		utf8 = to_utf8(vmargs);
		set_resource(exe_file, L"VMARGS", RT_RCDATA, (BYTE*)utf8, (DWORD)(strlen(utf8) + 1));
		free(utf8);
	}
	free(vmargs);

	if(opt['b'] != NULL && *opt['b'] != L'\0')
	{
		wchar_t * vmargs_b = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
		if(vmargs_b == NULL)
		{
			exit_process(ERROR_NOT_ENOUGH_MEMORY, L"malloc");
		}
		wcscpy_s(vmargs_b, BUFFER_SIZE, opt['b']);
		utf8 = to_utf8(vmargs_b);
		set_resource(exe_file, L"VMARGS_B", RT_RCDATA, (BYTE*)utf8, (DWORD)(strlen(utf8) + 1));
		free(utf8);
		free(vmargs_b);
	}

	if(opt['i'] != NULL)
	{
		if(*opt['i'] != L'-' && *opt['i'] != L'\0')
		{
			size_t len = wcslen(opt['i']);
			if(len > 4 && _wcsicmp(opt['i'] + len - 4, L".ico") == 0)
			{
				set_application_icon_file(exe_file, opt['i']);
				is_icon_set = TRUE;
			}
			else
			{
				exit_process(ERROR_BAD_PATHNAME, opt['i']);
			}
		}
	}
	else if(opt['g'] != NULL)
	{
		if(get_resource(L"IMAGE_GUI_DEFAULT_ICON", &res) != NULL)
		{
			set_application_icon(exe_file, res.buf);
		}
	}
	else if(opt['s'] != NULL)
	{
		if(get_resource(L"IMAGE_SERVICE_DEFAULT_ICON", &res) != NULL)
		{
			set_application_icon(exe_file, res.buf);
		}
	}
	
	if(opt['v'] != NULL && *opt['v'] != L'-' && *opt['v'] != L'\0')
	{
		version_number = opt['v'];
	}
	else
	{
		version_number = DEFAULT_VERSION;
	}

	if(opt['d'] != NULL && *opt['d'] != L'-' && *opt['d'] != L'\0')
	{
		file_description = opt['d'];
		if(opt['s'] != NULL)
		{
			utf8 = to_utf8(file_description);
			set_resource(exe_file, L"SVCDESC", RT_RCDATA, (BYTE*)utf8, (DWORD)(strlen(utf8) + 1));
			free(utf8);
		}
	}
	else
	{
		file_description = L"";
	}

	if(opt['c'] != NULL && *opt['c'] != L'-' && *opt['c'] != L'\0')
	{
		copyright = opt['c'];
	}
	else
	{
		copyright = L"";
	}

	if(opt['C'] != NULL && *opt['C'] != L'-' && *opt['C'] != L'\0')
	{
		company_name = opt['C'];
	}
	else
	{
		company_name = L"";
	}

	if(opt['p'] != NULL && *opt['p'] != L'-' && *opt['p'] != L'\0')
	{
		product_name = opt['p'];
	}
	else
	{
		product_name = L"";
	}

	if(opt['V'] != NULL && *opt['V'] != L'-' && *opt['V'] != L'\0')
	{
		product_version = opt['V'];
	}
	else
	{
		product_version = DEFAULT_PRODUCT_VERSION;
	}

	original_filename = wcsrchr(exe_file, L'\\') + 1;
	new_version = set_version_info(exe_file, version_number, previous_revision, file_description, copyright, company_name, product_name, product_version, original_filename, jar_file);
	
	if(get_resource(L"LOADER_JAR", &res) == NULL)
	{
		exit_process(GetLastError(), L"LOADER_JAR");
	}
	else
	{
		append_exe_file(exe_file, res.buf, res.len);
	}
	
	wcoutf(L"%ls (%d-bit) version %ls\r\n", original_filename, architecture_bits, new_version);
	g_is_dirty_exe = FALSE;
	
	cleanup();
	ExitProcess(0);
}

static void cleanup()
{
	if(g_is_dirty_exe)
	{
		DeleteFile(g_exe_file);
	}

	if(env != NULL)
	{
		if((*env)->ExceptionCheck(env) == JNI_TRUE)
		{
			(*env)->ExceptionDescribe(env);
			(*env)->ExceptionClear(env);
		}
		detach_java_vm();
	}
	if(jvm != NULL)
	{
		destroy_java_vm();
	}
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

	cleanup();
	ExitProcess(last_error);
}

static wchar_t** parse_opt(int argc, const wchar_t* argv[])
{
	wchar_t** opt = (wchar_t**)calloc(256, sizeof(wchar_t*));

	if(opt == NULL)
	{
		return NULL;
	}

	if((argc > 1) && (*argv[1] != L'-'))
	{
		opt[0] = (wchar_t*)argv[1];
	}
	while(*++argv)
	{
		if(*argv[0] == L'-' && *(argv[0] + 1) != L'\0' && *(argv[0] + 2) == L'\0')
		{
			char* c = (char*)(argv[0] + 1);
			char  c1 = *(c + 0);
			char  c2 = *(c + 1);
			if(c2 == 0x00)
			{
				if(argv[1] == NULL)
				{
					opt[c1] = (wchar_t*)L"";
				}
				else
				{
					opt[c1] = (wchar_t*)argv[1];
				}
			}
		}
	}
	argv--;
	if((opt[0] == NULL) && (*argv[0] != L'-'))
	{
		opt[0] = (wchar_t*)argv[0];
	}

	return opt;
}


static DWORD get_version_revision(const wchar_t* filename)
{
	/* GetFileVersionInfoSize, GetFileVersionInfo を使うと内部で LoadLibrary が使用されるらしく
	 * その後のリソース書き込みがうまくいかなくなるようです。なので、自力で EXEファイルから
	 * リビジョンナンバーを取り出すように変更しました。
	 */
	int    SCAN_SIZE = 8192 * 3;
	DWORD  revision = MAXDWORD;
	HANDLE hFile;
	char   HEADER[] = "VS_VERSION_INFO";
	size_t len;
	BYTE*  buf = NULL;
	DWORD  size;
	unsigned int i;
	size_t j;

	buf = (BYTE*)malloc(SCAN_SIZE);
	if(buf == NULL)
	{
		exit_process(ERROR_NOT_ENOUGH_MEMORY, L"malloc");
	}

	hFile = CreateFile(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile == INVALID_HANDLE_VALUE)
	{
		// リビジョンナンバーが参照できなくてもプロセスを終了せずに処理を継続します。
		goto EXIT;
	}

	SetFilePointer(hFile, -1 * SCAN_SIZE, 0, FILE_END);
	ReadFile(hFile, buf, SCAN_SIZE, &size, NULL);
	CloseHandle(hFile);

	len = strlen(HEADER);
	if(size > len)
	{
		for(i = 0; i < size - len; i++)
		{
			for(j = 0; j < len; j++)
			{
				if(buf[i + j * 2] != HEADER[j] || buf[i + j * 2 + 1] != 0x00) break;
			}
			if (j == strlen(HEADER))
			{
				revision = ((buf[i + 47] << 8) & 0xFF00) | ((buf[i + 46]) & 0x00FF);
			}
		}
	}

EXIT:
	if(buf != NULL)
	{
		free(buf);
	}
	return revision;
}


static void create_exe_file(const wchar_t* filename, BYTE* image_buf, DWORD image_len, BOOL is_reverse)
{
	HANDLE    hFile;
	BYTE*     buf = NULL;
	DWORD     write_size;
	wchar_t*  dir = NULL;
	wchar_t*  ptr;
	size_t    len;

	len = wcslen(filename);
	g_exe_file = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
	if(g_exe_file == NULL)
	{
		exit_process(ERROR_NOT_ENOUGH_MEMORY, L"malloc");
	}
	wcscpy_s(g_exe_file, len + 1, filename);

	dir = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
	if(dir == NULL)
	{
		exit_process(ERROR_NOT_ENOUGH_MEMORY, L"malloc");
	}
	wcscpy_s(dir, len + 1, filename);

	ptr = wcsrchr(dir, L'\\');
	if(ptr != NULL)
	{
		*ptr = L'\0';
		if(GetFileAttributes(dir) == INVALID_FILE_ATTRIBUTES)
		{
			SHCreateDirectoryEx(NULL, dir, NULL);
		}
		/*
		if(!PathIsDirectory(dir))
		{
			SHCreateDirectoryEx(NULL, dir, NULL);
		}
		*/
	}
	
	hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile == INVALID_HANDLE_VALUE)
	{
		hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
		if(hFile == INVALID_HANDLE_VALUE)
		{
			exit_process(GetLastError(), filename);
		}
	}
	g_is_dirty_exe = TRUE;

	if(is_reverse)
	{
		DWORD i;
		buf = (BYTE*)malloc(image_len);
		if(buf == NULL)
		{
			exit_process(ERROR_NOT_ENOUGH_MEMORY, L"malloc");
		}
		for(i = 0; i < image_len; i++)
		{
			buf[i] = ~image_buf[image_len - 1 - i];
		}
		image_buf = buf;
	}

	while(image_len > 0)
	{
		if(WriteFile(hFile, image_buf, image_len, &write_size, NULL) == 0)
		{
			exit_process(GetLastError(), filename);
		}
		image_buf += write_size;
		image_len -= write_size;
	}
	CloseHandle(hFile);

	if(is_reverse)
	{
		if(buf != NULL)
		{
			free(buf);
		}
	}
	if(dir != NULL)
	{
		free(dir);
	}
}


static void append_exe_file(const wchar_t* filename, BYTE* image_buf, DWORD image_len)
{
	HANDLE hFile;
	DWORD  write_size;
	
	hFile = CreateFile(filename, FILE_APPEND_DATA, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile == INVALID_HANDLE_VALUE)
	{
		exit_process(GetLastError(), filename);
	}

	while(image_len > 0)
	{
		if(WriteFile(hFile, image_buf, image_len, &write_size, NULL) == 0)
		{
			exit_process(GetLastError(), filename);
		}
		image_buf += write_size;
		image_len -= write_size;
	}
	CloseHandle(hFile);
}

static void get_target_java_runtime_version(const wchar_t* version, DWORD* major, DWORD* minor, DWORD* build, DWORD* revision)
{
	wchar_t* p = (wchar_t*)version;

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

	if(p != NULL && major != NULL)
	{
		if(major != NULL)
		{
			*major = _wtoi(p);
		}
		if((p = wcsstr(p, L".")) != NULL && minor != NULL)
		{
			if(minor != NULL)
			{
				*minor = _wtoi(++p);
			}
			if((p = wcsstr(p, L".")) != NULL && build != NULL)
			{
				if(build != NULL)
				{
					*build = _wtoi(++p);
				}
				if((p = wcsstr(p, L".")) != NULL && revision != NULL)
				{
					if(revision != NULL)
					{
						*revision = _wtoi(++p);
					}
				}
			}
		}
	}
}


static void set_resource(const wchar_t* filename, const wchar_t* rsc_name, const wchar_t* rsc_type, BYTE* rsc_data, DWORD rsc_size)
{
	HANDLE hRes = NULL;
	BOOL   ret1 = FALSE;
	BOOL   ret2 = FALSE;
	DWORD  last_error = 0;
	int i;

	for(i = 0; i < 100; i++)
	{
		ret1 = FALSE;
		ret2 = FALSE;

		hRes = BeginUpdateResource(filename, FALSE);
		if(hRes != NULL)
		{
			ret1 = UpdateResource(hRes, rsc_type, rsc_name, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), rsc_data, rsc_size);
			if(ret1 == FALSE)
			{
				last_error = GetLastError();
			}
			ret2 = EndUpdateResource(hRes, FALSE);
			if(ret1 == TRUE && ret2 == FALSE)
			{
				last_error = GetLastError();
			}
		}
		else
		{
			last_error = GetLastError();
		}
		if(ret1 && ret2)
		{
			break;
		}
		Sleep(100);
	}
	if(ret1 == FALSE || ret2 == FALSE)
	{
		exit_process(last_error, rsc_name);
	}
	CloseHandle(hRes);
}


static BYTE* get_jar_buf(const wchar_t* jar_file, DWORD* jar_len)
{
	HANDLE hFile;
	BYTE*  buf = NULL;
	BYTE*  p;
	DWORD  r;
	DWORD  read_size;

	hFile = CreateFile(jar_file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile == INVALID_HANDLE_VALUE)
	{
		exit_process(GetLastError(), jar_file);
	}
	*jar_len = GetFileSize(hFile, NULL);
	p = buf = (BYTE*)malloc(*jar_len);
	if(buf == NULL)
	{
		exit_process(ERROR_NOT_ENOUGH_MEMORY, L"malloc");
	}

	r = *jar_len;
	while(r > 0)
	{
		if(ReadFile(hFile, p, r, &read_size, NULL) == 0)
		{
			exit_process(GetLastError(), jar_file);
		}
		p += read_size;
		r -= read_size;
	}
	CloseHandle(hFile);

	return buf;
}

static BOOL set_application_icon_file(const wchar_t* filename, const wchar_t* icon_file)
{
	BOOL   succeeded        = FALSE;
	HANDLE icon_file_handle = NULL;
	DWORD  icon_file_size;
	BYTE*  icon_file_data   = NULL;
	BYTE*  p;
	DWORD  r;
	DWORD  read_size;

	if(icon_file == NULL || wcslen(icon_file) == 0)
	{
		goto EXIT;
	}
	icon_file_handle = CreateFile(icon_file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(icon_file_handle == INVALID_HANDLE_VALUE)
	{
		goto EXIT;
	}
	icon_file_size = GetFileSize(icon_file_handle, NULL);
	if(icon_file_size == INVALID_FILE_SIZE)
	{
		goto EXIT;
	}
	icon_file_data = (BYTE*)malloc(icon_file_size);
	if(icon_file_data == NULL)
	{
		goto EXIT;
	}

	p = icon_file_data;
	r = icon_file_size;

	while(r > 0)
	{
		if(ReadFile(icon_file_handle, p, r, &read_size, NULL) == 0)
		{
			goto EXIT;
		}
		p += read_size;
		r -= read_size;
	}

	succeeded = set_application_icon(filename, icon_file_data);

EXIT:
	if(icon_file_data != NULL)
	{
		free(icon_file_data);
	}
	if(icon_file_handle != NULL && icon_file_handle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(icon_file_handle);
	}
	return succeeded;
}

static BOOL set_application_icon(const wchar_t* filename, const BYTE* icon_bytes)
{
	BOOL         succeeded   = FALSE;
	HANDLE       hResource   = NULL;
	ICONDIR      id;
	size_t       pid_size;
	ICONDIR*     pid        = NULL;
	size_t       pgid_size;
	GRPICONDIR*  pgid       = NULL;
	int          i;
	DWORD        data_size;
	BYTE*        data       = NULL;
	BOOL         b;

	hResource = BeginUpdateResource(filename, FALSE);
	if(hResource == NULL)
	{
		goto EXIT;
	}

	memcpy_s(&id, sizeof(id), icon_bytes, sizeof(id));

	pid_size = sizeof(ICONDIR) + sizeof(ICONDIRENTRY) * (id.idCount - 1);
	pid = (ICONDIR*)malloc(pid_size);
	if(pid == NULL)
	{
		goto EXIT;
	}
	memcpy_s(pid, pid_size, icon_bytes, pid_size);

	pgid_size = sizeof(GRPICONDIR) + sizeof(GRPICONDIRENTRY) * (id.idCount - 1);
	pgid = (GRPICONDIR*)malloc(pgid_size);
	if(pgid == NULL)
	{
		goto EXIT;
	}
	memcpy_s(pgid, pgid_size, icon_bytes, sizeof(GRPICONDIR));

	for(i = 0; i < id.idCount; i++)
	{
		pgid->idEntries[i].common = pid->idEntries[i].common;
		pgid->idEntries[i].nID = (WORD)(i + 1);
		data_size = pid->idEntries[i].common.dwBytesInRes;

		if(data != NULL)
		{
			free(data);
		}
		data = (BYTE*)malloc(data_size);
		if(data == NULL)
		{
			goto EXIT;
		}
		memcpy_s(data, data_size, icon_bytes + pid->idEntries[i].dwImageOffset, data_size);

		b = UpdateResource(hResource, RT_ICON, MAKEINTRESOURCE(i + 1), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), data, data_size);
		if(b == FALSE)
		{
			goto EXIT;
		}
	}
	data_size = sizeof(GRPICONDIR) + sizeof(GRPICONDIRENTRY) * (id.idCount - 1);
	b = UpdateResource(hResource, RT_GROUP_ICON, MAKEINTRESOURCE(1), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), pgid, data_size);
	if(b == FALSE)
	{
		goto EXIT;
	}

	b = EndUpdateResource(hResource, FALSE);
	if(b  == FALSE)
	{
		goto EXIT;
	}

	succeeded = TRUE;

EXIT:
	if(hResource != NULL)
	{
		//  BeginUpdateResourceで取得したハンドルをCloseHandleで閉じる必要はないらしい。
	}
	if(data != NULL)
	{
		free(data);
	}
	if(pgid != NULL)
	{
		free(pgid);
	}
	if(pid != NULL)
	{
		free(pid);
	}

	return succeeded;
}


static wchar_t* set_version_info(const wchar_t* filename, const wchar_t* version_number, DWORD previous_revision, const wchar_t* file_description, const wchar_t* copyright, const wchar_t* company_name, const wchar_t* product_name, const wchar_t* product_version, const wchar_t* original_filename, const wchar_t* jar_file)
{
	const size_t VERSION_TEXT_LENGTH    = 24;
	const size_t TEXT_LENGTH            = 128;
	const int    ADDR_COMPANY_NAME      = 0x00B8;
	const int    ADDR_FILE_DESCRIPTION  = 0x01E4;
	const int    ADDR_FILE_VERSION      = 0x0308;
	const int    ADDR_INTERNAL_NAME     = 0x0358;
	const int    ADDR_COPYRIGHT         = 0x0480;
	const int    ADDR_ORIGINAL_FILENAME = 0x05AC;
	const int    ADDR_PRODUCT_NAME      = 0x06D0;
	const int    ADDR_PRODUCT_VERSION   = 0x07F8;

	const wchar_t* internal_name;

	wchar_t* buffer;
	wchar_t* file_version;
	wchar_t* context;
	BYTE*    version_info_buf = NULL;
	DWORD    version_info_len;
	short    file_version_major;
	short    file_version_minor;
	short    file_version_build;
	short    file_version_revision;
	short    product_version_major;
	short    product_version_minor;
	short    product_version_build;
	short    product_version_revision;
	wchar_t* new_version;
	RESOURCE res;

	buffer = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
	if(buffer == NULL)
	{
		return NULL;
	}

	wcscpy_s(buffer, BUFFER_SIZE, version_number);
	wcscat_s(buffer, BUFFER_SIZE, L".0.0.0.0");

	file_version_major = (short)_wtoi(wcstok_s(buffer, L".", &context));
	file_version_minor = (short)_wtoi(wcstok_s(NULL, L".", &context));
	file_version_build = (short)_wtoi(wcstok_s(NULL, L".", &context));
	file_version_revision = (short)_wtoi(wcstok_s(NULL, L".", &context));

	wcscpy_s(buffer, BUFFER_SIZE, product_version);
	wcscat_s(buffer, BUFFER_SIZE, L".0.0.0.0");
	product_version_major = (short)_wtoi(wcstok_s(buffer, L".", &context));
	product_version_minor = (short)_wtoi(wcstok_s(NULL, L".", &context));
	product_version_build = (short)_wtoi(wcstok_s(NULL, L".", &context));
	product_version_revision = (short)_wtoi(wcstok_s(NULL, L".", &context));

	// revision が明示的に指定されていなかった場合、既存ファイルから取得した値に 1　を加算して revision とする。
	wcscpy_s(buffer, BUFFER_SIZE, version_number);
	if(wcstok_s(buffer, L".", &context) != NULL)
	{
		if(wcstok_s(NULL, L".", &context) != NULL)
		{
			if(wcstok_s(NULL, L".", &context) != NULL)
			{
				if(wcstok_s(NULL, L".", &context) != NULL)
				{
					// revision が明示的に指定されているので previous_revision を無効値にする。
					previous_revision = MAXDWORD;
				}
			}
		}
	}

	// revision が無効値でないなら revision を 1 加算します。
	if(previous_revision != MAXDWORD) {
		file_version_revision = (short)previous_revision + 1;
	}
	// build 加算判定ここまで。

	file_version = (wchar_t*)malloc(VERSION_TEXT_LENGTH * sizeof(wchar_t));
	if(file_version == NULL)
	{
		return NULL;
	}
	swprintf_s(file_version, VERSION_TEXT_LENGTH, L"%d.%d.%d.%d", file_version_major, file_version_minor, file_version_build, file_version_revision);

	if(wcsrchr(jar_file, L'\\') != NULL)
	{
		internal_name = wcsrchr(jar_file, L'\\') + 1;
	}
	else
	{
		internal_name = jar_file;
	}

	if(get_resource(L"VERSION_INFO", &res) == NULL)
	{
		exit_process(GetLastError(), L"get_resource:VERSION_INFO");
	}
	version_info_len = res.len;
	version_info_buf = (BYTE*)malloc(res.len);
	if(version_info_buf == NULL)
	{
		return NULL;
	}

	memcpy_s(version_info_buf, version_info_len, res.buf, res.len);

	//FILEVERSION
	version_info_buf[48] = file_version_minor & 0xFF;
	version_info_buf[49] = (file_version_minor >> 8) & 0xFF;
	version_info_buf[50] = file_version_major & 0xFF;
	version_info_buf[51] = (file_version_major >> 8) & 0xFF;
	version_info_buf[52] = file_version_revision & 0xFF;
	version_info_buf[53] = (file_version_revision >> 8) & 0xFF;
	version_info_buf[54] = file_version_build & 0xFF;
	version_info_buf[55] = (file_version_build >> 8) & 0xFF;
	//PRODUCTVERSION
	version_info_buf[56] = product_version_minor & 0xFF;
	version_info_buf[57] = (product_version_minor >> 8) & 0xFF;
	version_info_buf[58] = product_version_major & 0xFF;
	version_info_buf[59] = (product_version_major >> 8) & 0xFF;
	version_info_buf[60] = product_version_revision & 0xFF;
	version_info_buf[61] = (product_version_revision >> 8) & 0xFF;
	version_info_buf[62] = product_version_build & 0xFF;
	version_info_buf[63] = (product_version_build >> 8) & 0xFF;

	//
	// company_name
	//
	wcscpy_s(buffer, BUFFER_SIZE, company_name);
	if(wcslen(buffer) >= TEXT_LENGTH)
	{
		wcerrf(L"Too long company name: %ls\r\n", buffer);
		buffer[TEXT_LENGTH - 1] = L'\0';
	}
	memset(version_info_buf + ADDR_COMPANY_NAME, 0x00, TEXT_LENGTH * sizeof(wchar_t));
	wcscpy_s((wchar_t*)(version_info_buf + ADDR_COMPANY_NAME), TEXT_LENGTH, buffer);

	//
	// file_description
	//
	wcscpy_s(buffer, BUFFER_SIZE, file_description);
	if(wcslen(buffer) >= TEXT_LENGTH)
	{
		wcerrf(L"Too long file description: %ls\r\n", buffer);
		buffer[TEXT_LENGTH - 1] = L'\0';
	}
	memset(version_info_buf + ADDR_FILE_DESCRIPTION, 0x00, TEXT_LENGTH * sizeof(wchar_t));
	wcscpy_s((wchar_t*)(version_info_buf + ADDR_FILE_DESCRIPTION), TEXT_LENGTH, buffer);

	//
	// file_version
	//
	wcscpy_s(buffer, BUFFER_SIZE, file_version);
	if(wcslen(buffer) >= VERSION_TEXT_LENGTH)
	{
		wcerrf(L"Too long product version: %ls\r\n", buffer);
		buffer[VERSION_TEXT_LENGTH - 1] = L'\0';
	}
	memset(version_info_buf + ADDR_FILE_VERSION, 0x00, VERSION_TEXT_LENGTH * sizeof(wchar_t));
	wcscpy_s((wchar_t*)(version_info_buf + ADDR_FILE_VERSION), VERSION_TEXT_LENGTH, buffer);

	//
	// internal_name
	//
	wcscpy_s(buffer, BUFFER_SIZE, internal_name);
	if(wcslen(buffer) >= TEXT_LENGTH)
	{
		wcerrf(L"Too long file version: %ls\r\n", buffer);
		buffer[TEXT_LENGTH - 1] = L'\0';
	}
	memset(version_info_buf + ADDR_INTERNAL_NAME, 0x00, TEXT_LENGTH * sizeof(wchar_t));
	wcscpy_s((wchar_t*)(version_info_buf + ADDR_INTERNAL_NAME), TEXT_LENGTH, buffer);

	//
	// copyright
	//
	wcscpy_s(buffer, BUFFER_SIZE, copyright);
	if(wcslen(buffer) >= TEXT_LENGTH)
	{
		wcerrf(L"Too long copyright: %ls\r\n", buffer);
		buffer[TEXT_LENGTH - 1] = L'\0';
	}
	memset(version_info_buf + ADDR_COPYRIGHT, 0x00, TEXT_LENGTH * sizeof(wchar_t));
	wcscpy_s((wchar_t*)(version_info_buf + ADDR_COPYRIGHT), TEXT_LENGTH, buffer);

	//
	// original_filename
	//
	wcscpy_s(buffer, BUFFER_SIZE, original_filename);
	if(wcslen(buffer) >= TEXT_LENGTH)
	{
		wcerrf(L"Too long original filename: %ls\r\n", buffer);
		buffer[TEXT_LENGTH - 1] = L'\0';
	}
	memset(version_info_buf + ADDR_ORIGINAL_FILENAME, 0x00, TEXT_LENGTH * sizeof(wchar_t));
	wcscpy_s((wchar_t*)(version_info_buf + ADDR_ORIGINAL_FILENAME), TEXT_LENGTH, buffer);

	//
	// product_name
	//
	wcscpy_s(buffer, BUFFER_SIZE, product_name);
	if(wcslen(buffer) >= TEXT_LENGTH)
	{
		wcerrf(L"Too long product name: %ls\r\n", buffer);
		buffer[TEXT_LENGTH - 1] = L'\0';
	}
	memset(version_info_buf + ADDR_PRODUCT_NAME, 0x00, TEXT_LENGTH * sizeof(wchar_t));
	wcscpy_s((wchar_t*)(version_info_buf + ADDR_PRODUCT_NAME), TEXT_LENGTH, buffer);

	//
	// product_version
	//
	wcscpy_s(buffer, BUFFER_SIZE, product_version);
	if(wcslen(buffer) >= VERSION_TEXT_LENGTH)
	{
		wcerrf(L"Too long product version: %ls\r\n", buffer);
		buffer[VERSION_TEXT_LENGTH - 1] = L'\0';
	}
	memset(version_info_buf + ADDR_PRODUCT_VERSION, 0x00, VERSION_TEXT_LENGTH * sizeof(wchar_t));
	wcscpy_s((wchar_t*)(version_info_buf + ADDR_PRODUCT_VERSION), VERSION_TEXT_LENGTH, buffer);

	//
	//
	//
	set_resource(filename, MAKEINTRESOURCE(VS_VERSION_INFO), RT_VERSION, version_info_buf, version_info_len);
	free(version_info_buf);

	new_version = (wchar_t*)malloc(VERSION_TEXT_LENGTH * sizeof(wchar_t));
	if(new_version != NULL)
	{
		swprintf_s(new_version, VERSION_TEXT_LENGTH, L"%d.%d.%d.%d", file_version_major, file_version_minor, file_version_build, file_version_revision);
	}

	return new_version;
}

static wchar_t* get_exewrap_version_string()
{
	HRSRC             hrsrc;
	VS_FIXEDFILEINFO* verInfo;
	size_t            buf_size;
	wchar_t*          buf = NULL;
	DWORD             major;
	DWORD             minor;
	DWORD             build;
	DWORD             revision;

	hrsrc = FindResource(NULL, MAKEINTRESOURCE(VS_VERSION_INFO), RT_VERSION);
	if(hrsrc == NULL)
	{
		return NULL;
	}

	verInfo = (VS_FIXEDFILEINFO*)((char*)LockResource(LoadResource(NULL, hrsrc)) + 40);

	buf_size = 24;
	buf = (wchar_t*)malloc(buf_size * sizeof(wchar_t));
	if(buf == NULL)
	{
		return NULL;
	}

	major = verInfo->dwFileVersionMS >> 16;
	minor = verInfo->dwFileVersionMS & 0xFFFF;
	build = verInfo->dwFileVersionLS >> 16;
	revision = verInfo->dwFileVersionLS & 0xFFFF;

	if(revision != 0)
	{
		swprintf_s(buf, buf_size, L"%d.%d.%d.%d", major, minor, build, revision);
	}
	else
	{
		swprintf_s(buf, buf_size, L"%d.%d.%d", major, minor, build);
	}
	return buf;
}



DWORD uncaught_exception(JNIEnv* env, jstring thread, jthrowable throwable)
{
	UNREFERENCED_PARAMETER(env);
	UNREFERENCED_PARAMETER(thread);
	UNREFERENCED_PARAMETER(throwable);
	return ERROR_UNKNOWN;
}
