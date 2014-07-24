#include "tools/profiler/elevate.h"
#include "base/logging.h"

#include <windows.h>
#include <tchar.h>
#include <Shlwapi.h>
#include <ShellAPI.h>

#ifndef ARRAY_SIZEOF
#define ARRAY_SIZEOF( a ) ( sizeof(a) / sizeof( (a)[0] ) )
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CElevate::CElevate()
{

}

CElevate::~CElevate()
{

}

//TRUE : Vista or win server 2008
//FALSE: Other
bool CElevate::IsWindowsVistaLater()
{
	BOOL bIsWindowsVista = FALSE;
	
	OSVERSIONINFO osVerInfo;
	osVerInfo.dwOSVersionInfoSize = sizeof(osVerInfo);
	if(GetVersionEx(&osVerInfo))
	{
		//6 The operating system is Windows Vista or Windows Server 2008.
		if ( osVerInfo.dwMajorVersion >= 6 )
		{
			bIsWindowsVista = TRUE;
		}
	}
	
	return bIsWindowsVista;
}

bool CElevate::IsElevated( bool * pbElevated )
{
	if( !IsWindowsVistaLater() )
	{
		if ( pbElevated)
			*pbElevated = TRUE;
		return TRUE ;
	}
	HRESULT hResult = E_FAIL; // assume an error occured
	HANDLE hToken	= NULL;
	BOOL bRet = FALSE;
	if ( !::OpenProcessToken( 
		::GetCurrentProcess(), 
		TOKEN_QUERY, 
		&hToken ) )
	{
		return FALSE ;
	}
	
	TOKEN_ELEVATION te = { 0 };
	DWORD dwReturnLength = 0;
	
	if ( !::GetTokenInformation(
		hToken,
		(_TOKEN_INFORMATION_CLASS)20,
		&te,
		sizeof( te ),
		&dwReturnLength ) )
	{
		DCHECK( FALSE );
	}
	else
	{
		DCHECK( dwReturnLength == sizeof( te ) );
		
		bRet = TRUE ; 
		
		if ( pbElevated)
			*pbElevated = (te.TokenIsElevated != 0);
	}
	
	::CloseHandle( hToken );
	
	return bRet;
}

bool CElevate::AutoElevate( void )
{
	bool bElevated = FALSE ;
	if( IsElevated( &bElevated ) )
	{
		if( bElevated == FALSE )
		{
			//选检测一下是不是已经是第二次运行的了，如果是就直接运行
			TCHAR szCmdLineTemp[MAX_PATH*2] = {0};
			TCHAR *pTemp = NULL;
			// StringCbPrintf(szCmdLineTemp, sizeof(szCmdLineTemp), _T("%s"), GetCommandLine());
			_tcsncpy(szCmdLineTemp, GetCommandLine(), ARRAY_SIZEOF(szCmdLineTemp));
			
			pTemp = StrStrI(szCmdLineTemp, _T("--elevated"));
			if( NULL == pTemp )
			{	
        TCHAR *pPos = StrStrI(szCmdLineTemp, _T("--"));
        std::wstring strCmd;
				if(pPos)
				{
					strCmd = pPos;
					strCmd += _T(" --elevated");
				}
				else
					strCmd = _T("--elevated");
				
				_tcsncpy(szCmdLineTemp, strCmd.c_str(), ARRAY_SIZEOF(szCmdLineTemp));
				
				TCHAR szFile[MAX_PATH*2] = {0};
				GetModuleFileName( NULL , szFile , sizeof(szFile) ) ;
				SHELLEXECUTEINFO TempInfo = {0};
				TempInfo.cbSize = sizeof(SHELLEXECUTEINFO);
				TempInfo.fMask = 0;
				TempInfo.hwnd = NULL;
				TempInfo.lpVerb = _T("runas");
				TempInfo.lpFile = szFile ;
				TempInfo.lpParameters = szCmdLineTemp;
				TempInfo.lpDirectory = NULL ;
				TempInfo.nShow = SW_NORMAL;
				
				::ShellExecuteEx(&TempInfo);
				return FALSE ;
			}
		}
	}
	
	return TRUE;
}
