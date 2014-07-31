#include <windows.h>
#include <stdio.h>

#include "include/message.h"

void service_main(int argc, char* argv[]);
extern int  service_start(int argc, char* argv[]);
extern int  service_stop(void);
extern int  service_error(void);
extern int  main_start(int argc, char* argv[]);

void  ServiceMain();
void  ServiceCtrlHandler(DWORD request);
BOOL  InstallService(int argc, char* argv[], int optc, char** opt);
BOOL  RemoveService(int optc, char** opt);
BOOL  SetServiceDescription(LPCTSTR Description);
void  SetCurrentDir(void);
void  GetBaseName(char* buffer);

static char** ParseOption(int argc, char* argv[]);
static void ShowErrorMessage();

SERVICE_STATUS        ServiceStatus;
SERVICE_STATUS_HANDLE hStatus;

static int    ARG_COUNT;
static char** ARG_VALUE;

void service_main(int argc, char* argv[]) {
	char service_name[1024];
	int i;
	char** opt;
	SC_HANDLE hSCManager = NULL;
	SC_HANDLE hService = NULL;
	
	ARG_COUNT = argc;
	ARG_VALUE = argv;
	
	if((argc >= 2) && ((_stricmp(argv[1], "-help") == 0) || (_stricmp(argv[1], "-h") == 0) || (_stricmp(argv[1], "-?") == 0))) {
		printf("Usage:\r\n"
			   "  %s [install-options] -install [runtime-arguments]\r\n"
			   "  %s [remove-options] -remove\r\n"
			   "  %s [runtime-arguments]\r\n"
			   "\r\n"
			   "Install Options:\r\n"
			   "  -n <display-name>\t set service display name.\r\n"
			   "  -i               \t allow interactive.\r\n"
			   "  -m               \t \r\n"
			   "  -d <dependencies>\t \r\n"
			   "  -u <username>    \t \r\n"
			   "  -p <password>    \t \r\n"
			   "  -s               \t start service.\r\n"
			   "\r\n"
			   "Remove Options:\r\n"
			   "  -s               \t stop service.\r\n"
			, argv[0], argv[0], argv[0]);
		return;
	}
	if((argc >= 2) && (_stricmp(argv[1], "-service") == 0)) {
		SetCurrentDir();
		GetBaseName(service_name);
		SERVICE_TABLE_ENTRY ServiceTable[2];
		ServiceTable[0].lpServiceName = service_name;
		ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceMain;
		ServiceTable[1].lpServiceName = NULL;
		ServiceTable[1].lpServiceProc = NULL;
		StartServiceCtrlDispatcher(ServiceTable);
		return;
	}
	if(argc >= 2) {
		for(i = 1; i < argc; i++) {
			if(_stricmp(argv[i], "-install") == 0) {
				opt = ParseOption(i, argv);
				if(InstallService(argc, argv, i - 1, opt)) {
					service_install(argc, argv);
					if(opt['s']) {
						Sleep(500);
						GetBaseName(service_name);
						if((hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) == NULL) {
							ShowErrorMessage();
						} else if((hService = OpenService(hSCManager, service_name, SERVICE_START)) == NULL) {
							ShowErrorMessage();
						} else {
							if(StartService(hService, 0, NULL)) {
								printf(_(MSG_ID_SUCCESS_SERVICE_START), service_name);
								printf("\n");
							} else {
								ShowErrorMessage();
							}
						}
						if(hService != NULL) {
							CloseServiceHandle(hService);
						}
						if(hSCManager != NULL) {
							CloseServiceHandle(hSCManager);
						}
					}
				}
				return;
			}
		}
	}
	if(argc >= 2) {
		for(i = 1; i < argc; i++) {
			if(_stricmp(argv[i], "-remove") == 0) {
				opt = ParseOption(i, argv);
				if(RemoveService(i - 1, opt)) {
					service_remove();
				}
				return;
			}
		}
	}
	GetBaseName(service_name);
	argv[0] = service_name;
	main_start(argc, argv);
}

void ServiceMain() {
	char service_name[1024];
	GetBaseName(service_name);

	ServiceStatus.dwServiceType             = SERVICE_WIN32;
	ServiceStatus.dwCurrentState            = SERVICE_START_PENDING;
	ServiceStatus.dwControlsAccepted        = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	ServiceStatus.dwWin32ExitCode           = 0;
	ServiceStatus.dwServiceSpecificExitCode = 0;
	ServiceStatus.dwCheckPoint              = 0;
	ServiceStatus.dwWaitHint                = 0;

	hStatus = RegisterServiceCtrlHandler(service_name, (LPHANDLER_FUNCTION)ServiceCtrlHandler);
	if(hStatus == (SERVICE_STATUS_HANDLE)0) {
		// Registering Control Handler failed.
		return;
	}

	ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	SetServiceStatus(hStatus, &ServiceStatus);

	ARG_VALUE[0] = service_name;
	ServiceStatus.dwServiceSpecificExitCode = service_start(ARG_COUNT, ARG_VALUE);
	ServiceStatus.dwWin32ExitCode = NO_ERROR;

	ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	ServiceStatus.dwCheckPoint++;
	ServiceStatus.dwWaitHint = 0;
	SetServiceStatus(hStatus, &ServiceStatus);
}

void ServiceCtrlHandler(DWORD request) {
	switch(request) {
	case SERVICE_CONTROL_STOP:
	case SERVICE_CONTROL_SHUTDOWN:
		if(ServiceStatus.dwCurrentState == SERVICE_RUNNING) {
			ServiceStatus.dwWin32ExitCode = 0;
			ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
			ServiceStatus.dwCheckPoint = 0;
			ServiceStatus.dwWaitHint = 2000;
			SetServiceStatus(hStatus, &ServiceStatus);
			service_stop();
			return;
		} else {
			ServiceStatus.dwCheckPoint++;
			SetServiceStatus(hStatus, &ServiceStatus);
			return;
		}
	}
	SetServiceStatus(hStatus, &ServiceStatus);
}

BOOL InstallService(int argc, char* argv[], int optc, char** opt) {
	BOOL ret = FALSE;
	char service_name[1024];
	GetBaseName(service_name);
	char path[1024];
	char* lpDisplayName = service_name;
	DWORD dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	DWORD dwStartType = SERVICE_AUTO_START;
	char* lpDependencies = NULL;
	char* lpServiceStartName = NULL;
	char* lpPassword = NULL;
	SC_HANDLE hSCManager = NULL;
	SC_HANDLE hService = NULL;
	
	path[0] = '"';
	GetModuleFileName(NULL, &path[1], 1024);
	
	strcat(path, "\" -service");
	int i;
	for(i = optc + 2; i < argc; i++) {
		strcat(path, " ");
		strcat(path, argv[i]);
	}
	
	if(opt['n']) {
		lpDisplayName = opt['n'];
	}
	if(opt['i'] && opt['u'] == 0 && opt['p'] == 0) {
		dwServiceType += SERVICE_INTERACTIVE_PROCESS;
	}
	if(opt['m']) {
		dwStartType = SERVICE_DEMAND_START;
	}
	if(opt['d']) {
		lpDependencies = HeapAlloc(GetProcessHeap(), 0, strlen(opt['d']) + 2);
		strcpy(lpDependencies, opt['d']);
		strcat(lpDependencies, ";");
		while(strrchr(lpDependencies, ';') != NULL) {
			*(strrchr(lpDependencies, ';')) = '\0';
		}
	}
	if(opt['u']) {
		lpServiceStartName = opt['u'];
	}
	if(opt['p']) {
		lpPassword = opt['p'];
	}
	
	if((hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) == NULL) {
		ShowErrorMessage();
		goto EXIT;
	}
	if((hService = CreateService(hSCManager, service_name, lpDisplayName,
						SERVICE_ALL_ACCESS, dwServiceType,
						dwStartType, SERVICE_ERROR_NORMAL,
						path, NULL, NULL, lpDependencies, lpServiceStartName, lpPassword)) == NULL) {
		ShowErrorMessage();
		goto EXIT;
	} else {
		printf(_(MSG_ID_SUCCESS_SERVICE_INSTALL), service_name);
		printf("\n");
		ret = TRUE;
	}
EXIT:
	if(hService != NULL) {
		CloseServiceHandle(hService);
	}
	if(hSCManager != NULL) {
		CloseServiceHandle(hSCManager);
	}
	if(lpDependencies != NULL) {
		HeapFree(GetProcessHeap(), 0, lpDependencies);
	}
	return ret;
}

BOOL RemoveService(int optc, char** opt) {
	BOOL ret = FALSE;
	char service_name[1024];
	GetBaseName(service_name);
	SC_HANDLE hSCManager = NULL;
	SC_HANDLE hService = NULL;
	SERVICE_STATUS Status;
	int i;
	
	if((hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) == NULL) {
		ShowErrorMessage();
		goto EXIT;
	}
	if((hService = OpenService(hSCManager, service_name, SERVICE_ALL_ACCESS)) == NULL) {
		ShowErrorMessage();
		goto EXIT;
	}
	if(opt['s'] && QueryServiceStatus(hService, &Status)) {
		if(Status.dwCurrentState != SERVICE_STOPPED && Status.dwCurrentState != SERVICE_STOP_PENDING) {
			if(ControlService(hService, SERVICE_CONTROL_STOP, &Status) == 0) {
				ShowErrorMessage();
				goto EXIT;
			}
			printf(_(MSG_ID_SERVICE_STOPING), service_name);
			printf("\n");
			for(i = 0; i < 240; i++) {
				if(QueryServiceStatus(hService, &Status) == 0) {
					ShowErrorMessage();
					goto EXIT;
				}
				if(Status.dwCurrentState == SERVICE_STOPPED) {
					printf(_(MSG_ID_SUCCESS_SERVICE_STOP), service_name);
					printf("\n");
					Sleep(500);
					break;
				}
				Sleep(500);
			}
		}
	}
	if(!DeleteService(hService)) {
		ShowErrorMessage();
		goto EXIT;
	}
	printf(_(MSG_ID_SUCCESS_SERVICE_REMOVE), service_name);
	printf("\n");
	ret = TRUE;
EXIT:
	if(hService != NULL) {
		CloseServiceHandle(hService);
	}
	if(hSCManager != NULL) {
		CloseServiceHandle(hSCManager);
	}
	return ret;
}

BOOL SetServiceDescription(LPCTSTR Description) {
	HKEY  hKey;
	DWORD keys;
	DWORD LastError = 0;
	char service_name[1024];
	GetBaseName(service_name);
	
	char key[1024] = "SYSTEM\\CurrentControlSet\\Services\\";
	strcat(key, service_name);
	
	if((LastError = RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_ALL_ACCESS, &hKey)) != ERROR_SUCCESS) {
		SetLastError(LastError);
		ShowErrorMessage();
		return FALSE;
	}
	if((LastError = RegSetValueEx(hKey, "Description", 0, REG_SZ, (LPBYTE)Description, strlen(Description) + 1)) != ERROR_SUCCESS) {
		SetLastError(LastError);
		ShowErrorMessage();
		RegCloseKey(hKey);
		return FALSE;
	}
    RegCloseKey(hKey);
	return TRUE;
}

void SetCurrentDir() {
	char b[1024];
	GetModuleFileName(NULL, b, 1024);
	*(strrchr(b, '\\')) = 0;
	SetCurrentDirectory(b);
}

void GetBaseName(char* buffer) {
	char b[1024];
	GetModuleFileName(NULL, b, 1024);
	*(strrchr(b, '.')) = 0;
	strcpy(buffer, strrchr(b, '\\') + 1);
}

static char** ParseOption(int argc, char* argv[]) {
	int i;
	char** opt = (char**)HeapAlloc(GetProcessHeap(), 0, 256 * 8);
	for(i = 0; i < 256; i++) {
		opt[i] = NULL;
	}
	if((argc > 1) && (*argv[1] != '-')) {
		opt[0] = argv[1];
	}
	for(i = 0; i < argc; i++) {
		if(*argv[i] == '-') {
			if(argv[i+1] == NULL || *argv[i+1] == '-') {
				opt[*(argv[i] + 1)] = "";
			} else {
				opt[*(argv[i] + 1)] = argv[i+1];
			}
		}
	}
	if((opt[0] == NULL) && (*argv[argc - 1] != '-')) {
		opt[0] = argv[argc - 1];
	}
	return opt;
}

static void ShowErrorMessage() {
	LPVOID Message = NULL;
	DWORD LastError = GetLastError();
	if(LastError != 0) {
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&Message, 0, NULL);
		printf("%s", Message);
		LocalFree(Message);
	}
}

