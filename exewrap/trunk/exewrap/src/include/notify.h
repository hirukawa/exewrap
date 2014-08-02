#ifndef _NOTIFY_H_
#define _NOTIFY_H_

#ifdef __cplusplus
extern "C" {
#endif

extern LPSTR GetModuleObjectName(LPCSTR prefix);
extern HANDLE notify_exec(DWORD (WINAPI *start_address)(void*), int argc, char* argv[]);
extern void notify_close();

#ifdef __cplusplus
}
#endif

#endif
