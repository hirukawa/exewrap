#ifndef _SERVICE_H_
#define _SERVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

extern void service_main(int argc, char* argv[]);
extern BOOL SetServiceDescription(LPCTSTR Description);
int service_install(int argc, char* argv[]);
int service_remove(void);
int service_error(void);
int service_start(int argc, char* argv[]);
int service_stop(void);

#ifdef __cplusplus
}
#endif

#endif
