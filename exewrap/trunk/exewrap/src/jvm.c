#include <windows.h>
#include <stdio.h>
#include <shlobj.h>
#include <jni.h>

#include "include/jvm.h"

void    InitializePath(LPTSTR relative_classpath, LPTSTR relative_extdirs, BOOL useServerVM);
JNIEnv* CreateJavaVM(LPTSTR vm_args_opt, BOOL useServerVM, int* err);
void    DestroyJavaVM();
JNIEnv* AttachJavaVM();
void    DetachJavaVM();
DWORD   GetJavaRuntimeVersion();
jstring GetJString(JNIEnv* _env, const char* src);
LPSTR   GetShiftJIS(JNIEnv* _env, jstring src);
LPTSTR  GetArgsOpt(LPTSTR vm_args_opt);
LPTSTR  FindJavaHomeFromRegistry(LPCTSTR _subkey, char* output);
void    AddPath(LPCTSTR path);
LPTSTR  GetModulePath(LPTSTR buffer, DWORD size);
LPTSTR	GetModuleVersion(LPTSTR buffer);
BOOL    IsDirectory(LPTSTR path);
LPTSTR  lstrrchr(LPTSTR source, TCHAR c);

typedef jint (WINAPI* JNIGetDefaultJavaVMInitArgs)(JavaVMInitArgs*);
typedef jint (WINAPI* JNICreateJavaVM)(JavaVM**, void**, JavaVMInitArgs*);

JavaVM* jvm = NULL;
JNIEnv* env = NULL;
DWORD javaRuntimeVersion = 0xFFFFFFFF;

TCHAR   opt_app_path[MAX_PATH + 32];
TCHAR   opt_app_name[MAX_PATH + 32];
TCHAR	opt_app_version[64];
TCHAR   opt_policy_path[MAX_PATH + 32];
BOOL    path_initialized = FALSE;
TCHAR   binpath[MAX_PATH];
TCHAR   jvmpath[MAX_PATH];
TCHAR   extpath[MAX_PATH];
TCHAR   classpath[2048];
TCHAR   extdirs[2048];
TCHAR	libpath[2048];
HMODULE jvmdll;

JNIEnv* CreateJavaVM(LPTSTR vm_args_opt, BOOL useServerVM, int* err) {
	if(!path_initialized) {
		InitializePath(NULL, "lib", useServerVM);
	}

	jvmdll = LoadLibrary("jvm.dll");
	if(jvmdll == NULL) {
		if(err != NULL) {
			*err = JVM_ELOADLIB;
		}
		env = NULL;
		goto EXIT;
	}
	
	JNIGetDefaultJavaVMInitArgs getDefaultJavaVMInitArgs = (JNIGetDefaultJavaVMInitArgs)GetProcAddress(jvmdll, "JNI_GetDefaultJavaVMInitArgs");
	JNICreateJavaVM createJavaVM = (JNICreateJavaVM)GetProcAddress(jvmdll, "JNI_CreateJavaVM");
	
	JavaVMOption options[64];
	options[0].optionString = opt_app_path;
	options[1].optionString = opt_app_name;
	options[2].optionString = opt_app_version;
	options[3].optionString = classpath;
	options[4].optionString = extdirs;
	options[5].optionString = libpath;
	
	JavaVMInitArgs vm_args;
	vm_args.version = JNI_VERSION_1_2;
	vm_args.options = options;
	vm_args.nOptions = 6;
	vm_args.ignoreUnrecognized = 1;
	
	if(opt_policy_path[0] != 0x00) {
		options[vm_args.nOptions++].optionString = (char*)"-Djava.security.manager";
		options[vm_args.nOptions++].optionString = opt_policy_path;
	}
	
	LPTSTR a;
	while((a = GetArgsOpt(vm_args_opt)) != NULL) {
		options[vm_args.nOptions++].optionString = a;
	}
	
	getDefaultJavaVMInitArgs(&vm_args);
	int result = createJavaVM(&jvm, (void**)&env, &vm_args);
	if(err != NULL) {
		*err = result;
	}
EXIT:
	return env;
}

LPTSTR GetArgsOpt(LPTSTR vm_args_opt) {
	static LPTSTR sp;
	static LPTSTR p;
	LPTSTR ret;
	
	if((sp == NULL) && (p == NULL)) {
		sp = vm_args_opt;
	}
	p = sp;
	if(sp == NULL) {
		return NULL;
	}
	for(;;) {
		if(*p == 0x00) {
			if(p > sp) {
				ret = sp;
				sp = NULL;
				return ret;
			} else {
				return NULL;
			}
		} else if((*p == ' ') && (*(p+1) == '-')) {
			*p = 0x00;
			ret = sp;
			sp = p + 1;
			return ret;
		} else {
			p++;
		}
	}
}

void DestroyJavaVM() {
	if(jvm != NULL) {
		(*jvm)->DestroyJavaVM(jvm);
		jvm = NULL;
	}
}

JNIEnv* AttachJavaVM() {
	JNIEnv* env;
	if((*jvm)->AttachCurrentThread(jvm, (void**)&env, NULL) != 0) {
		env = NULL;
	}
	return env;
}

void DetachJavaVM() {
	(*jvm)->DetachCurrentThread(jvm);
}

DWORD GetJavaRuntimeVersion() {
	jclass systemClass;
	jmethodID getPropertyMethod;
	char* version;
	DWORD major = 0;
	DWORD minor = 0;
	DWORD build = 0;
	char* tail;
	
	if(javaRuntimeVersion == 0xFFFFFFFF) {
		systemClass = (*env)->FindClass(env, "java/lang/System");
		if(systemClass != NULL) {
			getPropertyMethod = (*env)->GetStaticMethodID(env, systemClass, "getProperty", "(Ljava/lang/String;)Ljava/lang/String;");
			if(getPropertyMethod != NULL) {
				version = GetShiftJIS(env, (jstring)((*env)->CallStaticObjectMethod(env, systemClass, getPropertyMethod, GetJString(env, "java.runtime.version"))));
				while(*version != '\0' && !('0' <= *version && *version <= '9')) {
					while(*version != '\0' && !(*version == ' ' || *version == '\t' || *version == '_' || *version == '-')) {
						version++;
					}
					if(*version == ' ' || *version == '\t' || *version == '_' || *version == '-') {
						version++;
					}
				}
				if(*version == '\0') {
					javaRuntimeVersion = 0;
					return javaRuntimeVersion;
				}
				tail = version;
				while(*tail != '\0' && (*tail == '.' || ('0' <= *tail && *tail <= '9'))) {
					tail++;
				}
				major = atoi(version);
				version = strstr(version, ".");
				if(version != NULL && version < tail) {
					minor = atoi(++version);
				}
				version = strstr(version, ".");
				if(version != NULL && version < tail) {
					build = atoi(++version);
				}
				javaRuntimeVersion = ((major << 24) & 0xFF000000) | ((minor << 16) & 0x00FF0000) | ((build << 8) & 0x0000FF00);
			}
		}
		return javaRuntimeVersion;
	}
	javaRuntimeVersion = 0;
	return javaRuntimeVersion;
}

jstring GetJString(JNIEnv* _env, const char* src) {
	WCHAR* wBuf;
	jstring str;
	if(src == NULL) {
		return NULL;
	}
	if(_env == NULL) {
		_env = env;
	}
	int wSize = MultiByteToWideChar(CP_ACP, 0, src, strlen(src), NULL, 0);
	wBuf = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, wSize * 2);
	MultiByteToWideChar(CP_ACP, 0, src,	strlen(src), wBuf, wSize);
	str = (*_env)->NewString(_env, (jchar*)wBuf, wSize);
	HeapFree(GetProcessHeap(), 0, wBuf);
	return str;
}

LPSTR GetShiftJIS(JNIEnv* _env, jstring src) {
	if(src == NULL) {
		return NULL;
	}
	if(_env == NULL) {
		_env = env;
	}
	const jchar* unicode = (*_env)->GetStringChars(_env, src, NULL);
	int length = wcslen((wchar_t*)unicode);
	LPSTR ret = (LPSTR)HeapAlloc(GetProcessHeap(), 0, sizeof(char) * length * 2 + 1);
	ZeroMemory(ret, sizeof(char) * length * 2 + 1);
	WideCharToMultiByte(CP_ACP, 0, (WCHAR*)unicode, length, ret, length * 2 + 1, NULL, NULL);
	(*_env)->ReleaseStringChars(_env, src, unicode);
	return ret;
}

void InitializePath(LPTSTR relative_classpath, LPTSTR relative_extdirs, BOOL useServerVM) {
	char modulePath[_MAX_PATH];
	char buffer[2048];
	char* token;
	DWORD size = MAX_PATH;

	path_initialized = TRUE;

	binpath[0] = '\0';
	jvmpath[0] = '\0';
	extdirs[0] = '\0';
	libpath[0] = '\0';

	TCHAR jre1[MAX_PATH+1];
	TCHAR jre2[MAX_PATH+1];
	TCHAR jre3[MAX_PATH+1];

	lstrcpy(opt_app_version, "-Djava.application.version=");
	lstrcat(opt_app_version, GetModuleVersion(buffer));

	GetModulePath(modulePath, MAX_PATH);
	lstrcpy(opt_app_path, "-Djava.application.path=");
	lstrcat(opt_app_path, modulePath);
	lstrcpy(opt_policy_path, "-Djava.security.policy=");
	lstrcat(opt_policy_path, modulePath);

	GetModuleFileName(NULL, buffer, size);
	lstrcpy(opt_app_name, "-Djava.application.name=");
	lstrcat(opt_app_name, lstrrchr(buffer, '\\') + 1);

	lstrcat(opt_policy_path, "\\");
	*(lstrrchr(buffer, '.')) = 0;
	lstrcat(opt_policy_path, lstrrchr(buffer, '\\') + 1);
	lstrcat(opt_policy_path, ".policy");
	if(GetFileAttributes(opt_policy_path + 23) == 0xFFFFFFFF) {
		opt_policy_path[0] = 0x00;
	}

	// Find local JRE
	GetModulePath(jre1, MAX_PATH);
	lstrcat(jre1, "\\jre");

	if(IsDirectory(jre1)) {
		SetEnvironmentVariable("JAVA_HOME", jre1);
		lstrcpy(binpath, jre1);
		lstrcat(binpath, "\\bin");
		lstrcpy(extpath, jre1);
		lstrcat(extpath, "\\lib\\ext");
		jvmpath[0] = '\0';
		if(useServerVM) {
			lstrcpy(jvmpath, jre1);
			lstrcat(jvmpath, "\\bin\\server");
			lstrcpy(buffer, jvmpath);
			lstrcat(buffer, "\\jvm.dll");
			if(GetFileAttributes(buffer) == 0xFFFFFFFF) {
				jvmpath[0] = '\0';
			}
		}
		if(jvmpath[0] == '\0') {
			lstrcpy(jvmpath, jre1);
			lstrcat(jvmpath, "\\bin\\client");
		}
	} else {
		GetModulePath(jre2, MAX_PATH);
		if(lstrrchr(jre2, '\\') != NULL && strlen(jre2) >= 4 && _stricmp(jre2 + strlen(jre2) - 4, "\\bin") == 0) {
			*(lstrrchr(jre2, '\\')) = 0;
			lstrcat(jre2, "\\jre");
			if(IsDirectory(jre2)) {
				SetEnvironmentVariable("JAVA_HOME", jre2);
				lstrcpy(binpath, jre2);
				lstrcat(binpath, "\\bin");
				lstrcpy(extpath, jre2);
				lstrcat(extpath, "\\lib\\ext");
				jvmpath[0] = '\0';
				if(useServerVM) {
					lstrcpy(jvmpath, jre2);
					lstrcat(jvmpath, "\\bin\\server");
					lstrcpy(buffer, jvmpath);
					lstrcat(buffer, "\\jvm.dll");
					if(GetFileAttributes(buffer) == 0xFFFFFFFF) {
						jvmpath[0] = '\0';
					}
				}
				if(jvmpath[0] == '\0') {
					lstrcpy(jvmpath, jre2);
					lstrcat(jvmpath, "\\bin\\client");
				}
			}
		}
	}

	if(jvmpath[0] == 0) {
		jre3[0] = '\0';
		if(GetEnvironmentVariable("JAVA_HOME", jre3, MAX_PATH) == 0) {
			char* subkeys[]= {
				"SOFTWARE\\Wow6432Node\\JavaSoft\\Java Development Kit",
				"SOFTWARE\\Wow6432Node\\JavaSoft\\Java Runtime Environment",
				"SOFTWARE\\JavaSoft\\Java Development Kit",
				"SOFTWARE\\JavaSoft\\Java Runtime Environment",
				NULL
			};
			int i = 0;
			char* output = malloc(MAX_PATH);
			while(subkeys[i] != NULL) {
				if(FindJavaHomeFromRegistry(subkeys[i], output) != NULL) {
					strcpy(jre3, output);
					SetEnvironmentVariable("JAVA_HOME", jre3);
					break;
				}
				i++;
			}
			free(output);
		}

		if(jre3[0] != '\0') {
			if(jre3[strlen(jre3) - 1] == '\\') {
				lstrcat(jre3, "jre");
			} else {
				lstrcat(jre3, "\\jre");
			}
			if(!IsDirectory(jre3)) {
				*(lstrrchr(jre3, '\\')) = '\0';
			}
			if(IsDirectory(jre3)) {
				lstrcpy(binpath, jre3);
				lstrcat(binpath, "\\bin");
				lstrcpy(extpath, jre3);
				lstrcat(extpath, "\\lib\\ext");
				jvmpath[0] = '\0';
				if(useServerVM) {
					lstrcpy(jvmpath, jre3);
					lstrcat(jvmpath, "\\bin\\server");
					lstrcpy(buffer, jvmpath);
					lstrcat(buffer, "\\jvm.dll");
					if(GetFileAttributes(buffer) == 0xFFFFFFFF) {
						jvmpath[0] = '\0';
					}
				}
				if(jvmpath[0] == '\0') {
					lstrcpy(jvmpath, jre3);
					lstrcat(jvmpath, "\\bin\\client");
				}
			}
		}
	}

	lstrcpy(classpath, "-Djava.class.path=");
	lstrcpy(extdirs, "-Djava.ext.dirs=");
	lstrcpy(libpath, "-Djava.library.path=.;");

	if(relative_classpath != NULL) {
		while((token = strtok(relative_classpath, ";")) != NULL) {
			if(strstr(token, ":") == NULL) {
				lstrcat(classpath, modulePath);
				lstrcat(classpath, "\\");
			}
			lstrcat(classpath, token);
			lstrcat(classpath, ";");
			relative_classpath = NULL;
		}
	}
	lstrcat(classpath, ".");

	if(relative_extdirs != NULL) {
		char* extdir = malloc(MAX_PATH);
		char* buf_extdirs = malloc(strlen(relative_extdirs) + 1);
		char* p = buf_extdirs;
		strcpy(buf_extdirs, relative_extdirs);
		while((token = strtok(p, ";")) != NULL) {
			extdir[0] = '\0';
			if(strstr(token, ":") == NULL) {
				lstrcat(extdir, modulePath);
				lstrcat(extdir, "\\");
			}
			lstrcat(extdir, token);
			if(IsDirectory(extdir)) {
				lstrcat(classpath, ";");
				lstrcat(classpath, extdir);
				lstrcat(extdirs, extdir);
				lstrcat(extdirs, ";");
				lstrcat(libpath, extdir);
				lstrcat(libpath, ";");
				AddPath(extdir);
			}
			p = NULL;
		}
		free(buf_extdirs);
		free(extdir);
	}
	lstrcat(extdirs, extpath);

	if(GetEnvironmentVariable("PATH", buffer, 2048)) {
		lstrcat(libpath, buffer);
	}

	AddPath(binpath);
	AddPath(jvmpath);
}

LPTSTR FindJavaHomeFromRegistry(LPCTSTR _subkey, char* output) {
	LPTSTR JavaHome = NULL;
	HKEY  key = NULL;
	DWORD size;
	LPTSTR subkey;
	LPTSTR buf;

	subkey = malloc(MAX_PATH);
	strcpy(subkey, _subkey);
	buf = malloc(MAX_PATH);

	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
		goto EXIT;
	}
	size = MAX_PATH;
	if(RegQueryValueEx(key, "CurrentVersion", NULL, NULL, (LPBYTE)buf, &size) != ERROR_SUCCESS) {
		goto EXIT;
	}
	RegCloseKey(key);
	key = NULL;

	lstrcat(subkey, "\\");
	lstrcat(subkey, buf);

	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
		goto EXIT;
	}

	size = MAX_PATH;
	if(RegQueryValueEx(key, "JavaHome", NULL, NULL, (LPBYTE)buf, &size) != ERROR_SUCCESS) {
		goto EXIT;
	}
	RegCloseKey(key);
	key = NULL;

	strcpy(output, buf);
	return output;

EXIT:
	if(key != NULL) {
		RegCloseKey(key);
	}
	free(buf);
	free(subkey);
	return NULL;
}

void AddPath(LPCTSTR path) {
	char* buf = (char*)HeapAlloc(GetProcessHeap(), 0, 2048);
	char* old_path = (char*)HeapAlloc(GetProcessHeap(), 0, 2048);
	GetEnvironmentVariable("PATH", old_path, 2048);
	lstrcpy(buf, path);
	lstrcat(buf, ";");
	lstrcat(buf, old_path);
	SetEnvironmentVariable("PATH", buf);
	HeapFree(GetProcessHeap(), 0, buf);
	HeapFree(GetProcessHeap(), 0, old_path);
}

LPTSTR GetModulePath(LPTSTR buffer, DWORD size) {
	GetModuleFileName(NULL, buffer, size);
	*(lstrrchr(buffer, '\\')) = 0;
	return buffer;
}

LPTSTR GetModuleVersion(LPTSTR buffer) {
	HRSRC hrsrc = FindResource(NULL, (LPCTSTR)VS_VERSION_INFO, RT_VERSION);
	VS_FIXEDFILEINFO* verInfo = (VS_FIXEDFILEINFO*)((char*)LockResource(LoadResource(NULL, hrsrc)) + 40);
	sprintf(buffer, "%d.%d.%d.%d",
		verInfo->dwFileVersionMS >> 16,
		verInfo->dwFileVersionMS & 0xFFFF,
		verInfo->dwFileVersionLS >> 16,
		verInfo->dwFileVersionLS & 0xFFFF
	);
	return buffer;
}

BOOL IsDirectory(LPTSTR path) {
	return (GetFileAttributes(path) != -1) && (GetFileAttributes(path) & FILE_ATTRIBUTE_DIRECTORY);
}

LPTSTR lstrrchr(LPTSTR source, TCHAR c) {
	LPTSTR tmp = source + lstrlen(source) - 1;
	while(tmp >= source) {
		if(*tmp == c) {
			return tmp;
		} else {
			tmp--;
		}
	}
	return NULL;
}
