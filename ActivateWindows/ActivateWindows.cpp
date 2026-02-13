#include <iostream>
#include <Windows.h>
#include <string>
#include <cstdlib>
#include <ShlObj_core.h>
#include <ShlObj.h>
#pragma comment(lib,"ntdll.lib")
using namespace std;
CONSOLE_SCREEN_BUFFER_INFOEX csbiex;
CONSOLE_SCREEN_BUFFER_INFO csbi;
bool IsWindows10OrGreater() {
	OSVERSIONINFOEXW osvi{};
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
	typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOEXW);
	RtlGetVersionPtr RtlGetVersion = (RtlGetVersionPtr)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
	RtlGetVersion(&osvi);
	return osvi.dwBuildNumber >= 10240;
}
void SetConsoleColor(const char* color) {
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	if (color == "red") {
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
	}
	else if (color == "green") {
		SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
	}
	else if (color == "blue") {
		SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
	}
	else if (color == "yellow") {
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
	}
	else if (color == "purple") {
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
	}
	else if (color == "cyan") {
		SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
	}
	else if (color == "white") {
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
	}
	else {
		SetConsoleScreenBufferInfoEx(hConsole, &csbiex);
	}
}
DWORD RunProcess(const wstring& cmd, string& _stdout, string& _stderr) {
	_stdout.clear();
	_stderr.clear();
	HANDLE hOutRead = 0, hOutWrite = 0;
	HANDLE hErrRead = 0, hErrWrite = 0;
	PROCESS_INFORMATION pi = { 0 };
	STARTUPINFO si = { sizeof(STARTUPINFO) };
	SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), 0, TRUE };
	CreatePipe(&hOutRead, &hOutWrite, &sa, 0) || !CreatePipe(&hErrRead, &hErrWrite, &sa, 0);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = hOutWrite;
	si.hStdError = hErrWrite;
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	CreateProcessW(0, const_cast<wchar_t*>(cmd.c_str()), 0, 0, true, 0, 0, 0, &si, &pi);
	CloseHandle(hOutWrite);
	CloseHandle(hErrWrite);
	WaitForSingleObject(pi.hProcess, INFINITE);
	char buffer[4096]{};
	DWORD dwRead = 0;
	while (ReadFile(hOutRead, buffer, sizeof(buffer) - 1, &dwRead, 0) && dwRead > 0) {
		buffer[dwRead] = '\0';
		_stdout += buffer;
	}
	while (ReadFile(hErrRead, buffer, sizeof(buffer) - 1, &dwRead, 0) && dwRead > 0) {
		buffer[dwRead] = '\0';
		_stderr += buffer;
	}
	DWORD exitCode = 0;
	GetExitCodeProcess(pi.hProcess, &exitCode);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	CloseHandle(hOutRead);
	CloseHandle(hErrRead);
	return exitCode;
}
int RunVBScript(const wstring& cmd) {
	string out, err;
	DWORD result = RunProcess(cmd, out, err);
	if (out.size()) {
		out.pop_back();
	}
	if (err.size()) {
		err.pop_back();
	}
	if (result) {
		SetConsoleColor("red");
		cout << out;
		return 1;
	}
	else {
		SetConsoleColor("green");
		cout << out;
		return 0;
	}
	return 0;
}
void PrintSeparator(const string& text = "") {
	SetConsoleColor("default");
	int width = csbiex.srWindow.Right - csbiex.srWindow.Left + 1;
	if (text.size()) {
		int textWidth = text.size() + 2;
		int padding = (width - textWidth) / 2;
		cout << string(padding, '=') << ' ' << text << ' ' << string(padding, '=');
		if (padding * 2 + textWidth < width) {
			cout << '=';
		}
	}
	else {
		cout << string(width, '-');
	}
}
BOOL StartWindowsService(const string& serviceName) {
	SC_HANDLE hSCM = OpenSCManagerA(NULL,NULL,SC_MANAGER_ALL_ACCESS),
		hService = OpenServiceA(hSCM,serviceName.c_str(),SERVICE_START | SERVICE_QUERY_STATUS);
	SERVICE_STATUS serviceStatus = { 0 };
	QueryServiceStatus(hService, &serviceStatus);
	if (serviceStatus.dwCurrentState == SERVICE_RUNNING) {
		SetConsoleColor("green");
		cout << "服务 " << serviceName << " 已经启动。\n";
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCM);
		return true;
	}
	BOOL bStart = StartServiceA(hService,0,0);
	if (!bStart) {
		DWORD dwError = GetLastError();
		SetConsoleColor("red");
		cout << "服务启动失败：" << dwError << '\n';
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCM);
		return false;
	}
	while (true) {
		QueryServiceStatus(hService, &serviceStatus);
		if (serviceStatus.dwCurrentState == SERVICE_RUNNING) {
			SetConsoleColor("green");
			cout << "服务 " << serviceName << " 已经启动。\n";
			break;
		}
		if (serviceStatus.dwCurrentState == SERVICE_START_PENDING) {
			Sleep(1000);
			continue;
		}
		SetConsoleColor("red");
		cout << "服务 " << serviceName << " 无法启动：" << serviceStatus.dwCurrentState << '\n';
		break;
	}
	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);
	return (serviceStatus.dwCurrentState == SERVICE_RUNNING);
}
int main() {
	wchar_t path[MAX_PATH]{};
	GetSystemDirectoryW(path, MAX_PATH);
	SetCurrentDirectoryW(path);
	csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
	GetConsoleScreenBufferInfoEx(GetStdHandle(-11), &csbiex);
	csbiex.srWindow.Bottom += 1;
	atexit([]() { SetConsoleScreenBufferInfoEx(GetStdHandle(-11), &csbiex); });
	PrintSeparator("第一步：初始化");
	if (!IsUserAnAdmin()) {
		SetConsoleColor("red");
		cout << "请以管理员身份运行此程序。\n";
		SetConsoleColor("white");
		system("set /p = \"按任意键关闭窗口. . . \"<nul&pause>nul");
		SetConsoleColor("default");
		return -1;
	}
	if (IsWindows10OrGreater()) {
		SetConsoleColor("green");
		cout << "这是 Windows 10 或更高版本。\n";
	}
	else {
		SetConsoleColor("red");
		cout << "这不是 Windows 10 或更高版本。\n";
		SetConsoleColor("white");
		system("set /p = \"按任意键关闭窗口. . . \"<nul&pause>nul");
		SetConsoleColor("default");
		return -1;
	}
	PrintSeparator("第二步：清除信息");
	SetConsoleColor("blue");
	cout << "正在修改注册表设置. . . \n";
	HKEY hKey;
	int results = 0;
	DWORD dwValue = 1;
	RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows NT\\CurrentVersion\\Software Protection Platform", 0, KEY_WRITE, &hKey);
	RegSetValueExW(hKey, L"NoGenTicket", 0, REG_DWORD, (const BYTE*)&dwValue, sizeof(DWORD));
	results += GetLastError();
	RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\SoftwareProtectionPlatform", 0, KEY_WRITE, &hKey);
	results += GetLastError();
	if (results == 0) {
		SetConsoleColor("green");
		cout << "注册表设置已成功修改。\n";
	}
	else {
		SetConsoleColor("yellow");
		cout << "修改注册表设置失败。\n";
	}
	SetConsoleColor("blue");
	cout << "正在清除 Windows KMS 许可信息. . . \n";
	RunVBScript(L"cscript //nologo slmgr.vbs /upk");
	RunVBScript(L"cscript //nologo slmgr.vbs /cpky");
	RunVBScript(L"cscript //nologo slmgr.vbs /ckms");
	PrintSeparator("第三步：启动服务");
	results = 0;
	results += StartWindowsService("LicenseManager");
	results += StartWindowsService("wuauserv");
	if (results == 2) {
		SetConsoleColor("green");
		cout << "所有服务已成功启动。\n";
	}
	else {
		SetConsoleColor("yellow");
		cout << "部分服务无法启动。\n";
	}
	PrintSeparator("第四步：安装密钥");
	SetConsoleColor("blue");
	cout << "正在安装密钥. . . \n";
	RunVBScript(L"cscript //nologo slmgr.vbs /ipk W269N-WFGWX-YVC9B-4J6C9-T83GX");
	SetConsoleColor("blue");
	cout << "正在设置代理服务器. . . \n";
	RunVBScript(L"cscript //nologo slmgr.vbs /skms kms.loli.best");
	ShellExecuteW(GetConsoleWindow(), L"open", L"ms-settings:activation", 0, 0, SW_SHOW);
	PrintSeparator("第五步：激活系统");
	SetConsoleColor("yellow");
	cout << "正在激活 Windows，请耐心等待，不要关闭应用. . . \n";
	RunVBScript(L"cscript //nologo slmgr.vbs /ato");
	PrintSeparator();
	SetConsoleColor("white");
	system("set /p = \"按任意键关闭窗口. . . \"<nul&pause>nul");
	SetConsoleColor("default");
	return 0;
}