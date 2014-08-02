#include <Windows.h>

extern int main(int argc, char* argv[]);
static LPSTR  _W2A(LPCWSTR s);

void mainCRTStartup()
{
	int     argc;
	LPWSTR  lpCmdLineW;
	LPWSTR* argvW;
	LPSTR*  argvA;
	int     i;
	int     ret = 0;
	
	lpCmdLineW = GetCommandLineW();
	argvW = CommandLineToArgvW(lpCmdLineW, &argc);
	argvA = (LPSTR*)HeapAlloc(GetProcessHeap(), 0, (argc + 1) * sizeof(LPSTR));
	for(i = 0; i < argc; i++)
	{
		argvA[i] = _W2A(argvW[i]);
	}
	argvA[argc] = NULL;
	
	ret = main(argc, argvA);
	HeapFree(GetProcessHeap(), 0, argvA);
	ExitProcess(ret);
}

static LPSTR _W2A(LPCWSTR s)
{
	LPSTR buf;
	int ret;
	
	ret = WideCharToMultiByte(CP_ACP, 0, s, -1, NULL, 0, NULL, NULL);
	if(ret <= 0)
	{
		return NULL;
	}
	buf = (LPSTR)HeapAlloc(GetProcessHeap(), 0, ret + 1);
	ret = WideCharToMultiByte(CP_ACP, 0, s, -1, buf, (ret + 1), NULL, NULL);
	if(ret == 0)
	{
		HeapFree(GetProcessHeap(), 0, buf);
		return NULL;
	}
	buf[ret] = '\0';

	return buf;
}
