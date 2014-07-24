#pragma once

#include <windows.h>
#include <tchar.h>

namespace lowpri
{
	// 降权启动进程
	BOOL ExecuteFileAsLowPri(LPCTSTR lpstrFile, int *nErrCode, LPCWSTR lpArg = 0);
}