#pragma once

#include <windows.h>
#include <tchar.h>

namespace lowpri
{
	// ��Ȩ��������
	BOOL ExecuteFileAsLowPri(LPCTSTR lpstrFile, int *nErrCode, LPCWSTR lpArg = 0);
}