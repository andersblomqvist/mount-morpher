#include <iostream>
#include <cstdio>
#include <windows.h>
#include <tlhelp32.h>
#include <string>

using namespace std;

/*
	Find process ID from name

	@param processName Name of the application "Wow.exe"
*/
DWORD FindProcessId(const std::wstring& processName)
{
	PROCESSENTRY32 processInfo;
	processInfo.dwSize = sizeof(processInfo);

	HANDLE processesSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (processesSnapshot == INVALID_HANDLE_VALUE) {
		return 0;
	}

	Process32First(processesSnapshot, &processInfo);
	if (!processName.compare(processInfo.szExeFile))
	{
		CloseHandle(processesSnapshot);
		return processInfo.th32ProcessID;
	}

	while (Process32Next(processesSnapshot, &processInfo))
	{
		if (!processName.compare(processInfo.szExeFile))
		{
			CloseHandle(processesSnapshot);
			return processInfo.th32ProcessID;
		}
	}

	CloseHandle(processesSnapshot);
	return 0;
}

// Get directory path
wstring ExePath() {
	wchar_t buffer[MAX_PATH];
	GetModuleFileNameW(NULL, buffer, MAX_PATH);
	std::wstring::size_type pos = std::wstring(buffer).find_last_of(L"\\/");
	return std::wstring(buffer).substr(0, pos);
}

// wide char to multi byte:
std::string ws2s(const std::wstring& wstr)
{
	int size_needed = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), int(wstr.length() + 1), 0, 0, 0, 0);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), int(wstr.length() + 1), &strTo[0], size_needed, 0, 0);
	return strTo;
}

/*
	Simple DLL Injection

	Code from: https://github.com/Zer0Mem0ry/StandardInjection
*/
int main()
{
	// Find path to dll, which lies in the same directory as .exe
	wstring path = ExePath();
	path.append(L"\\MountMorph.dll");
	string dllpath = ws2s(path);
	LPCSTR DllPath = dllpath.c_str();

	// Find process ID from name
	DWORD procID = FindProcessId(L"Wow.exe");

	// Find and open process
	HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procID);

	// Allocate memory for the dllpath in the target process length of the path string + null terminator
	LPVOID pDllPath = VirtualAllocEx(hProc, 0, strlen(DllPath) + 1, MEM_COMMIT, PAGE_READWRITE);

	// Write the path to the address of the memory we just allocated in the target process
	WriteProcessMemory(hProc, pDllPath, (LPVOID)DllPath, strlen(DllPath) + 1, 0);

	// Create a Remote Thread in the target process which
	// calls LoadLibraryA as our dllpath as an argument -> program loads our dll
	HANDLE hLoadThread = CreateRemoteThread(hProc, 0, 0,
		(LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("Kernel32.dll"),
			"LoadLibraryA"), pDllPath, 0, 0);

	// Wait for the execution of our loader thread to finish
	WaitForSingleObject(hLoadThread, INFINITE);

	std::cout << "Dll path allocated at: " << std::hex << pDllPath << std::endl;

	// Free the memory allocated for our dll path
	VirtualFreeEx(hProc, pDllPath, strlen(DllPath) + 1, MEM_RELEASE);

	return 0;
}