#include <Windows.h>

int _main(int argc, char* argv[]);
static LPWSTR _A2W(LPCSTR s);
static LPSTR  _W2A(LPCWSTR s);

void mainCRTStartup()
{
	int     argc;
	LPSTR   lpCmdLineA;
	LPWSTR  lpCmdLineW;
	LPWSTR* argvW;
	LPSTR*  argvA;
	int     i;
	int     ret;
	
	lpCmdLineA = GetCommandLineA();
	lpCmdLineW = _A2W(lpCmdLineA);
	lpCmdLineW = GetCommandLineW();
	argvW = CommandLineToArgvW(lpCmdLineW, &argc);
	argvA = (LPSTR*)HeapAlloc(GetProcessHeap(), 0, (argc + 1) * sizeof(LPSTR));
	for(i = 0; i < argc; i++)
	{
		argvA[i] = _W2A(argvW[i]);
	}
	HeapFree(GetProcessHeap(), 0, lpCmdLineW);
	argvA[argc] = NULL;
	
	ret = _main(argc, argvA);
	HeapFree(GetProcessHeap(), 0, argvA);
	ExitProcess(ret);
}

static LPWSTR _A2W(LPCSTR s)
{
	LPWSTR buf;
	int ret;

	ret = MultiByteToWideChar(CP_ACP, 0, s, -1, NULL, 0);
	if(ret == 0)
	{
		return NULL;
	}

	buf = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, (ret + 1) * sizeof(WCHAR));
	ret = MultiByteToWideChar(CP_ACP, 0, s, -1, buf, (ret + 1));
	if(ret == 0)
	{
		HeapFree(GetProcessHeap(), 0, buf);
		return NULL;
	}
	buf[ret] = L'\0';

	return buf;
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
