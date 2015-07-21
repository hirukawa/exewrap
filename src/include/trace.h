#ifndef _TRACE_H_
#define _TRACE_H_

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

	extern const char* StartTrace(BOOL replace_stdout);
	extern void StopTrace();

#ifdef __cplusplus
}
#endif

#endif
