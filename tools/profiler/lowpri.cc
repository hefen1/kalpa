#include "tools/profiler/lowpri.h"
#include "base/logging.h"

#include <Shlwapi.h>
#include <ShellAPI.h>
#include <atltrace.h>
#include <ShlObj.h>
#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>

#ifndef ERROR_ELEVATION_REQUIRED
#define ERROR_ELEVATION_REQUIRED		740
#endif

namespace lowpri
{
	BOOL RunAsDesktopUser(__in const wchar_t * szApp,
		__in const wchar_t * szCmdLine,
		__in const wchar_t * szCurrDir,
		__in STARTUPINFOW & si,
		__inout PROCESS_INFORMATION & pi,
		__inout DWORD & dwLastErr,
		LPCWSTR lpArg = 0)
	{
		HANDLE hShellProcess = NULL, hShellProcessToken = NULL, hPrimaryToken = NULL;
		HWND hwnd = NULL;
		DWORD dwPID = 0;
		BOOL ret;

		// Enable SeIncreaseQuotaPrivilege in this process.  (This won't work if current process is not elevated.)
		HANDLE hProcessToken = NULL;
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hProcessToken))
		{
			dwLastErr = GetLastError();
			return FALSE;
		}
		else
		{
			TOKEN_PRIVILEGES tkp;
			tkp.PrivilegeCount = 1;
			LookupPrivilegeValueW(NULL, SE_INCREASE_QUOTA_NAME, &tkp.Privileges[0].Luid);
			tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
			AdjustTokenPrivileges(hProcessToken, FALSE, &tkp, 0, NULL, NULL);
			dwLastErr = GetLastError();
			CloseHandle(hProcessToken);
			if (ERROR_SUCCESS != dwLastErr)
			{
				return FALSE;
			}
		}

		// Get an HWND representing the desktop shell.
		// CAVEATS:  This will fail if the shell is not running (crashed or terminated), or the default shell has been
		// replaced with a custom shell.  This also won't return what you probably want if Explorer has been terminated and
		// restarted elevated.
		hwnd = GetShellWindow();
		if (NULL == hwnd)
		{
			return FALSE;
		}

		// Get the PID of the desktop shell process.
		GetWindowThreadProcessId(hwnd, &dwPID);
		if (0 == dwPID)
		{
			return FALSE;
		}

		// Open the desktop shell process in order to query it (get the token)
		hShellProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwPID);
		if (!hShellProcess)
		{
			dwLastErr = GetLastError();
			return FALSE;
		}

		// From this point down, we have handles to close, so make sure to clean up.

		BOOL bRetval = FALSE;
		// Get the process token of the desktop shell.
		ret = OpenProcessToken(hShellProcess, TOKEN_DUPLICATE, &hShellProcessToken);
		if (!ret)
		{
			dwLastErr = GetLastError();
			ATLTRACE(L"Can't get process token of desktop shell %d\n", dwLastErr);
			if (hShellProcessToken != NULL)
				CloseHandle(hShellProcessToken);
			if (hPrimaryToken != NULL)
				CloseHandle(hPrimaryToken);
			if (hPrimaryToken != NULL)
				CloseHandle(hShellProcess);
			return bRetval;
		}

		// Duplicate the shell's process token to get a primary token.
		// Based on experimentation, this is the minimal set of rights required for CreateProcessWithTokenW (contrary to current documentation).
		const DWORD dwTokenRights = TOKEN_QUERY | TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID;
		ret = DuplicateTokenEx(hShellProcessToken, dwTokenRights, NULL, SecurityImpersonation, TokenPrimary, &hPrimaryToken);
		if (!ret)
		{
			dwLastErr = GetLastError();
			ATLTRACE(L"Can't get primary token: %d\n", dwLastErr);
			if (hShellProcessToken != NULL)
				CloseHandle(hShellProcessToken);
			if (hPrimaryToken != NULL)
				CloseHandle(hPrimaryToken);
			if (hPrimaryToken != NULL)
				CloseHandle(hShellProcess);
			return bRetval;
		}

		// Start the target process with the new token.
		HMODULE hAdvApi32 = GetModuleHandle(L"Advapi32.dll");
		if (hAdvApi32 != NULL)
		{
			typedef BOOL (WINAPI *pFunCreateProcessWithTokenW)(
				HANDLE hToken,
				DWORD   dwLogonFlags,
				LPCWSTR lpApplicationName,
				LPWSTR lpCommandLine,
				DWORD dwCreationFlags,
				LPVOID lpEnvironment,
				LPCWSTR lpCurrentDirectory,
				LPSTARTUPINFOW lpStartupInfo,
				LPPROCESS_INFORMATION lpProcessInformation);

			pFunCreateProcessWithTokenW pFun = (pFunCreateProcessWithTokenW)GetProcAddress(hAdvApi32, "CreateProcessWithTokenW");
			if (pFun != NULL)
			{
				ret = pFun(
					hPrimaryToken,
					0,
					szApp,
					const_cast<LPWSTR>(lpArg),
					0,
					NULL,
					szCurrDir,
					&si,
					&pi);

				if (!ret)
				{
					dwLastErr = GetLastError();
					ATLTRACE(L"CreateProcessWithTokenW failed: %d\n",dwLastErr);
					if (hShellProcessToken != NULL)
						CloseHandle(hShellProcessToken);
					if (hPrimaryToken != NULL)
						CloseHandle(hPrimaryToken);
					if (hPrimaryToken != NULL)
						CloseHandle(hShellProcess);
					return bRetval;
				}

				bRetval = TRUE;
			}
		}

		if (hShellProcessToken != NULL)
			CloseHandle(hShellProcessToken);
		if (hPrimaryToken != NULL)
			CloseHandle(hPrimaryToken);
		if (hPrimaryToken != NULL)
			CloseHandle(hShellProcess);
		return bRetval;
	}

	BOOL ExecuteFileAsLowPri(LPCTSTR lpstrFile, int *nErrCode, LPCWSTR lpArg)
	{
		*nErrCode = 0;
		if (!PathFileExists(lpstrFile))
			return FALSE;

		BOOL bLaunched = FALSE;
		LPCTSTR lpszFileExt = PathFindExtension(lpstrFile);

		if ( lpszFileExt != NULL && 
			(StrCmpI(lpszFileExt, L".exe") == 0 || StrCmpI(lpszFileExt, L"msi") == 0) )
		{
			// 只对exe和msi文件尝试降权运行。
			if (IsUserAnAdmin())
			{
				// 尝试降权运行目标文件
				USES_CONVERSION;
				DWORD dwLastError = 0;
				CString strCmdLine;
				strCmdLine.Format(L"\"%s\"", lpstrFile);

				CString strDirectory(lpstrFile);
				int nPos=strDirectory.ReverseFind('\\');
				if(nPos!=-1)
				{
					strDirectory =strDirectory.Left(nPos);
				}
				
				STARTUPINFOW si;
				PROCESS_INFORMATION pi;
				SecureZeroMemory(&si, sizeof(si));
				SecureZeroMemory(&pi, sizeof(pi));
				si.cb = sizeof(si);
				if (RunAsDesktopUser(
					T2CW(lpstrFile), 
					T2CW(strCmdLine),
					strDirectory,
					si,
					pi,
					dwLastError,
					lpArg))
				{
					// Make sure to close HANDLEs return in the PROCESS_INFORMATION.
					CloseHandle(pi.hProcess);
					CloseHandle(pi.hThread);
					bLaunched = TRUE;
				}
				else
				{
					if (ERROR_ELEVATION_REQUIRED == dwLastError)
					{// 目标程序需要提权运行
						*nErrCode = -1;
						return FALSE;
					}
					else if (ERROR_ACCESS_DENIED == dwLastError)
					{
						// 无权限执行，或者被拦截了。
						return FALSE;
					}
				}
			}
		}

		if (!bLaunched)
		{
			return 0;
			//return ExecuteFileAsNormal(lpstrFile, nErrCode);
		}
		return TRUE;
	}
}
