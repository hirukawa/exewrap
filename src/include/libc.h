#ifndef _LIBC_H_
#define _LIBC_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
extern size_t strlen(const char* s);
extern char*  strcpy(char* dest, const char* src);
extern char*  strcat(char* dest, const char* src);
extern int    strcmp(const char* s1, const char* s2);
*/
extern void* memcpy(void* dst, const void* src, size_t size);
extern void* memset(void* dst, int val, size_t size);
extern char* lstrchr(const char* s, int c);
extern char* lstrrchr(const char* s, int c);
extern char* lstrstr(const char* haystack, const char* needle);
extern char* lstrtok(char* str, const char* delim);
extern char* lstrupr(char* s);
extern int   printf(const char* format, ...);
extern int   sprintf(char* str, const char* format, ...);
extern int   atoi(const char* nptr);
extern long  atol(const char* nptr);

#ifdef __cplusplus
}
#endif

#endif
