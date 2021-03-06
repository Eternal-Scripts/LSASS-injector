// DedicatedInjector.cpp: 콘솔 응용 프로그램의 진입점을 정의합니다.
//

#include <Windows.h>
#include <iostream>
#include "Injector.h"
#include "Logger.h"
#define DELAY_MS_CHECK_DEBUG_EVENT 100

Logger logger(L"injectorLog");
using namespace std;
static volatile BOOL Injecting = true;
static volatile BOOL Debugger_Closed = false;

DWORD FindPid(const char* strname) {
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	PROCESSENTRY32 procEntry;
	procEntry.dwSize = sizeof(PROCESSENTRY32);
	Process32First(hSnapshot, &procEntry);

	do {
		if (!lstrcmp(procEntry.szExeFile, strname))
			return procEntry.th32ProcessID;
	} while (Process32Next(hSnapshot, &procEntry));
	return NULL;
}

bool SetPrivilege(LPCSTR lpszPrivilege, BOOL bEnablePrivilege = TRUE) {
	TOKEN_PRIVILEGES priv = { 0,0,0,0 };
	HANDLE hToken = NULL;
	LUID luid = { 0,0 };
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)) {
		if (hToken)
			CloseHandle(hToken);
		return false;
	}
	if (!LookupPrivilegeValue(0, lpszPrivilege, &luid)) {
		if (hToken)
			CloseHandle(hToken);
		return false;
	}
	priv.PrivilegeCount = 1;
	priv.Privileges[0].Luid = luid;
	priv.Privileges[0].Attributes = bEnablePrivilege ? SE_PRIVILEGE_ENABLED : SE_PRIVILEGE_REMOVED;
	if (!AdjustTokenPrivileges(hToken, false, &priv, 0, 0, 0)) {
		if (hToken)
			CloseHandle(hToken);
		return false;
	}
	if (hToken)
		CloseHandle(hToken);
	return true;
}

DWORD DebugProcess(LPVOID hProc) {
	DWORD TargetProcessID = GetProcessId((HANDLE)hProc);
	BOOL debuggerSet = DebugActiveProcess(TargetProcessID);
	if (!debuggerSet) {
		cout << "[ :( ] Failed to debug process. GetLastError() = " << dec << GetLastError() << endl;
		system("pause");
		return false;
	}
	cout << "[ :) ] Debugging process." << endl;
	SHORT keyEscape = GetAsyncKeyState(VK_ESCAPE);
	while (Injecting) {
		DEBUG_EVENT debugEvent;
		if (WaitForDebugEvent(&debugEvent, DELAY_MS_CHECK_DEBUG_EVENT)) {
			cout << "[ >> ] Debug event. Code: " << dec << debugEvent.dwDebugEventCode << ", PID: " << dec << debugEvent.dwProcessId << ", TID: " << dec << debugEvent.dwThreadId << ". Continuing execution." << endl;
			ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, DBG_CONTINUE);
		}
	}

	BOOL debuggerStopped = DebugActiveProcessStop(TargetProcessID);
	if (!debuggerStopped)
		cout << "[ :( ] Could not stop debugger! Exiting this program will most likely crash the target process." << endl;
	else
		cout << "[ :) ] Debugger stopped correctly." << endl;

	Debugger_Closed = true;
	return true;
}

int main(int argc, char* argv[])
{
	//logger.startLog();
	bool isPid = false;

	if (argc < 3) {
		cout << "Invalid Command arguments" << endl;
		return 0;
	}

	if (argc == 4)
		isPid = true;

	if (!SetPrivilege(SE_DEBUG_NAME)) {
		cout << "Can not get a debug Privilege" << endl;
		return 0;
	}

	//DLL 패러미터 셋팅
	LoadLibrary("win32u.dll");
	auto ntfunc = GetProcAddress(GetModuleHandle("win32u.dll"), "NtUserPeekMessage");
	if (!ntfunc) {
		cout << "can not get ntfunc address";
		return 0;
	}
	DLL_PARAM dllparam = { 0 };
	if (isPid) 
		memcpy(dllparam.TargetProcessName, argv[argc - 2], strlen(argv[argc - 2])+1);
	else
		memcpy(dllparam.TargetProcessName, argv[argc - 1], strlen(argv[argc - 1])+1);
	
	dllparam.addressOfHookFunction = ntfunc;
	HANDLE hProc = NULL;
#ifdef _DEBUG
	hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, FindPid("lsass.exe"));
#else
	hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, FindPid("lsass.exe"));
#endif
	if (!hProc) {
		cout << "Can not get a HANDLE of the prcoess" << endl;
		return 0;
	}
	CreateThread(0, 0, DebugProcess, hProc, 0, 0);
	Sleep(10);
	BOOL Result = false;
	logger.LogString("Call Manual Mapping");
#ifdef _DEBUG
	if (isPid) {
		cout << "Injecting... into TID : " << (DWORD)atoi(argv[argc - 1]) << endl;
		Result = ManualMap(hProc, "C:\\Users\\Hunter\\Documents\\Visual Studio 2017\\Projects\\LsassInjector\\x64\\Debug\\LsassInjector.dll",
			argv[argc - 3], &dllparam, (DWORD)atoi(argv[argc - 1]));
	}
	else {
		cout << "Injecting..." << endl;
		Result = ManualMap(hProc, "C:\\Users\\Hunter\\Documents\\Visual Studio 2017\\Projects\\LsassInjector\\x64\\Debug\\LsassInjector.dll",
			argv[argc - 2], &dllparam);
	}
#else
	if (isPid) {
		cout << "Injecting... into TID : " << (DWORD)atoi(argv[argc - 1]) << endl;
		Result = ManualMap(hProc, "C:\\Users\\Hunter\\Documents\\Visual Studio 2017\\Projects\\LsassInjector\\x64\\Release\\LsassInjector.dll",
			argv[argc - 3], &dllparam, (DWORD)atoi(argv[argc - 1]));
	}
	else {
		cout << "Injecting..." << endl;
		Result = ManualMap(hProc, "C:\\Users\\Hunter\\Documents\\Visual Studio 2017\\Projects\\LsassInjector\\x64\\Release\\LsassInjector.dll",
			argv[argc - 2], &dllparam);
	}
#endif
	if(Result)
		if(isPid)
			cout << argv[argc - 3] << "    --INJECTED INTO->    " << dllparam.TargetProcessName << endl;
		else
			cout << argv[argc - 2] << "    --INJECTED INTO->    " << dllparam.TargetProcessName << endl;
	else
		cout << "Injection Failed" << endl;
	Injecting = false;
	while (!Debugger_Closed)
		Sleep(10);
	CloseHandle(hProc);
	logger.LogString("Injection Finished");
    return 0;
}

