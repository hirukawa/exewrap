#ifndef _JVM_H_
#define _JVM_H_

#define JVM_ELOADLIB	(+1)

extern int     GetProcessArchitecture();
extern void    InitializePath(LPTSTR relative_classpath, LPTSTR relative_extdirs, BOOL useServerVM, BOOL useSideBySideJRE);
extern JNIEnv* CreateJavaVM(LPTSTR vm_args_opt, BOOL useServerVM, BOOL useSideBySideJRE, int* err);
extern void    DestroyJavaVM();
extern JNIEnv* AttachJavaVM();
extern void    DetachJavaVM();
extern DWORD   GetJavaRuntimeVersion();
extern jstring GetJString(JNIEnv* _env, const char* src);
extern LPSTR   GetShiftJIS(JNIEnv* _env, jstring src);
extern LPTSTR  GetProgramFiles();
extern LPTSTR  GetJavaVMPath();
extern LPTSTR  GetLibraryPath();
extern void    AddPath(const char* path);

extern JavaVM* jvm;
extern JNIEnv* env;

#endif
