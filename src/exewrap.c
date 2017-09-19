#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <Shlwapi.h>
#include <Shlobj.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <tchar.h>
#include <locale.h>
#include <jni.h>

#include "include/jvm.h"
#include "include/loader.h"
#include "include/icon.h"
#include "include/message.h"

#define DEFAULT_VERSION         "0.0"
#define DEFAULT_PRODUCT_VERSION "0.0"

void    OutputConsole(BYTE* buf, DWORD len);
void    OutputMessage(const char* text);
UINT    UncaughtException(const char* thread, const char* message, const char* trace);

static char** parse_opt(int argc, char* argv[]);
static DWORD  get_version_revision(char* filename);
static BOOL   create_exe_file(const char* filename, BYTE* image_buf, DWORD image_len, BOOL is_reverse);
static DWORD  get_target_java_runtime_version(char* version);
static char*  get_target_java_runtime_version_string(DWORD version, char* buf);
static void   set_resource(const char* filename, const char* rsc_name, const char* rsc_type, BYTE* rsc_data, DWORD rsc_size);
static BYTE*  get_jar_buf(const char* jar_file, DWORD* jar_len);
static void   set_application_icon(const char* filename, const char* icon_file);
static char*  set_version_info(const char* filename, const char* version_number, DWORD previous_revision, char* file_description, char* copyright, char* company_name, char* product_name, char* product_version, char* original_filename, char* jar_file);


int main(int argc, char* argv[])
{
	char**   opt = NULL;
	char*    jar_file = NULL;
	char*    exe_file = NULL;
	int      architecture_bits = 0;
	char     image_name[32];
	BYTE*    image_buf;
	DWORD    image_len;
	DWORD    previous_revision = 0;
	DWORD    target_version;
	char*    target_version_string;
	BOOL     disable_pack200 = FALSE;
	BOOL     enable_java = FALSE;
	BYTE*    jar_buf;
	DWORD    jar_len;
	char*    ext_flags = NULL;
	char*    vmargs = NULL;
	char*    vmargs_b = NULL;
	char*    version_number;
	char*    file_description;
	char*    copyright;
	char*    company_name;
	char*    product_name;
	char*    product_version;
	char*    original_filename;
	char*    new_version;
	BOOL     is_trace_version = FALSE;
	BOOL     contains_visualvm_display_name = FALSE;

	char*    buf = NULL;
	char*    ptr = NULL;
	RESOURCE res;
	BOOL     b;
	
	opt = parse_opt(argc, argv);

	if((argc < 2) || ((opt['j'] == NULL) && (opt[0] == NULL)))
	{
		int bits = GetProcessArchitecture();

		if (strrchr(argv[0], '\\') > 0)
		{
			exe_file = strrchr(argv[0], '\\') + 1;
		}
		else
		{
			exe_file = argv[0];
		}
		
		is_trace_version = strstr(exe_file, "trace") != NULL;

		printf("exewrap 1.1.7 for %s (%d-bit) %s\r\n"
			   "Native executable java application wrapper.\r\n"
			   "Copyright (C) 2005-2017 HIRUKAWA Ryo. All rights reserved.\r\n"
			   "\r\n"
			   "Usage: %s <options> <jar-file>\r\n"
			   "Options:\r\n"
			   "  -g                  \t Create Window application.\r\n"
			   "  -s                  \t Create Windows Service application.\r\n"
			   "  -A <architecture>   \t Select exe-file architecture. (default %s)\r\n"
			   "  -t <version>        \t Set target java runtime version. (default 1.5)\r\n"
			   "  -2                  \t Disable Pack200.\r\n"
			   "  -M <main-class>     \t Set main-class.\r\n"
			   "  -L <ext-dirs>       \t Set ext-dirs.\r\n"
			   "  -e <ext-flags>      \t Set extended flags.\r\n"
			   "  -a <vm-args>        \t Set Java VM arguments.\r\n"
			   "  -b <vm-args>        \t This arguments will be applied when the service was started without the SCM.\r\n"
			   "  -i <icon-file>      \t Set application icon.\r\n"
			   "  -v <version>        \t Set version number.\r\n"
			   "  -d <description>    \t Set file description.\r\n"
			   "  -c <copyright>      \t Set copyright.\r\n"
			   "  -C <company-name>   \t Set company name.\r\n"
			   "  -p <product-name>   \t Set product name.\r\n"
			   "  -V <product-version>\t Set product version.\r\n"
			   "  -j <jar-file>       \t Input jar-file.\r\n"
			   "  -o <exe-file>       \t Output exe-file.\r\n"
			, (bits == 64 ? "x64" : "x86"), bits, (is_trace_version ? "[ TRACE VERSION ]" : ""), exe_file, (bits == 64 ? "x64" : "x86"));

		return 0;
	}

	buf = malloc(2048);
	
	jar_file = malloc(1024);
	if(opt['j'] && *opt['j'] != '-' && *opt['j'] != '\0')
	{
		strcpy(jar_file, opt['j']);
	}
	else
	{
		strcpy(jar_file, opt[0]);

		if(strlen(jar_file) > 4)
		{
			strcpy(buf, jar_file + strlen(jar_file) - 4);
			if(strcmp(_strupr(buf), ".JAR"))
			{
				printf("You must specify the jar-file.\n");
				return 1;
			}
		}
	}
	
	exe_file = malloc(1024);
	if(opt['o'] && *opt['o'] != '-' && *opt['o'] != '\0')
	{
		strcpy(exe_file, opt['o']);
	}
	else
	{
		strcpy(exe_file, jar_file);
		exe_file[strlen(exe_file) - 4] = 0;
		strcat(exe_file, ".exe");
	}

	if (GetFullPathName(exe_file, _MAX_PATH, buf, &ptr) == 0)
	{
		printf("Invalid path: %s\n", exe_file);
		return 2;
	}
	strcpy(exe_file, buf);

	if (is_trace_version)
	{
		*strrchr(exe_file, '.') = '\0';
		strcat(exe_file, ".TRACE.exe");
	}

	if(opt['A'])
	{
		if(strstr(opt['A'], "86") != NULL)
		{
			architecture_bits = 32;
		}
		else if (strstr(opt['A'], "64") != NULL)
		{
			architecture_bits = 64;
		}
	}
	if(architecture_bits == 0)
	{
		architecture_bits = GetProcessArchitecture();
	}
	printf("Architecture: %s\n", (architecture_bits == 64 ? "x64 (64-bit)" : "x86 (32-bit)"));

	if(opt['g'])
	{
		sprintf(image_name, "IMAGE%s_GUI_%d", (is_trace_version ? "_TRACE" : ""), architecture_bits);
	}
	else if(opt['s'])
	{
		sprintf(image_name, "IMAGE%s_SERVICE_%d", (is_trace_version ? "_TRACE" : ""), architecture_bits);
	}
	else
	{
		sprintf(image_name, "IMAGE%s_CONSOLE_%d", (is_trace_version ? "_TRACE" : ""), architecture_bits);
	}

	GetResource(image_name, &res);
	image_buf = res.buf;
	image_len = res.len;
	
	previous_revision = get_version_revision(exe_file);
	DeleteFile(exe_file);
	
	b = create_exe_file(exe_file, image_buf, image_len, TRUE);
	if (b == FALSE)
	{
		goto EXIT;
	}

	if(opt['t'])
	{
		target_version = get_target_java_runtime_version(opt['t']);
	}
	else
	{
		target_version = get_target_java_runtime_version("1.5"); //default target version 1.5
	}
	target_version_string = get_target_java_runtime_version_string(target_version, buf);
	printf("Target: %s (%d.%d.%d.%d)\n", target_version_string + 4, target_version >> 25 & 0x7F, target_version >> 18 & 0x7F, target_version >> 11 & 0x7F, target_version & 0x7FF);
	set_resource(exe_file, "TARGET_VERSION", RT_RCDATA, target_version_string, (DWORD)(4 + strlen(target_version_string + 4) + 1));
	
	if(opt['2'])
	{
		disable_pack200 = TRUE;
	}

	if(opt['L'] && *opt['L'] != '-' && *opt['L'] != '\0')
	{
		set_resource(exe_file, "EXTDIRS", RT_RCDATA, opt['L'], (DWORD)strlen(opt['L']) + 1);
	}
	else
	{
		set_resource(exe_file, "EXTDIRS", RT_RCDATA, "lib", 4);
	}

	enable_java = CreateJavaVM(NULL, FALSE, TRUE, NULL) != NULL;
	if (enable_java)
	{
		LOAD_RESULT result;
		jbyteArray  buf;
		jclass      JarProcessor;
		jmethodID   JarProcessor_init;
		jobject     jarProcessor;
		jmethodID   jarProcessor_getMainClass;
		jmethodID   jarProcessor_getClassPath;
		jmethodID   jarProcessor_getSplashScreenName;
		jmethodID   jarProcessor_getSplashScreenImage;
		jmethodID   jarProcessor_getBytes;
		char*       main_class;
		char*       class_path;
		char*       splash_screen_name;
		BYTE*       splash_screen_image;
		BYTE*       bytes;

		result.msg = (char*)malloc(2048);

		if (LoadMainClass(argc, argv, NULL, &result) == FALSE)
		{
			printf("ERROR: LoadMainClass: tool.jar exewrap.tool.JarProcessor\n");
			goto EXIT;
		}
		JarProcessor = result.MainClass;

		jar_buf = get_jar_buf(jar_file, &jar_len);
		if (jar_buf == NULL)
		{
			goto EXIT;
		}
		buf = (*env)->NewByteArray(env, jar_len);
		if (buf == NULL)
		{
			goto EXIT;
		}
		(*env)->SetByteArrayRegion(env, buf, 0, jar_len, (jbyte*)jar_buf);

		JarProcessor_init = (*env)->GetMethodID(env, JarProcessor, "<init>", "([BZ)V");
		if (JarProcessor_init == NULL)
		{
			result.msg_id = MSG_ID_ERR_GET_CONSTRUCTOR;
			printf(_(MSG_ID_ERR_GET_CONSTRUCTOR), "exewrap.tool.JarProcessor(byte[], boolean)");
			goto EXIT;
		}
		jarProcessor = (*env)->NewObject(env, JarProcessor, JarProcessor_init, buf, !disable_pack200);
		if (jarProcessor == NULL)
		{
			result.msg_id = MSG_ID_ERR_NEW_OBJECT;
			printf(_(MSG_ID_ERR_NEW_OBJECT), "exewrap.tool.JarProcessor(byte[], boolean)");
			goto EXIT;
		}
		jarProcessor_getMainClass = (*env)->GetMethodID(env, JarProcessor, "getMainClass", "()Ljava/lang/String;");
		if (jarProcessor_getMainClass == NULL)
		{
			result.msg_id = MSG_ID_ERR_GET_METHOD;
			printf(_(MSG_ID_ERR_GET_METHOD), "exewrap.tool.JarProcessor.getMainClass()");
			goto EXIT;
		}
		jarProcessor_getClassPath = (*env)->GetMethodID(env, JarProcessor, "getClassPath", "()Ljava/lang/String;");
		if (jarProcessor_getClassPath == NULL)
		{
			result.msg_id = MSG_ID_ERR_GET_METHOD;
			printf(_(MSG_ID_ERR_GET_METHOD), "exewrap.tool.JarProcessor.getClassPath()");
			goto EXIT;
		}
		jarProcessor_getSplashScreenName = (*env)->GetMethodID(env, JarProcessor, "getSplashScreenName", "()Ljava/lang/String;");
		if (jarProcessor_getSplashScreenName == NULL)
		{
			result.msg_id = MSG_ID_ERR_GET_METHOD;
			printf(_(MSG_ID_ERR_GET_METHOD), "exewrap.tool.JarProcessor.getSplashScreenName()");
			goto EXIT;
		}
		jarProcessor_getSplashScreenImage = (*env)->GetMethodID(env, JarProcessor, "getSplashScreenImage", "()[B");
		if (jarProcessor_getSplashScreenImage == NULL)
		{
			result.msg_id = MSG_ID_ERR_GET_METHOD;
			printf(_(MSG_ID_ERR_GET_METHOD), "exewrap.tool.JarProcessor.getSplashScreenImage()");
			goto EXIT;
		}
		jarProcessor_getBytes =             (*env)->GetMethodID(env, JarProcessor, "getBytes", "()[B");
		if (jarProcessor_getBytes == NULL)
		{
			result.msg_id = MSG_ID_ERR_GET_METHOD;
			printf(_(MSG_ID_ERR_GET_METHOD), "exewrap.tool.JarProcessor.getBytes()");
			goto EXIT;
		}

		if (opt['M'])
		{
			main_class = opt['M'];
		}
		else
		{
			main_class = GetShiftJIS(env, (*env)->CallObjectMethod(env, jarProcessor, jarProcessor_getMainClass));
		}
		if (main_class == NULL)
		{
			result.msg_id = MSG_ID_ERR_LOAD_MAIN_CLASS;
			printf(_(MSG_ID_ERR_LOAD_MAIN_CLASS));
			goto EXIT;
		}
		set_resource(exe_file, "MAIN_CLASS", RT_RCDATA, main_class, (DWORD)strlen(main_class) + 1);

		class_path = GetShiftJIS(env, (*env)->CallObjectMethod(env, jarProcessor, jarProcessor_getClassPath));
		if (class_path != NULL)
		{
			set_resource(exe_file, "CLASS_PATH", RT_RCDATA, class_path, (DWORD)strlen(class_path) + 1);
		}

		splash_screen_name = GetShiftJIS(env, (*env)->CallObjectMethod(env, jarProcessor, jarProcessor_getSplashScreenName));
		if (splash_screen_name != NULL)
		{
			set_resource(exe_file, "SPLASH_SCREEN_NAME", RT_RCDATA, splash_screen_name, (DWORD)strlen(splash_screen_name) + 1);
		}
		buf = (*env)->CallObjectMethod(env, jarProcessor, jarProcessor_getSplashScreenImage);
		if (buf != NULL)
		{
			jboolean isCopy = 0;
			jint len;
			splash_screen_image = (*env)->GetByteArrayElements(env, buf, &isCopy);
			len = (*env)->GetArrayLength(env, buf);
			set_resource(exe_file, "SPLASH_SCREEN_IMAGE", RT_RCDATA, splash_screen_image, (DWORD)len);
			(*env)->ReleaseByteArrayElements(env, buf, splash_screen_image, 0);
		}

		buf = (*env)->CallObjectMethod(env, jarProcessor, jarProcessor_getBytes);
		if (buf != NULL)
		{
			jboolean isCopy;
			jint len;
			bytes  = (*env)->GetByteArrayElements(env, buf, &isCopy);
			len = (*env)->GetArrayLength(env, buf);
			set_resource(exe_file, (disable_pack200 ? "JAR" : "PACK_GZ"), RT_RCDATA, bytes, (DWORD)len);
			(*env)->ReleaseByteArrayElements(env, buf, bytes, 0);
		}
	}
	else
	{
		jar_buf = get_jar_buf(jar_file, &jar_len);
		if (jar_buf != NULL)
		{
			set_resource(exe_file, "JAR", RT_RCDATA, jar_buf, jar_len);
		}
		printf("Pack200: disable / JavaVM (%d-bit) not found.\r\n", GetProcessArchitecture());
	}
	
	ext_flags = (char*)malloc(1024);
	ext_flags[0] = '\0';
	if (is_trace_version)
	{
		strcat(ext_flags, "NOSIDEBYSIDE;");
	}
	if (opt['e'] && *opt['e'] != '-' && *opt['e'] != '\0')
	{
		strcat(ext_flags, opt['e']);
	}
	set_resource(exe_file, "EXTFLAGS", RT_RCDATA, ext_flags, (DWORD)strlen(ext_flags) + 1);
	free(ext_flags);

	if (is_trace_version)
	{
		if (vmargs == NULL)
		{
			vmargs = (char*)malloc(2048);
			vmargs[0] = '\0';
		}
		else
		{
			strcat(vmargs, " ");
		}
		strcat(vmargs, "-XX:+TraceClassLoading");
	}
	if(opt['a'] && *opt['a'] != '\0')
	{
		if (vmargs == NULL)
		{
			vmargs = (char*)malloc(2048);
			vmargs[0] = '\0';
		}
		else
		{
			strcat(vmargs, " ");
		}
		strcat(vmargs, opt['a']);
	}
	if(opt['a'] && *opt['a'] != '\0')
	{
		if(strstr(opt['a'], "-Dvisualvm.display.name="))
		{
			contains_visualvm_display_name = TRUE;
		}
	}
	if(!contains_visualvm_display_name)
	{
		char* visualvm_display_name = malloc(25 + MAX_PATH);
		strcpy(visualvm_display_name, "-Dvisualvm.display.name=");
		strcat(visualvm_display_name, strrchr(exe_file, '\\') + 1);
		
		if (vmargs == NULL)
		{
			vmargs = (char*)malloc(2048);
			vmargs[0] = '\0';
		}
		else
		{
			strcat(vmargs, " ");
		}
		strcat(vmargs, visualvm_display_name);
	}
	if (vmargs != NULL)
	{
		set_resource(exe_file, "VMARGS", RT_RCDATA, vmargs, (DWORD)strlen(vmargs) + 1);
		free(vmargs);
	}
	
	if(opt['b'] && *opt['b'] != '\0')
	{
		if (vmargs_b == NULL)
		{
			vmargs_b = (char*)malloc(2048);
			vmargs_b[0] = '\0';
		}
		else
		{
			strcat(vmargs_b, " ");
		}
		strcat(vmargs_b, opt['b']);
	}
	if (vmargs_b != NULL)
	{
		set_resource(exe_file, "VMARGS_B", RT_RCDATA, vmargs_b, (DWORD)strlen(vmargs_b) + 1);
		free(vmargs_b);
	}

	if(opt['i'] && *opt['i'] != '-' && *opt['i'] != '\0')
	{
		set_application_icon(exe_file, opt['i']);
	}
	
	if(opt['v'] && *opt['v'] != '-' && *opt['v'] != '\0')
	{
		version_number = opt['v'];
	}
	else
	{
		version_number = DEFAULT_VERSION;
	}

	if(opt['d'] && *opt['d'] != '-' && *opt['d'] != '\0')
	{
		file_description = opt['d'];
		if(opt['s'])
		{
			set_resource(exe_file, "SVCDESC", RT_RCDATA, opt['d'], (DWORD)strlen(opt['d']) + 1);
		}
	}
	else
	{
		file_description = (char*)"";
	}

	if(opt['c'] && *opt['c'] != '-' && *opt['c'] != '\0')
	{
		copyright = opt['c'];
	}
	else
	{
		copyright = (char*)"";
	}

	if(opt['C'] && *opt['C'] != '-' && *opt['C'] != '\0')
	{
		company_name = opt['C'];
	}
	else
	{
		company_name = (char*)"";
	}

	if(opt['p'] && *opt['p'] != '-' && *opt['p'] != '\0')
	{
		product_name = opt['p'];
	}
	else
	{
		product_name = (char*)"";
	}

	if(opt['V'] && *opt['V'] != '-' && *opt['V'] != '\0')
	{
		product_version = opt['V'];
	}
	else
	{
		product_version = DEFAULT_PRODUCT_VERSION;
	}

	original_filename = strrchr(exe_file, '\\') + 1;
	new_version = set_version_info(exe_file, version_number, previous_revision, file_description, copyright, company_name, product_name, product_version, original_filename, jar_file);
	printf("%s (%d-bit) version %s\r\n", strrchr(exe_file, '\\') + 1, architecture_bits, new_version);
	
EXIT:
	if (env != NULL)
	{
		if ((*env)->ExceptionCheck(env) == JNI_TRUE)
		{
			(*env)->ExceptionDescribe(env);
			(*env)->ExceptionClear(env);
		}
		DetachJavaVM();
	}
	if (jvm != NULL)
	{
		DestroyJavaVM();
	}
	ExitProcess(0);
}

static char** parse_opt(int argc, char* argv[])
{
	char** opt = (char**)HeapAlloc(GetProcessHeap(), 0, 256 * 8);

	SecureZeroMemory(opt, 256 * 8);

	if ((argc > 1) && (*argv[1] != '-'))
	{
		opt[0] = argv[1];
	}
	while (*++argv)
	{
		if (*argv[0] == '-' && *(argv[0] + 1) != '\0' && *(argv[0] + 2) == '\0')
		{
			if (argv[1] == NULL)
			{
				opt[*(argv[0] + 1)] = (char*)"";
			}
			else
			{
				opt[*(argv[0] + 1)] = argv[1];
			}
		}
	}
	argv--;
	if ((opt[0] == NULL) && (*argv[0] != '-'))
	{
		opt[0] = argv[0];
	}

	return opt;
}


static DWORD get_version_revision(char* filename)
{
	/* GetFileVersionInfoSize, GetFileVersionInfo を使うと内部で LoadLibrary が使用されるらしく
	* その後のリソース書き込みがうまくいかなくなるようです。なので、自力で EXEファイルから
	* リビジョンナンバーを取り出すように変更しました。
	*/
	DWORD  revision = 0;
	HANDLE hFile;
	char   HEADER[] = "VS_VERSION_INFO";
	size_t len;
	BYTE*  buf = NULL;
	DWORD  size;
	unsigned int i;
	size_t j;

	buf = (BYTE*)malloc(8192);

	hFile = CreateFile(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		goto EXIT;
	}

	SetFilePointer(hFile, -1 * 8192, 0, FILE_END);
	ReadFile(hFile, buf, 8192, &size, NULL);
	CloseHandle(hFile);

	len = strlen(HEADER);
	if(size > len)
	{
		for (i = 0; i < size - len; i++)
		{
			for (j = 0; j < len; j++)
			{
				if (buf[i + j * 2] != HEADER[j]) break;
			}
			if (j == strlen(HEADER))
			{
				revision = ((buf[i + 47] << 8) & 0xFF00) | ((buf[i + 46]) & 0x00FF);
			}
		}
	}

EXIT:
	if (buf != NULL)
	{
		free(buf);
	}
	return revision;
}


static BOOL create_exe_file(const char* filename, BYTE* image_buf, DWORD image_len, BOOL is_reverse)
{
	BOOL   ret = FALSE;
	HANDLE hFile;
	BYTE*  buf = NULL;
	DWORD  write_size;
	char*  dir;
	char*  ptr;
	
	dir = malloc(strlen(filename) + 1);
	strcpy(dir, filename);
	if((ptr = strrchr(dir, '\\')) != NULL)
	{
		*ptr = '\0';
		if(!PathIsDirectory(dir))
		{
			SHCreateDirectoryEx(NULL, dir, NULL);
		}
	}
	
	hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			printf("Failed to create file: %s\n", filename);
			goto EXIT;
		}
	}

	if (is_reverse)
	{
		DWORD i;
		buf = (BYTE*)malloc(image_len);
		for (i = 0; i < image_len; i++)
		{
			buf[i] = ~image_buf[image_len - 1 - i];
		}
		image_buf = buf;
	}
	else
	{
		buf = image_buf;
	}

	while (image_len > 0)
	{
		if (WriteFile(hFile, image_buf, image_len, &write_size, NULL) == 0)
		{
			printf("Failed to write: %s\n", filename);
			goto EXIT;
		}
		image_buf += write_size;
		image_len -= write_size;
	}
	CloseHandle(hFile);

	ret = TRUE;

EXIT:
	if (is_reverse)
	{
		if (buf != NULL)
		{
			free(buf);
		}
	}

	return ret;
}


/*
 * major(7bit) 31-25
 * minor(7bit) 24-18
 * build(7bit) 17-11
 * revision(11bit) 10-0
 */
static DWORD get_target_java_runtime_version(char* version)
{
	char* p = version;
	DWORD major = 0;
	DWORD minor = 0;
	DWORD build = 0;
	DWORD revision = 0;

	if (p != NULL)
	{
		major = atoi(p);
		if ((p = strstr(p, ".")) != NULL)
		{
			minor = atoi(++p);
			if ((p = strstr(p, ".")) != NULL)
			{
				build = atoi(++p);
				if ((p = strstr(p, ".")) != NULL)
				{
					revision = atoi(++p);
				}
			}
		}
		return major << 25 & 0xFE000000 | minor << 18 & 0x00FC0000 | build << 11 & 0x0000F800 | revision & 0x000007FF;
	}
	return 0x00000000;
}

/*
 * major(7bit) 31-25
 * minor(7bit) 24-18
 * build(7bit) 17-11
 * revision(11bit) 10-0
 */
static char* get_target_java_runtime_version_string(DWORD version, char* buf)
{
	DWORD major = version >> 25 & 0x7F;
	DWORD minor = version >> 18 & 0x7F;
	DWORD build = version >> 11 & 0x7F;
	DWORD revision = version & 0x7FF;

	*(DWORD*)buf = version;
	
	//1.7～
	if (major == 1 && minor >= 7 && build == 0)
	{
		if(revision == 0)
		{
			sprintf(buf + 4, "Java %d", minor);
		}
		else
		{
			sprintf(buf + 4, "Java %du%d", minor, revision);
		}
		return buf;
	}
	
	//1.5, 1.6
	if (major == 1 && (minor == 5 || minor == 6))
	{
		if (revision == 0)
		{
			sprintf(buf + 4, "Java %d.%d", minor, build);
		}
		else
		{
			sprintf(buf + 4, "Java %d.%d.%d", minor, build, revision);
		}
		return buf;
	}

	//1.2, 1.3, 1.4
	if (major == 1 && (minor == 2 || minor == 3 || minor == 4))
	{
		if (revision == 0)
		{
			if (build == 0)
			{
				sprintf(buf + 4, "Java2 %d.%d", major, minor);
			}
			else
			{
				sprintf(buf + 4, "Java2 %d.%d.%d", major, minor, build);
			}
		}
		else
		{
			sprintf(buf + 4, "Java2 %d.%d.%d.%d", major, minor, build, revision);
		}
		return buf;
	}

	//1.0, 1.1
	if (major == 1 && (minor == 0 || minor == 1))
	{
		if (revision == 0)
		{
			if (build == 0)
			{
				sprintf(buf + 4, "Java %d.%d", major, minor);
			}
			else
			{
				sprintf(buf + 4, "Java %d.%d.%d", major, minor, build);
			}
		}
		else
		{
			sprintf(buf + 4, "Java %d.%d.%d.%d", major, minor, build, revision);
		}
		return buf;
	}

	//other
	if (revision == 0)
	{
		sprintf(buf + 4, "Java %d.%d.%d", major, minor, build);
	}
	else
	{
		sprintf(buf + 4, "Java %d.%d.%d.%d", major, minor, build, revision);
	}
	return buf;
}


static void set_resource(const char* filename, const char* rsc_name, const char* rsc_type, BYTE* rsc_data, DWORD rsc_size)
{
	HANDLE hRes;
	BOOL   ret1;
	BOOL   ret2;
	int i;

	for (i = 0; i < 100; i++)
	{
		ret1 = FALSE;
		ret2 = FALSE;
		hRes = BeginUpdateResource(filename, FALSE);
		if (hRes != NULL)
		{
			ret1 = UpdateResource(hRes, rsc_type, rsc_name, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), rsc_data, rsc_size);
			ret2 = EndUpdateResource(hRes, FALSE);
		}
		if (ret1 && ret2)
		{
			break;
		}
		Sleep(100);
	}
	if (ret1 == FALSE || ret2 == FALSE)
	{
		printf("Failed to update resource: %s: %s\n", filename, rsc_name);
		ExitProcess(0);
	}
	CloseHandle(hRes);
}


static BYTE* get_jar_buf(const char* jar_file, DWORD* jar_len)
{
	HANDLE hFile;
	char*  buf;
	char*  p;
	DWORD  r;
	DWORD  read_size;

	hFile = CreateFile(jar_file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		printf("Failed to read file: %s\n", jar_file);
		buf = NULL;
		goto EXIT;
	}
	*jar_len = GetFileSize(hFile, NULL);
	p = buf = (BYTE*)malloc(*jar_len);

	r = *jar_len;
	while (r > 0)
	{
		if (ReadFile(hFile, p, r, &read_size, NULL) == 0)
		{
			printf("Failed to read: %s\n", jar_file);
			free(buf);
			buf = NULL;
			goto EXIT;
		}
		p += read_size;
		r -= read_size;
	}
	CloseHandle(hFile);

EXIT:
	return buf;
}


static void set_application_icon(const char* filename, const char* icon_file)
{
	void* pvFile;
	DWORD nSize;
	int f, z;
	ICONDIR id, *pid;
	GRPICONDIR *pgid;
	HANDLE hResource;
	BOOL ret1;
	BOOL ret2;
	BOOL ret3;
	int i;

	//Delete default icon.
	for (z = 0; z < 99; z++)
	{
		hResource = BeginUpdateResource(filename, FALSE);
		ret1 = UpdateResource(hResource, RT_ICON, MAKEINTRESOURCE(z + 1), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), NULL, 0);
		EndUpdateResource(hResource, FALSE);
		if (ret1 == FALSE)
		{
			break;
		}
	}

	//Delete default icon group.
	{
		hResource = BeginUpdateResource(filename, FALSE);
		UpdateResource(hResource, RT_GROUP_ICON, MAKEINTRESOURCE(1), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), NULL, 0);
		EndUpdateResource(hResource, FALSE);
	}

	if (!strlen(icon_file))
	{
		return;
	}
	if ((f = _lopen(icon_file, OF_READ)) == -1)
	{
		return;
	}

	for (i = 0; i < 100; i++)
	{
		ret1 = FALSE;
		ret2 = FALSE;
		ret3 = FALSE;
		hResource = BeginUpdateResource(filename, FALSE);

		_lread(f, &id, sizeof(id));
		_llseek(f, 0, SEEK_SET);
		pid = (ICONDIR *)HeapAlloc(GetProcessHeap(), 0, sizeof(ICONDIR) + sizeof(ICONDIRENTRY) * (id.idCount - 1));
		pgid = (GRPICONDIR *)HeapAlloc(GetProcessHeap(), 0, sizeof(GRPICONDIR) + sizeof(GRPICONDIRENTRY) * (id.idCount - 1));
		_lread(f, pid, sizeof(ICONDIR) + sizeof(ICONDIRENTRY) * (id.idCount - 1));
		memcpy(pgid, pid, sizeof(GRPICONDIR));

		for (z = 0; z < id.idCount; z++)
		{
			pgid->idEntries[z].common = pid->idEntries[z].common;
			pgid->idEntries[z].nID = z + 1;
			nSize = pid->idEntries[z].common.dwBytesInRes;
			pvFile = HeapAlloc(GetProcessHeap(), 0, nSize);
			if (!pvFile)
			{
				_lclose(f);
				return;
			}
			_llseek(f, pid->idEntries[z].dwImageOffset, SEEK_SET);
			_lread(f, pvFile, nSize);
			ret1 = UpdateResource(hResource, RT_ICON, MAKEINTRESOURCE(z + 1), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), pvFile, nSize);
			HeapFree(GetProcessHeap(), 0, pvFile);
		}
		nSize = sizeof(GRPICONDIR) + sizeof(GRPICONDIRENTRY) * (id.idCount - 1);
		ret2 = UpdateResource(hResource, RT_GROUP_ICON, MAKEINTRESOURCE(1), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), pgid, nSize);
		_lclose(f);

		ret3 = EndUpdateResource(hResource, FALSE);
		if (ret1 && ret2 && ret3)
		{
			break;
		}
		Sleep(100);
	}
	if (ret1 == FALSE || ret2 == FALSE || ret3 == FALSE)
	{
		printf("Failed to set icon\n");
		return;
	}
	CloseHandle(hResource);
}


static char* set_version_info(const char* filename, const char* version_number, DWORD previous_revision, char* file_description, char* copyright, char* company_name, char* product_name, char* product_version, char* original_filename, char* jar_file)
{
	int   i;
	int   SIZE_VERSION = 48;
	int   SIZE_TEXT = 256;
	int   ADDR_COMPANY_NAME = 0x00B8;
	int   ADDR_FILE_DESCRIPTION = 0x01E4;
	int   ADDR_FILE_VERSION = 0x0308;
	int   ADDR_INTERNAL_NAME = 0x0358;
	int   ADDR_COPYRIGHT = 0x0480;
	int   ADDR_ORIGINAL_FILENAME = 0x05AC;
	int   ADDR_PRODUCT_NAME = 0x06D0;
	int   ADDR_PRODUCT_VERSION = 0x07F8;
	char* internal_name;
	char  file_version[48];
	char  buffer[260];
	BYTE* version_info_buf = NULL;
	DWORD version_info_len;
	short file_version_major;
	short file_version_minor;
	short file_version_build;
	short file_version_revision;
	short product_version_major;
	short product_version_minor;
	short product_version_build;
	short product_version_revision;
	char* new_version;
	RESOURCE res;

	if (strrchr(jar_file, '\\') != NULL)
	{
		internal_name = strrchr(jar_file, '\\') + 1;
	}
	else
	{
		internal_name = jar_file;
	}

	strcpy(buffer, version_number);
	strcat(buffer, ".0.0.0.0");
	file_version_major = atoi(strtok(buffer, "."));
	file_version_minor = atoi(strtok(NULL, "."));
	file_version_build = atoi(strtok(NULL, "."));
	file_version_revision = atoi(strtok(NULL, "."));

	strcpy(buffer, product_version);
	strcat(buffer, ".0.0.0.0");
	product_version_major = atoi(strtok(buffer, "."));
	product_version_minor = atoi(strtok(NULL, "."));
	product_version_build = atoi(strtok(NULL, "."));
	product_version_revision = atoi(strtok(NULL, "."));

	// revison が明示的に指定されていなかった場合、既存ファイルから取得した値に 1　を加算して revision とする。
	strcpy(buffer, version_number);
	if (strtok(buffer, ".") != NULL)
	{
		if (strtok(NULL, ".") != NULL)
		{
			if (strtok(NULL, ".") != NULL)
			{
				if (strtok(NULL, ".") != NULL)
				{
					previous_revision = file_version_revision - 1;
				}
			}
		}
	}

	file_version_revision = (short)previous_revision + 1;
	// build 加算判定ここまで。
	sprintf(file_version, "%d.%d.%d.%d", file_version_major, file_version_minor, file_version_build, file_version_revision);

	GetResource("VERSION_INFO", &res);
	version_info_len = res.len;
	version_info_buf = (BYTE*)malloc(res.len);
	memcpy(version_info_buf, res.buf, res.len);

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

	SecureZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, company_name, (int)strlen(company_name), (WCHAR*)buffer, 128);
	for (i = 0; i < SIZE_TEXT; i++)
	{
		version_info_buf[ADDR_COMPANY_NAME + i] = buffer[i];
	}

	SecureZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, file_description, (int)strlen(file_description), (WCHAR*)buffer, 128);
	for (i = 0; i < SIZE_TEXT; i++)
	{
		version_info_buf[ADDR_FILE_DESCRIPTION + i] = buffer[i];
	}

	SecureZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, file_version, (int)strlen(file_version), (WCHAR*)buffer, 128);
	for (i = 0; i < SIZE_VERSION; i++)
	{
		version_info_buf[ADDR_FILE_VERSION + i] = buffer[i];
	}

	SecureZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, internal_name, (int)strlen(internal_name), (WCHAR*)buffer, 128);
	for (i = 0; i < SIZE_TEXT; i++)
	{
		version_info_buf[ADDR_INTERNAL_NAME + i] = buffer[i];
	}

	SecureZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, copyright, (int)strlen(copyright), (WCHAR*)buffer, 128);
	for (i = 0; i < SIZE_TEXT; i++)
	{
		version_info_buf[ADDR_COPYRIGHT + i] = buffer[i];
	}

	SecureZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, original_filename, (int)strlen(original_filename), (WCHAR*)buffer, 128);
	for (i = 0; i < SIZE_TEXT; i++)
	{
		version_info_buf[ADDR_ORIGINAL_FILENAME + i] = buffer[i];
	}

	SecureZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, product_name, (int)strlen(product_name), (WCHAR*)buffer, 128);
	for (i = 0; i < SIZE_TEXT; i++)
	{
		version_info_buf[ADDR_PRODUCT_NAME + i] = buffer[i];
	}

	SecureZeroMemory(buffer, sizeof(char) * 260);
	MultiByteToWideChar(CP_ACP, 0, product_version, (int)strlen(product_version), (WCHAR*)buffer, 128);
	for (i = 0; i < SIZE_VERSION; i++)
	{
		version_info_buf[ADDR_PRODUCT_VERSION + i] = buffer[i];
	}

	set_resource(filename, (LPCTSTR)VS_VERSION_INFO, RT_VERSION, version_info_buf, version_info_len);

	new_version = (char*)malloc(128);
	sprintf(new_version, "%d.%d.%d.%d", file_version_major, file_version_minor, file_version_build, file_version_revision);

	free(version_info_buf);

	return new_version;
}


void OutputConsole(BYTE* buf, DWORD len)
{
}


void OutputMessage(const char* text)
{
}


UINT UncaughtException(const char* thread, const char* message, const char* trace)
{
	return 0;
}
