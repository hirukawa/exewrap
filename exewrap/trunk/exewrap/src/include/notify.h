#ifndef _NOTIFY_H_
#define _NOTIFY_H_

#ifdef __cplusplus
extern "C" {
#endif

extern HANDLE NotifyExec(DWORD (WINAPI *start_address)(void*), int argc, char* argv[]);
extern void   NotifyClose();

#ifdef __cplusplus
}
#endif

#endif
