/* このファイルの文字コードは Shift_JIS (MS932) です。*/

#ifndef _WCOUT_H_
#define _WCOUT_H_

#ifdef __cplusplus
extern "C" {
#endif


extern int      wcout(const wchar_t* str);
extern int      wcerr(const wchar_t* str);
extern int      wcoutf(const wchar_t* format, ...);
extern int      wcerrf(const wchar_t* format, ...);
extern wchar_t* get_error_message(DWORD last_error);


#ifdef __cplusplus
}
#endif


#endif /* _WCOUT_H_ */
