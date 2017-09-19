#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <shlobj.h>
#include <jni.h>

#include "include/jvm.h"

void    InitializePath(char* relative_classpath, char* relative_extdirs, BOOL useServerVM, BOOL useSideBySideJRE);
int     GetProcessArchitecture();
JNIEnv* CreateJavaVM(LPTSTR vm_args_opt, BOOL useServerVM, BOOL useSideBySideJRE, int* err);
void    DestroyJavaVM();
JNIEnv* AttachJavaVM();
void    DetachJavaVM();
DWORD   GetJavaRuntimeVersion();
jstring GetJString(JNIEnv* _env, const char* src);
LPSTR   GetShiftJIS(JNIEnv* _env, jstring src);
BOOL    FindJavaVM(char* output, const char* jre, BOOL useServerVM);
LPTSTR  FindJavaHomeFromRegistry(LPCTSTR _subkey, char* output);
void    AddPath(LPCTSTR path);
LPTSTR  GetModulePath(LPTSTR buffer, DWORD size);
LPTSTR	GetModuleVersion(LPTSTR buffer);
BOOL    IsDirectory(LPTSTR path);
char*   GetSubDirs(char* dir);
char*   AddSubDirs(char* buf, char* dir, int* size);
char**  GetArgsOpt(char* vm_args_opt, int* argc);
int     GetArchitectureBits(const char* jvmpath);
static  LPWSTR A2W(LPCSTR s);
static  LPSTR W2A(LPCWSTR s);


typedef jint (WINAPI* JNIGetDefaultJavaVMInitArgs)(JavaVMInitArgs*);
typedef jint (WINAPI* JNICreateJavaVM)(JavaVM**, void**, JavaVMInitArgs*);

JavaVM* jvm = NULL;
JNIEnv* env = NULL;
DWORD   javaRuntimeVersion = 0xFFFFFFFF;

char    opt_app_path[MAX_PATH + 32];
char    opt_app_name[MAX_PATH + 32];
char	opt_app_version[64];
char    opt_policy_path[MAX_PATH + 32];
BOOL    path_initialized = FALSE;
char    binpath[MAX_PATH];
char    jvmpath[MAX_PATH];
char    extpath[MAX_PATH];
char*   classpath = NULL;
char*   extdirs = NULL;
char*	libpath = NULL;
HMODULE jvmdll;

/* このプロセスのアーキテクチャ(32ビット/64ビット)を返します。
 * 戻り値として32ビットなら 32 を返します。64ビットなら 64 を返します。
 */
int GetProcessArchitecture()
{
	return sizeof(int*) * 8;
}

JNIEnv* CreateJavaVM(LPTSTR vm_args_opt, BOOL useServerVM, BOOL useSideBySideJRE, int* err)
{
	JNIGetDefaultJavaVMInitArgs getDefaultJavaVMInitArgs;
	JNICreateJavaVM createJavaVM;
	JavaVMOption options[64];
	JavaVMInitArgs vm_args;
	char** argv = NULL;
	int argc;
	int i;
	int result;

	if (!path_initialized)
	{
		InitializePath(NULL, "lib", useServerVM, useSideBySideJRE);
	}

	jvmdll = LoadLibrary("jvm.dll");
	if (jvmdll == NULL)
	{
		if(err != NULL)
		{
			*err = JVM_ELOADLIB;
		}
		env = NULL;
		goto EXIT;
	}
	
	getDefaultJavaVMInitArgs = (JNIGetDefaultJavaVMInitArgs)GetProcAddress(jvmdll, "JNI_GetDefaultJavaVMInitArgs");
	createJavaVM = (JNICreateJavaVM)GetProcAddress(jvmdll, "JNI_CreateJavaVM");

	options[0].optionString = opt_app_path;
	options[1].optionString = opt_app_name;
	options[2].optionString = opt_app_version;
	options[3].optionString = classpath;
	options[4].optionString = extdirs;
	options[5].optionString = libpath;
	
	vm_args.version = JNI_VERSION_1_2;
	vm_args.options = options;
	vm_args.nOptions = 6;
	vm_args.ignoreUnrecognized = 1;
	
	if(opt_policy_path[0] != 0x00)
	{
		options[vm_args.nOptions++].optionString = (char*)"-Djava.security.manager";
		options[vm_args.nOptions++].optionString = opt_policy_path;
	}
	
	argv = GetArgsOpt(vm_args_opt, &argc);
	for (i = 0; i < argc; i++)
	{
		options[vm_args.nOptions++].optionString = argv[i];
	}
	
	getDefaultJavaVMInitArgs(&vm_args);
	result = createJavaVM(&jvm, (void**)&env, &vm_args);
	if(err != NULL)
	{
		*err = result;
	}

EXIT:
	if (argv != NULL)
	{
		for (i = 0; i < argc; i++)
		{
			HeapFree(GetProcessHeap(), 0, argv[i]);
		}
		HeapFree(GetProcessHeap(), 0, argv);
	}

	return env;
}

void DestroyJavaVM()
{
	if(jvm != NULL)
	{
		(*jvm)->DestroyJavaVM(jvm);
		jvm = NULL;
	}

	if (classpath != NULL)
	{
		HeapFree(GetProcessHeap(), 0, classpath);
		classpath = NULL;
	}
	if (extdirs != NULL)
	{
		HeapFree(GetProcessHeap(), 0, extdirs);
		extdirs = NULL;
	}
	if (libpath != NULL)
	{
		HeapFree(GetProcessHeap(), 0, libpath);
		libpath = NULL;
	}
}

JNIEnv* AttachJavaVM()
{
	JNIEnv* env;

	if((*jvm)->AttachCurrentThread(jvm, (void**)&env, NULL) != 0)
	{
		env = NULL;
	}
	return env;
}

void DetachJavaVM()
{
	(*jvm)->DetachCurrentThread(jvm);
}

/*
 * major(7bit) 31-25
 * minor(7bit) 24-18
 * build(7bit) 17-11
 * revision(11bit) 10-0
 */
DWORD GetJavaRuntimeVersion()
{
	jclass systemClass;
	jmethodID getPropertyMethod;
	char* version;
	DWORD major = 0;
	DWORD minor = 0;
	DWORD build = 0;
	DWORD revision = 0;
	
	if(javaRuntimeVersion == 0xFFFFFFFF)
	{
		systemClass = (*env)->FindClass(env, "java/lang/System");
		if(systemClass != NULL)
		{
			getPropertyMethod = (*env)->GetStaticMethodID(env, systemClass, "getProperty", "(Ljava/lang/String;)Ljava/lang/String;");
			if(getPropertyMethod != NULL)
			{
				version = GetShiftJIS(env, (jstring)((*env)->CallStaticObjectMethod(env, systemClass, getPropertyMethod, GetJString(env, "java.runtime.version"))));
				//数字が出てくるまでスキップ
				while(*version != '\0' && !('0' <= *version && *version <= '9'))
				{
					version++;
				}
				if(*version == '\0')
				{
					goto END_PARSE;
				}
				major = atoi(version);
				
				//数字以外が出てくるまでスキップ
				while(*version != '\0' && ('0' <= *version && *version <= '9'))
				{
					version++;
				}
				if(*version == '\0')
				{
					goto END_PARSE;
				}
				//数字が出てくるまでスキップ
				while(*version != '\0' && !('0' <= *version && *version <= '9'))
				{
					version++;
				}
				if(*version == '\0')
				{
					goto END_PARSE;
				}
				minor = atoi(version);
				
				//数字以外が出てくるまでスキップ
				while(*version != '\0' && ('0' <= *version && *version <= '9'))
				{
					version++;
				}
				if(*version == '\0')
				{
					goto END_PARSE;
				}
				//数字が出てくるまでスキップ
				while(*version != '\0' && !('0' <= *version && *version <= '9'))
				{
					version++;
				}
				if(*version == '\0')
				{
					goto END_PARSE;
				}
				build = atoi(version);
				
				//数字以外が出てくるまでスキップ
				while(*version != '\0' && ('0' <= *version && *version <= '9'))
				{
					version++;
				}
				if(*version == '\0')
				{
					goto END_PARSE;
				}
				//数字が出てくるまでスキップ
				while(*version != '\0' && !('0' <= *version && *version <= '9'))
				{
					version++;
				}
				if(*version == '\0')
				{
					goto END_PARSE;
				}
				revision = atoi(version);
			}
		}
END_PARSE:
		javaRuntimeVersion = ((major << 25) & 0xFE000000) | ((minor << 18) & 0x00FC0000) | ((build << 11) & 0x0000F800) | (revision & 0x000007FF);
	}
	return javaRuntimeVersion;
}

jstring GetJString(JNIEnv* _env, const char* src)
{
	int wSize;
	WCHAR* wBuf;
	jstring str;

	if(src == NULL)
	{
		return NULL;
	}
	if(_env == NULL)
	{
		_env = env;
	}
	wSize = MultiByteToWideChar(CP_ACP, 0, src, lstrlen(src), NULL, 0);
	wBuf = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, wSize * 2);
	MultiByteToWideChar(CP_ACP, 0, src,	lstrlen(src), wBuf, wSize);
	str = (*_env)->NewString(_env, (jchar*)wBuf, wSize);
	HeapFree(GetProcessHeap(), 0, wBuf);
	return str;
}

char* GetShiftJIS(JNIEnv* _env, jstring src)
{
	const jchar* unicode;
	int length;
	char* ret;

	if(src == NULL)
	{
		return NULL;
	}
	if(_env == NULL)
	{
		_env = env;
	}
	unicode = (*_env)->GetStringChars(_env, src, NULL);
	length = lstrlenW((wchar_t*)unicode);
	ret = (char*)HeapAlloc(GetProcessHeap(), 0, sizeof(char) * length * 2 + 1);
	SecureZeroMemory(ret, sizeof(char) * length * 2 + 1);
	WideCharToMultiByte(CP_ACP, 0, (WCHAR*)unicode, length, ret, length * 2 + 1, NULL, NULL);
	(*_env)->ReleaseStringChars(_env, src, unicode);
	return ret;
}

void InitializePath(char* relative_classpath, char* relative_extdirs, BOOL useServerVM, BOOL useSideBySideJRE) {
	char modulePath[_MAX_PATH];
	char* buffer;
	char* token;
	DWORD size = MAX_PATH;
	TCHAR jre1[MAX_PATH+1];
	TCHAR jre2[MAX_PATH+1];
	TCHAR jre3[MAX_PATH+1];
	TCHAR search[MAX_PATH + 1];
	WIN32_FIND_DATA fd;
	HANDLE hSearch;
	BOOL found = FALSE;

	path_initialized = TRUE;

	if (classpath == NULL)
	{
		classpath = (char*)HeapAlloc(GetProcessHeap(), 0, 64 * 1024);
	}
	if (extdirs == NULL)
	{
		extdirs = (char*)HeapAlloc(GetProcessHeap(), 0, 64 * 1024);
	}
	if (libpath == NULL)
	{
		libpath = (char*)HeapAlloc(GetProcessHeap(), 0, 64 * 1024);
	}

	binpath[0] = '\0';
	jvmpath[0] = '\0';
	extdirs[0] = '\0';
	libpath[0] = '\0';

	buffer = HeapAlloc(GetProcessHeap(), 0, 64 * 1024);

	lstrcpy(opt_app_version, "-Djava.application.version=");
	lstrcat(opt_app_version, GetModuleVersion(buffer));

	GetModulePath(modulePath, MAX_PATH);
	lstrcpy(opt_app_path, "-Djava.application.path=");
	lstrcat(opt_app_path, modulePath);
	lstrcpy(opt_policy_path, "-Djava.security.policy=");
	lstrcat(opt_policy_path, modulePath);

	GetModuleFileName(NULL, buffer, size);
	lstrcpy(opt_app_name, "-Djava.application.name=");
	lstrcat(opt_app_name, strrchr(buffer, '\\') + 1);

	lstrcat(opt_policy_path, "\\");
	*(strrchr(buffer, '.')) = 0;
	lstrcat(opt_policy_path, strrchr(buffer, '\\') + 1);
	lstrcat(opt_policy_path, ".policy");
	if(GetFileAttributes(opt_policy_path + 23) == -1)
	{
		opt_policy_path[0] = 0x00;
	}

	// Find local JRE
	if (useSideBySideJRE && jvmpath[0] == 0)
	{
		GetModulePath(jre1, MAX_PATH);
		lstrcpy(search, jre1);
		lstrcat(search, "\\jre*");
		hSearch = FindFirstFile(search, &fd);
		if (hSearch != INVALID_HANDLE_VALUE)
		{
			found = FALSE;
			do
			{
				if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					lstrcat(jre1, "\\");
					lstrcat(jre1, fd.cFileName);
					found = TRUE;
				}
			} while (!found && FindNextFile(hSearch, &fd));
			FindClose(hSearch);
			if (found)
			{
				if (FindJavaVM(jvmpath, jre1, useServerVM))
				{
					int bits = GetArchitectureBits(jvmpath);
					if (bits == 0 || bits == GetProcessArchitecture())
					{
						SetEnvironmentVariable("JAVA_HOME", jre1);
						lstrcpy(binpath, jre1);
						lstrcat(binpath, "\\bin");
						lstrcpy(extpath, jre1);
						lstrcat(extpath, "\\lib\\ext");
					}
					else
					{
						jvmpath[0] = '\0';
					}
				}
			}
		}
	}

	if (useSideBySideJRE && jvmpath[0] == 0)
	{
		GetModulePath(jre2, MAX_PATH);
		lstrcpy(search, jre2);
		lstrcat(search, "\\..\\jre*");
		hSearch = FindFirstFile(search, &fd);
		if (hSearch != INVALID_HANDLE_VALUE)
		{
			found = FALSE;
			do
			{
				if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					lstrcat(jre2, "\\");
					lstrcat(jre2, fd.cFileName);
					found = TRUE;
				}
			} while (!found && FindNextFile(hSearch, &fd));
			FindClose(hSearch);
			if (found)
			{
				if (FindJavaVM(jvmpath, jre2, useServerVM) == FALSE)
				{
					int bits = GetArchitectureBits(jvmpath);
					if (bits == 0 || bits == GetProcessArchitecture())
					{
						SetEnvironmentVariable("JAVA_HOME", jre2);
						lstrcpy(binpath, jre2);
						lstrcat(binpath, "\\bin");
						lstrcpy(extpath, jre2);
						lstrcat(extpath, "\\lib\\ext");
					}
					else
					{
						jvmpath[0] = '\0';
					}
				}
			}
		}
	}

	if(jvmpath[0] == 0)
	{
		jre3[0] = '\0';
		if(GetEnvironmentVariable("JAVA_HOME", jre3, MAX_PATH) == 0)
		{
			char* subkeys[] =
			{
				"SOFTWARE\\Wow6432Node\\JavaSoft\\Java Development Kit",
				"SOFTWARE\\Wow6432Node\\JavaSoft\\Java Runtime Environment",
				"SOFTWARE\\JavaSoft\\Java Development Kit",
				"SOFTWARE\\JavaSoft\\Java Runtime Environment",
				NULL
			};
			char* output = (char*)HeapAlloc(GetProcessHeap(), 0, MAX_PATH);
			int i = 0;

			if (GetProcessArchitecture() == 64)
			{
				i = 2; //64ビットEXEの場合は 32ビットJREに適合しないので、Wow6432 レジストリの検索をスキップします。
			}

			while(subkeys[i] != NULL)
			{
				if(FindJavaHomeFromRegistry(subkeys[i], output) != NULL)
				{
					lstrcpy(jre3, output);
					SetEnvironmentVariable("JAVA_HOME", jre3);
					break;
				}
				i++;
			}
			HeapFree(GetProcessHeap(), 0, output);
		}

		if(jre3[0] != '\0')
		{
			if(jre3[lstrlen(jre3) - 1] == '\\')
			{
				lstrcat(jre3, "jre");
			}
			else
			{
				lstrcat(jre3, "\\jre");
			}
			if(!IsDirectory(jre3))
			{
				*(strrchr(jre3, '\\')) = '\0';
			}
			if(IsDirectory(jre3))
			{
				lstrcpy(binpath, jre3);
				lstrcat(binpath, "\\bin");
				lstrcpy(extpath, jre3);
				lstrcat(extpath, "\\lib\\ext");
				FindJavaVM(jvmpath, jre3, useServerVM);
			}
		}
	}

	lstrcpy(classpath, "-Djava.class.path=");
	lstrcpy(extdirs, "-Djava.ext.dirs=");
	lstrcpy(libpath, "-Djava.library.path=.;");

	if(relative_classpath != NULL)
	{
		while((token = strtok(relative_classpath, " ")) != NULL)
		{
			if(strstr(token, ":") == NULL)
			{
				lstrcat(classpath, modulePath);
				lstrcat(classpath, "\\");
			}
			lstrcat(classpath, token);
			lstrcat(classpath, ";");
			relative_classpath = NULL;
		}
	}
	lstrcat(classpath, ".");

	if(relative_extdirs != NULL)
	{
		char* extdir = (char*)HeapAlloc(GetProcessHeap(), 0, MAX_PATH);
		char* buf_extdirs = (char*)HeapAlloc(GetProcessHeap(), 0, lstrlen(relative_extdirs) + 1);
		char* p = buf_extdirs;

		lstrcpy(buf_extdirs, relative_extdirs);
		while((token = strtok(p, ";")) != NULL)
		{
			extdir[0] = '\0';
			if(strstr(token, ":") == NULL)
			{
				lstrcat(extdir, modulePath);
				lstrcat(extdir, "\\");
			}
			lstrcat(extdir, token);
			if(IsDirectory(extdir))
			{
				char* dirs = GetSubDirs(extdir);
				char* dir = dirs;
				while (*dir)
				{
					lstrcat(classpath, ";");
					lstrcat(classpath, dir);
					lstrcat(extdirs, dir);
					lstrcat(extdirs, ";");
					lstrcat(libpath, dir);
					lstrcat(libpath, ";");
					AddPath(dir);

					dir += lstrlen(dir) + 1;
				}
				HeapFree(GetProcessHeap(), 0, dirs);
			}
			p = NULL;
		}
		HeapFree(GetProcessHeap(), 0, buf_extdirs);
		HeapFree(GetProcessHeap(), 0, extdir);
	}
	lstrcat(extdirs, extpath);

	if(GetEnvironmentVariable("PATH", buffer, 64 * 1024))
	{
		lstrcat(libpath, buffer);
	}

	AddPath(binpath);
	AddPath(jvmpath);

	HeapFree(GetProcessHeap(), 0, buffer);
}

/* 指定したJRE BINディレクトリで client\jvm.dll または server\jvm.dll を検索します。
 * jvm.dll の見つかったディレクトリを output に格納します。
 * jvm.dll が見つからなかった場合は output に client のパスを格納します。
 * useServerVM が TRUE の場合、Server VM を優先検索します。
 * jvm.dll が見つかった場合は TRUE, 見つからなかった場合は FALSE を返します。
 */
BOOL FindJavaVM(char* output, const char* jre, BOOL useServerVM)
{
	char path[MAX_PATH];
	char buf[MAX_PATH];

	if (useServerVM)
	{
		lstrcpy(path, jre);
		lstrcat(path, "\\bin\\server");
		lstrcpy(buf, path);
		lstrcat(buf, "\\jvm.dll");
		if (GetFileAttributes(buf) == -1)
		{
			lstrcpy(path, jre);
			lstrcat(path, "\\bin\\client");
			lstrcpy(buf, path);
			lstrcat(buf, "\\jvm.dll");
			if (GetFileAttributes(buf) == -1)
			{
				path[0] = '\0';
			}
		}
	}
	else
	{
		lstrcpy(path, jre);
		lstrcat(path, "\\bin\\client");
		lstrcpy(buf, path);
		lstrcat(buf, "\\jvm.dll");
		if (GetFileAttributes(buf) == -1)
		{
			lstrcpy(path, jre);
			lstrcat(path, "\\bin\\server");
			lstrcpy(buf, path);
			lstrcat(buf, "\\jvm.dll");
			if (GetFileAttributes(buf) == -1)
			{
				path[0] = '\0';
			}
		}
	}
	if (path[0] != '\0')
	{
		lstrcpy(output, path);
		return TRUE;
	}
	else
	{
		lstrcpy(output, jre);
		lstrcat(output, "\\bin\\client");
		return FALSE;
	}
}

char* FindJavaHomeFromRegistry(LPCTSTR _subkey, char* output)
{
	char* JavaHome = NULL;
	HKEY  key = NULL;
	DWORD size;
	char* subkey;
	char* buf;

	subkey = (char*)HeapAlloc(GetProcessHeap(), 0, MAX_PATH);
	lstrcpy(subkey, _subkey);
	buf = (char*)HeapAlloc(GetProcessHeap(), 0, MAX_PATH);

	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ, &key) != ERROR_SUCCESS)
	{
		goto EXIT;
	}
	size = MAX_PATH;
	if(RegQueryValueEx(key, "CurrentVersion", NULL, NULL, (LPBYTE)buf, &size) != ERROR_SUCCESS)
	{
		goto EXIT;
	}
	RegCloseKey(key);
	key = NULL;

	lstrcat(subkey, "\\");
	lstrcat(subkey, buf);

	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ, &key) != ERROR_SUCCESS)
	{
		goto EXIT;
	}

	size = MAX_PATH;
	if(RegQueryValueEx(key, "JavaHome", NULL, NULL, (LPBYTE)buf, &size) != ERROR_SUCCESS)
	{
		goto EXIT;
	}
	RegCloseKey(key);
	key = NULL;

	lstrcpy(output, buf);
	return output;

EXIT:
	if(key != NULL)
	{
		RegCloseKey(key);
	}
	HeapFree(GetProcessHeap(), 0, buf);
	HeapFree(GetProcessHeap(), 0, subkey);

	return NULL;
}

void AddPath(LPCTSTR path)
{
	char* buf = (char*)HeapAlloc(GetProcessHeap(), 0, 64 * 1024);
	char* old_path = (char*)HeapAlloc(GetProcessHeap(), 0, 64 * 1024);

	GetEnvironmentVariable("PATH", old_path, 64 * 1024);
	lstrcpy(buf, path);
	lstrcat(buf, ";");
	lstrcat(buf, old_path);
	SetEnvironmentVariable("PATH", buf);

	HeapFree(GetProcessHeap(), 0, buf);
	HeapFree(GetProcessHeap(), 0, old_path);
}

char* GetModulePath(char* buf, DWORD size)
{
	GetModuleFileName(NULL, buf, size);
	*(strrchr(buf, '\\')) = 0;

	return buf;
}

char* GetModuleVersion(char* buf)
{
	HRSRC hrsrc;
	VS_FIXEDFILEINFO* verInfo;

	hrsrc = FindResource(NULL, (char*)VS_VERSION_INFO, RT_VERSION);
	if (hrsrc == NULL)
	{
		*buf = '\0';
		return buf;
	}

	verInfo = (VS_FIXEDFILEINFO*)((char*)LockResource(LoadResource(NULL, hrsrc)) + 40);
	sprintf(buf, "%d.%d.%d.%d",
		verInfo->dwFileVersionMS >> 16,
		verInfo->dwFileVersionMS & 0xFFFF,
		verInfo->dwFileVersionLS >> 16,
		verInfo->dwFileVersionLS & 0xFFFF
	);

	return buf;
}

BOOL IsDirectory(char* path)
{
	return (GetFileAttributes(path) != -1) && (GetFileAttributes(path) & FILE_ATTRIBUTE_DIRECTORY);
}

char* GetSubDirs(char* dir)
{
	int size = lstrlen(dir) + 2;
	char* buf;

	AddSubDirs(NULL, dir, &size);
	buf = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
	lstrcpy(buf, dir);
	AddSubDirs(buf + lstrlen(dir) + 1, dir, NULL);
	return buf;
}

char* AddSubDirs(char* buf, char* dir, int* size)
{
	WIN32_FIND_DATA fd;
	HANDLE hSearch;
	char search[MAX_PATH];
	char child[MAX_PATH];
	lstrcpy(search, dir);
	lstrcat(search, "\\*");

	hSearch = FindFirstFile(search, &fd);
	if (hSearch != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (lstrcmp(fd.cFileName, ".") == 0 || lstrcmp(fd.cFileName, "..") == 0)
				{
					continue;
				}

				lstrcpy(child, dir);
				lstrcat(child, "\\");
				lstrcat(child, fd.cFileName);
				if (size != NULL)
				{
					*size = *size + lstrlen(child) + 1;
				}
				if (buf != NULL)
				{
					lstrcpy(buf, child);
					buf += lstrlen(child) + 1;
				}
				buf = AddSubDirs(buf, child, size);
			}
		} while (FindNextFile(hSearch, &fd));
		FindClose(hSearch);
	}
	return buf;
}

char** GetArgsOpt(char* vm_args_opt, int* argc)
{
	LPWSTR lpCmdLineW;
	LPWSTR* argvW;
	LPSTR* argvA;
	int i;

	if (vm_args_opt == NULL || vm_args_opt[0] == '\0') {
		argvA = (char**)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 1);
		*argc = 0;
		return argvA;
	}

	lpCmdLineW = A2W((LPCSTR)vm_args_opt);
	if (lpCmdLineW == NULL)
	{
		return NULL;
	}
	argvW = CommandLineToArgvW(lpCmdLineW, argc);
	HeapFree(GetProcessHeap(), 0, lpCmdLineW);
	if (argvW == NULL)
	{
		return NULL;
	}

	argvA = (LPSTR*)HeapAlloc(GetProcessHeap(), 0, (*argc + 1) * sizeof(LPSTR));
	for (i = 0; i < *argc; i++)
	{
		argvA[i] = W2A(argvW[i]);
	}
	argvA[*argc] = NULL;
	LocalFree(argvW);
	return (LPTSTR*)argvA;
}

int GetArchitectureBits(const char* jvmpath)
{
	int bits = 0;
	char*  file = NULL;
	HANDLE hFile = NULL;
	BYTE*  buf = NULL;
	DWORD size = 512;
	DWORD read_size;
	UINT  i;

	file = (char*)malloc(1024);
	sprintf(file, "%s\\jvm.dll", jvmpath);

	buf = (BYTE*)malloc(size);

	hFile = CreateFile(file, GENERIC_READ, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		goto EXIT;
	}

	if (ReadFile(hFile, buf, size, &read_size, NULL) == 0)
	{
		goto EXIT;
	}
	CloseHandle(hFile);
	hFile = NULL;

	for (i = 0; i < size - 6; i++)
	{
		if (buf[i + 0] == 'P' && buf[i + 1] == 'E' && buf[i + 2] == 0x00 && buf[i + 3] == 0x00)
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
	if (hFile != NULL)
	{
		CloseHandle(hFile);
	}
	if (buf != NULL)
	{
		free(buf);
	}
	if (file != NULL)
	{
		free(file);
	}

	return bits;
}

static LPWSTR A2W(LPCSTR s)
{
	LPWSTR buf;
	int ret;

	ret = MultiByteToWideChar(CP_ACP, 0, s, -1, NULL, 0);
	if (ret == 0)
	{
		return NULL;
	}

	buf = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, (ret + 1) * sizeof(WCHAR));
	ret = MultiByteToWideChar(CP_ACP, 0, s, -1, buf, (ret + 1));
	if (ret == 0)
	{
		HeapFree(GetProcessHeap(), 0, buf);
		return NULL;
	}
	buf[ret] = L'\0';

	return buf;
}

static LPSTR W2A(LPCWSTR s)
{
	LPSTR buf;
	int ret;

	ret = WideCharToMultiByte(CP_ACP, 0, s, -1, NULL, 0, NULL, NULL);
	if (ret == 0)
	{
		return NULL;
	}
	buf = (LPSTR)HeapAlloc(GetProcessHeap(), 0, ret + 1);
	ret = WideCharToMultiByte(CP_ACP, 0, s, -1, buf, (ret + 1), NULL, NULL);
	if (ret == 0)
	{
		HeapFree(GetProcessHeap(), 0, buf);
		return NULL;
	}
	buf[ret] = '\0';

	return buf;
}
