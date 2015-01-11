#ifndef _SERVICE_H_
#define _SERVICE_H_

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int  service_main(int argc, char* argv[]);
extern BOOL SetServiceDescription(LPCTSTR Description);
extern int  ShowErrorMessage();

#ifdef __cplusplus
}
#endif

#endif
