/* ���̃t�@�C���̕����R�[�h�� Shift_JIS (MS932) �ł��B*/

#ifndef _NOTIFY_H_
#define _NOTIFY_H_

#ifdef __cplusplus
extern "C" {
#endif


extern HANDLE notify_exec(UINT (WINAPI *start_address)(void*), int argc, const wchar_t* argv[]);
extern void   notify_close(void);


#ifdef __cplusplus
}
#endif


#endif
