/* このファイルの文字コードは Shift_JIS (MS932) です。*/

#ifndef _JVM_H_
#define _JVM_H_

#define JVM_ELOADLIB  (+1)


#ifdef __cplusplus
extern "C" {
#endif


extern int      get_process_architecture(void);
extern int      get_platform_architecture(void);
extern BOOL     initialize_path(const wchar_t* relative_classpath, const wchar_t* relative_extdirs, BOOL use_server_vm, BOOL use_side_by_side_jre);
extern JNIEnv*  create_java_vm(const wchar_t* vm_args_opt, BOOL use_server_vm, BOOL use_side_by_side_jre, BOOL* is_security_manager_required, int* error);
extern jint     destroy_java_vm(void);
extern JNIEnv*  attach_java_vm(void);
extern jint     detach_java_vm(void);
extern BOOL     set_application_properties(SYSTEMTIME* startup);
extern wchar_t* get_classpath(void);
extern void     get_java_runtime_version(const wchar_t* version_string, DWORD* major, DWORD* minor, DWORD* build, DWORD* revision);
extern wchar_t* get_java_version_string(DWORD major, DWORD minor, DWORD build, DWORD revision);
extern wchar_t* get_module_version(wchar_t* buf, size_t size);

extern char*    to_platform_encoding(const wchar_t* str);
extern char*    to_utf8(const wchar_t* str);
extern wchar_t* from_utf8(const char* utf8);
extern jstring  to_jstring(JNIEnv* env, const wchar_t* str);
extern wchar_t* from_jstring(JNIEnv* env, jstring jstr);

extern JavaVM* jvm;
extern JNIEnv* env;
extern BOOL    is_add_dll_directory_supported;


#ifdef __cplusplus
}
#endif


#endif /* _JVM_H_ */
