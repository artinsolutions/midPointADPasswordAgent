#include <stdio.h>
#include <wchar.h>
#include <windows.h>
#include <ntsecapi.h>
#include <ntstatus.h>

/**
 * https://docs.microsoft.com/en-us/windows/win32/secmgmt/password-filters
 * https://docs.microsoft.com/en-us/windows/win32/secmgmt/installing-and-registering-a-password-filter-dll
 * https://docs.microsoft.com/en-us/windows/win32/secmgmt/management-functions
 */

#define UNICODE
#define _UNICODE
#define _WIN32_WINNT
#define EXPORT __declspec(dllexport)

wchar_t wcPath[MAX_PATH];

BOOL ReadPath(wchar_t *pwcPath, DWORD *pdwPath)
{
	// Read HKEY_LOCAL_MACHINE\SOFTWARE\ADPasswordFilter\Agent

	HKEY hKey;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\ADPasswordFilter", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		if (RegQueryValueExW(hKey, L"Agent", NULL, NULL, (BYTE *)pwcPath, pdwPath) == STATUS_SUCCESS)
		{
			return TRUE;
		}
	}
	RegCloseKey(hKey);
	return FALSE;
}

void Log(wchar_t *log)
{
	FILE *file = _wfopen(L"Filter.log", L"a+,ccs=UTF-8");
	if (file != NULL)
	{
		fwrite(log, sizeof(wchar_t), wcslen(log), file);
		fclose(file);
	} else {
		printf("Error writing logs\n");
	}
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	return TRUE;
}

BOOLEAN EXPORT WINAPI InitializeChangeNotify(void)
{
	wchar_t wcRawPath[MAX_PATH];
	DWORD dwPath = sizeof(wcPath);
	if (ReadPath(wcRawPath, &dwPath))
	{
		// Expand environment strings: %programfiles% -> C:\Program Files
		BOOL bRet = ExpandEnvironmentStringsW(wcRawPath, wcPath, MAX_PATH);
		if (bRet) {
			Log(L"Path: ");
			Log(wcPath);
			Log(L"\n");
		}
		else
		{
			Log(L"Error expanding environment strings\n");
		}
		return bRet;
	}
	else
	{
		Log(L"Error reading path\n");
	}
	return FALSE;
}

NTSTATUS EXPORT WINAPI PasswordChangeNotify(PUNICODE_STRING UserName, ULONG RelativeId, PUNICODE_STRING NewPassword)
{
	if (UserName == NULL && NewPassword == NULL)
	{
		Log(L"Empty UserName and Password\n");
		return STATUS_SUCCESS;
	}

	// Format: "Agent.exe" -change "Username" "Password"
	wchar_t wcArgv[2048];
	_snwprintf_s(wcArgv, sizeof(wcArgv), (sizeof(wcArgv) / 2) - 1, L"\"%s\" -change \"%s\" \"%s\"", wcPath, UserName->Buffer, NewPassword->Buffer);

	Log(L"Changing password for user: ");
	Log(UserName->Buffer);
	Log(L"\n");

	STARTUPINFOW StartupInfo = { 0 };
	StartupInfo.cb = sizeof(StartupInfo);
	StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
	StartupInfo.wShowWindow = SW_HIDE;
	PROCESS_INFORMATION ProcessInfo;

	Log(L"Calling process: ");
	Log(wcArgv);
	Log(L"\n");

	if (CreateProcessW(NULL, wcArgv, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &StartupInfo, &ProcessInfo))
	{
		Log(L"Process created\n");
		if (WaitForSingleObject(ProcessInfo.hProcess, INFINITE) == WAIT_OBJECT_0)
		{
			Log(L"Process ended\n");
		}
		CloseHandle(ProcessInfo.hProcess);
		CloseHandle(ProcessInfo.hThread);
	} else {
		Log(L"Error calling agent.\n");
	}

	SecureZeroMemory(NewPassword->Buffer, NewPassword->Length);
	SecureZeroMemory(wcArgv, sizeof(wcArgv));

	return STATUS_SUCCESS;
}

BOOLEAN EXPORT WINAPI PasswordFilter(PUNICODE_STRING AccountName, PUNICODE_STRING FullName, PUNICODE_STRING Password, BOOLEAN SetOperation)
{
	return TRUE;
}
