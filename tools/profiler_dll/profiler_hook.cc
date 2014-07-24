#include "tools/profiler_dll/profiler_hook.h"

#include <windows.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/strings/StringPrintf.h"
#include "third_party/detours/src/detours.h"
#include "tools/profiler_dll/profiler_data.h"
#include "tools/profiler_dll/handle_watcher.h"

//////////////////////////////////////////////////////////////////////////

namespace{

//common
typedef HMODULE (WINAPI *MYLOADLIBRARYW)(LPCWSTR lpFileName);
typedef HMODULE (WINAPI *MYLOADLIBRARYEXW)( LPCWSTR , HANDLE , DWORD);
typedef BOOL    (WINAPI * MYCREATEPROCESSW)(LPCWSTR lpApplicationName,LPWSTR lpCommandLine,LPSECURITY_ATTRIBUTES lpProcessAttributes,LPSECURITY_ATTRIBUTES lpThreadAttributes,BOOL bInheritHandles,DWORD dwCreationFlags,LPVOID lpEnvironment,LPCWSTR lpCurrentDirectory,LPSTARTUPINFOW lpStartupInfo,LPPROCESS_INFORMATION lpProcessInformatio);
typedef LRESULT (WINAPI* MYDISPATCHMESSAGE)( const MSG *lpmsg);
typedef BOOL    (WINAPI* MYSHOWWINDOW) ( HWND hWnd , int nCmdShow);
typedef HWND    (WINAPI* MYCREATEWINDOWEXW) (DWORD dwExStyle,LPCWSTR lpClassName,LPCWSTR lpWindowName,DWORD dwStyle,int x,int y,int nWidth,int nHeight,HWND hWndParent,HMENU hMenu,HINSTANCE hInstance,LPVOID lpParam);
typedef HWND    (WINAPI* MYCREATEWINDOWW)(LPCWSTR lpClassName,LPCWSTR lpWindowName,DWORD dwStyle,int x,int y,int nWidth,int nHeight,HWND hWndParent,HMENU hMenu,HINSTANCE hInstance,LPVOID lpParam);
typedef BOOL    (WINAPI* MYSETPROCESSWORKINGSETSIZE)(HANDLE hProcess,SIZE_T dwMinimumWorkingSetSize,SIZE_T dwMaximumWorkingSetSize);
typedef DWORD   (WINAPI * MYGETPRIVATEPROFILESTRINGW)(LPCWSTR lpAppName, LPCWSTR lpKeyName,LPCWSTR lpDefault, LPWSTR lpReturnedString, DWORD nSize , LPCWSTR lpFileName);
typedef UINT    (WINAPI* MYGETPRIVATEPROFILEINTW)(LPCWSTR lpAppName, LPCWSTR lpKeyName, int nDefault, LPCWSTR lpFileName);
typedef BOOL    (WINAPI* MYSETWINDOWPOS)(HWND hWnd,HWND hWndInsertAfter,int X,int Y,int cx,int cy,UINT uFlags);
typedef BOOL    (WINAPI* MYCREATEPROCESSASUSERW)(HANDLE hToken,LPCWSTR lpApplicationName,LPWSTR lpCommandLine,LPSECURITY_ATTRIBUTES lpProcessAttributes,LPSECURITY_ATTRIBUTES lpThreadAttributes,BOOL bInheritHandles,DWORD dwCreationFlags,LPVOID lpEnvironment,LPCWSTR lpCurrentDirectory,LPSTARTUPINFOW lpStartupInfo,LPPROCESS_INFORMATION lpProcessInformation);
typedef LPWSTR  (WINAPI* MYGETCOMMANDLINEW)();
typedef LPTOP_LEVEL_EXCEPTION_FILTER (WINAPI* MYSETUNHANDLEDEXCEPTIONFILTER)(LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter);
typedef BOOL    (WINAPI* MYQUEUEUSERWORKITEM)(LPTHREAD_START_ROUTINE Function,PVOID Context,ULONG Flags);

// gdi object: http://msdn.microsoft.com/en-us/magazine/cc188782.aspx

//LoadBitmapA, LoadBitmapW, CreateBitmap, CreateBitmapIndirect, CreateCompatibleBitmap
typedef HBITMAP (WINAPI* MYLOADBITMAPA)(HINSTANCE hInstance,LPCSTR lpBitmapName);
typedef HBITMAP (WINAPI* MYLOADBITMAPW)(HINSTANCE hInstance,LPCWSTR lpBitmapName);
typedef HBITMAP (WINAPI* MYCREATEBITMAP)(int nWidth,int nHeight,UINT nPlanes,UINT nBitCount,CONST VOID *lpBits);
typedef HBITMAP (WINAPI* MYCREATEBITMAPINDIRECT)(CONST BITMAP *pbm);
typedef HBITMAP (WINAPI* MYCREATECOMPATIBLEBITMAP)(HDC hdc, int cx,int cy);

//CreateDIBitmap,CreateDIBSection,
typedef HBITMAP (WINAPI* MyCreateDIBSection)(__in_opt HDC hdc, __in CONST BITMAPINFO *lpbmi, __in UINT usage, __deref_opt_out VOID **ppvBits, __in_opt HANDLE hSection, __in DWORD offset);
typedef HBITMAP (WINAPI* MyCreateDIBitmap)( __in HDC hdc, __in_opt CONST BITMAPINFOHEADER *pbmih, __in DWORD flInit, __in_opt CONST VOID *pjBits, __in_opt CONST BITMAPINFO *pbmi, __in UINT iUsage);

//CreateBrushIndirect, CreateSolidBrush, CreatePatternBrush, CreateDIBPatternBrush, CreateDIBPatternBrushPt, CreateHatchBrush
typedef HBRUSH (WINAPI* MyCreateBrushIndirect)( __in CONST LOGBRUSH *plbrush);
typedef HBRUSH (WINAPI* MyCreateSolidBrush)(COLORREF crColor);
typedef HBRUSH (WINAPI* MyCreatePatternBrush)(HBITMAP hbmp);
typedef HBRUSH (WINAPI* MyCreateDIBPatternBrush)(HGLOBAL hglbDIBPacked,UINT fuColorSpec);
typedef HBRUSH (WINAPI* MyCreateDIBPatternBrushPt)(CONST VOID *lpPackedDIB,UINT iUsage);
typedef HBRUSH (WINAPI* MyCreateHatchBrush)(int fnStyle,COLORREF clrref);

//CreateCompatibleDC, CreateDCA, CreateDCW, CreateICA, CreateICW, GetDC, GetDCEx, GetWindowDC
typedef HDC (WINAPI* MyCreateCompatibleDC)(HDC hdc);
typedef HDC (WINAPI* MyCreateDCA)(LPCSTR lpszDriver,LPCSTR lpszDevice,LPCSTR lpszOutput,CONST DEVMODE* lpInitData);
typedef HDC (WINAPI* MyCreateDCW)(LPCWSTR lpszDriver,LPCWSTR lpszDevice,LPCWSTR lpszOutput,CONST DEVMODE* lpInitData);
typedef HDC (WINAPI* MyCreateICA)(LPCSTR lpszDriver,LPCSTR lpszDevice,LPCSTR lpszOutput,CONST DEVMODE* lpInitData);
typedef HDC (WINAPI* MyCreateICW)(LPCWSTR lpszDriver,LPCWSTR lpszDevice,LPCWSTR lpszOutput,CONST DEVMODE* lpInitData);
typedef HDC (WINAPI* MyGetDC)(HWND hWnd);
typedef HDC (WINAPI* MyGetDCEx)(HWND hWnd,HRGN hrgnClip,DWORD flags);
typedef HDC (WINAPI* MyGetWindowDC)(HWND hWnd);

//CreateFontA, CreateFontW, CreateFontIndirectA, CreateFontIndirectW
typedef HFONT (WINAPI* MyCreateFontA)(int nHeight,int nWidth,int nEscapement,int nOrientation,int fnWeight,DWORD fdwItalic,
                                      DWORD fdwUnderline,DWORD fdwStrikeOut,DWORD fdwCharSet,DWORD fdwOutputPrecision,
                                      DWORD fdwClipPrecision,DWORD fdwQuality,DWORD fdwPitchAndFamily,LPCSTR lpszFace);
typedef HFONT (WINAPI* MyCreateFontW)(int nHeight,int nWidth,int nEscapement,int nOrientation,int fnWeight,DWORD fdwItalic,
                                      DWORD fdwUnderline,DWORD fdwStrikeOut,DWORD fdwCharSet,DWORD fdwOutputPrecision,
                                      DWORD fdwClipPrecision,DWORD fdwQuality,DWORD fdwPitchAndFamily,LPCWSTR lpszFace);
typedef HFONT (WINAPI* MyCreateFontIndirectA)(CONST LOGFONTA *lplf);
typedef HFONT (WINAPI* MyCreateFontIndirectW)(CONST LOGFONTW *lplf);

//CreatePen, CreatePenIndirect, ExtCreatePen
typedef HPEN (WINAPI* MyCreatePen)( __in int iStyle, __in int cWidth, __in COLORREF color);
typedef HPEN (WINAPI* MyCreatePenIndirect)( __in CONST LOGPEN *plpen);
typedef HPEN (WINAPI* MyExtCreatePen)(__in DWORD iPenStyle,__in DWORD cWidth,__in CONST LOGBRUSH *plbrush,
                                      __in DWORD cStyle,__in_ecount_opt(cStyle) CONST DWORD *pstyle);

//PathToRegion, CreateEllipticRgn, CreateEllipticRgnIndirect, CreatePolygonRgn, CreatePolyPolygonRgn, CreateRectRgn, CreateRectRgnIndirect, CreateRoundRectRgn, ExtCreateRegion
typedef HRGN (WINAPI* MyPathToRegion)(__in HDC hdc);
typedef HRGN (WINAPI* MyCreateEllipticRgn)( __in int x1, __in int y1, __in int x2, __in int y2);
typedef HRGN (WINAPI* MyCreateEllipticRgnIndirect)( __in CONST RECT *lprect);
typedef HRGN (WINAPI* MyCreatePolygonRgn)(__in_ecount(cPoint) CONST POINT *pptl,__in int cPoint,__in int iMode);
typedef HRGN (WINAPI* MyCreatePolyPolygonRgn)(  __in CONST POINT *pptl,__in CONST INT  *pc,__in int cPoly,__in int iMode);
typedef HRGN (WINAPI* MyCreateRectRgn)( __in int x1, __in int y1, __in int x2, __in int y2);
typedef HRGN (WINAPI* MyCreateRectRgnIndirect)( __in CONST RECT *lprect);
typedef HRGN (WINAPI* MyCreateRoundRectRgn)( __in int x1, __in int y1, __in int x2, __in int y2, __in int w, __in int h);
typedef HRGN (WINAPI* MyExtCreateRegion)( __in_opt CONST XFORM * lpx, __in DWORD nCount, __in_bcount(nCount) CONST RGNDATA * lpData);

//DeleteDC,DeleteObject,ReleaseDC
typedef BOOL (WINAPI* MyDeleteDC)( __in HDC hdc);
typedef BOOL (WINAPI* MYDELETEOBJECT)(HGDIOBJ ho);
typedef int  (WINAPI* MyReleaseDC)(__in_opt HWND hWnd,__in HDC hDC);

//user object: 
//CreateIcon LoadIcon DestroyIcon
typedef HICON (WINAPI* MyCreateIcon)(HINSTANCE hInstance,int nWidth,int nHeight,BYTE cPlanes,BYTE cBitsPixel,const BYTE *lpbANDbits,const BYTE *lpbXORbits);
typedef HICON (WINAPI* MyLoadIcon)(HINSTANCE hInstance,LPCTSTR lpIconName);
typedef BOOL  (WINAPI* MyDestroyIcon)(__in HICON hIcon);

HMODULE WINAPI OnLoadLibraryW(LPCWSTR lpFileName);
HMODULE WINAPI OnLoadLibraryExW( LPCWSTR lpFileName , HANDLE hFile, DWORD dwFlags);
BOOL    WINAPI OnCreateProcessW(LPCWSTR lpApplicationName,LPWSTR lpCommandLine,LPSECURITY_ATTRIBUTES lpProcessAttributes,LPSECURITY_ATTRIBUTES lpThreadAttributes,BOOL bInheritHandles,DWORD dwCreationFlags,LPVOID lpEnvironment,LPCWSTR lpCurrentDirectory,LPSTARTUPINFOW lpStartupInfo,LPPROCESS_INFORMATION lpProcessInformatio);
LRESULT WINAPI OnDispatchMessage(const MSG* lpmsg);
BOOL    WINAPI OnShowWindow( HWND hWnd , int nCmdShow);
HWND    WINAPI OnCreateWindowExW(DWORD dwExStyle,LPCWSTR lpClassName,LPCWSTR lpWindowName,DWORD dwStyle,int x,int y,int nWidth,int nHeight,HWND hWndParent,HMENU hMenu,HINSTANCE hInstance,LPVOID lpParam); 
HWND    WINAPI OnCreateWindowW(LPCWSTR lpClassName,LPCWSTR lpWindowName,DWORD dwStyle,int x,int y,int nWidth,int nHeight,HWND hWndParent,HMENU hMenu,HINSTANCE hInstance,LPVOID lpParam);
BOOL    WINAPI OnSetProcessWorkingSetSize(HANDLE hProcess,SIZE_T dwMinimumWorkingSetSize,SIZE_T dwMaximumWorkingSetSize);
DWORD   WINAPI OnGetPrivateProfileStringW(LPCWSTR lpAppName, LPCWSTR lpKeyName,LPCWSTR lpDefault, LPWSTR lpReturnedString, DWORD nSize , LPCWSTR lpFileName);
DWORD   WINAPI OnGetPrivateProfileIntW(LPCWSTR lpAppName, LPCWSTR lpKeyName, int nDefault, LPCWSTR lpFileName);
BOOL    WINAPI OnSetWindowPos(HWND hWnd,HWND hWndInsertAfter,int X,int Y,int cx,int cy,UINT uFlags);
BOOL    WINAPI OnCreateProcessAsUserW(HANDLE hToken,LPCWSTR lpApplicationName,LPWSTR lpCommandLine,LPSECURITY_ATTRIBUTES lpProcessAttributes,LPSECURITY_ATTRIBUTES lpThreadAttributes,BOOL bInheritHandles,DWORD dwCreationFlags,LPVOID lpEnvironment,LPCWSTR lpCurrentDirectory,LPSTARTUPINFOW lpStartupInfo,LPPROCESS_INFORMATION lpProcessInformation);
LPWSTR  WINAPI OnGetCommandLineW();
LPTOP_LEVEL_EXCEPTION_FILTER WINAPI OnSetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter);
BOOL    WINAPI OnQueueUserWorkItem(LPTHREAD_START_ROUTINE Function,PVOID Context,ULONG Flags);

HBITMAP WINAPI OnLoadBitmapA(HINSTANCE hInstance,LPCSTR lpBitmapName);
HBITMAP WINAPI OnLoadBitmapW(HINSTANCE hInstance,LPCWSTR lpBitmapName);
HBITMAP WINAPI OnCreateBitmap(int nWidth,int nHeight,UINT nPlanes,UINT nBitCount,CONST VOID *lpBits);
HBITMAP WINAPI OnCreateBitmapIndirect(CONST BITMAP *pbm);
HBITMAP WINAPI OnCreateCompatibleBitmap(HDC hdc, int cx,int cy);

HBITMAP WINAPI OnCreateDIBSection(__in_opt HDC hdc, __in CONST BITMAPINFO *lpbmi, __in UINT usage, __deref_opt_out VOID **ppvBits, __in_opt HANDLE hSection, __in DWORD offset);
HBITMAP WINAPI OnCreateDIBitmap( __in HDC hdc, __in_opt CONST BITMAPINFOHEADER *pbmih, __in DWORD flInit, __in_opt CONST VOID *pjBits, __in_opt CONST BITMAPINFO *pbmi, __in UINT iUsage);
HBRUSH  WINAPI OnCreateBrushIndirect( __in CONST LOGBRUSH *plbrush);

HBRUSH WINAPI OnCreateSolidBrush(COLORREF crColor);
HBRUSH WINAPI OnCreatePatternBrush(HBITMAP hbmp);
HBRUSH WINAPI OnCreateDIBPatternBrush(HGLOBAL hglbDIBPacked,UINT fuColorSpec);
HBRUSH WINAPI OnCreateDIBPatternBrushPt(CONST VOID *lpPackedDIB,UINT iUsage);
HBRUSH WINAPI OnCreateHatchBrush(int fnStyle,COLORREF clrref);

HDC WINAPI OnCreateCompatibleDC(HDC hdc);
HDC WINAPI OnCreateDCA(LPCSTR lpszDriver,LPCSTR lpszDevice,LPCSTR lpszOutput,CONST DEVMODE* lpInitData);
HDC WINAPI OnCreateDCW(LPCWSTR lpszDriver,LPCWSTR lpszDevice,LPCWSTR lpszOutput,CONST DEVMODE* lpInitData);
HDC WINAPI OnCreateICA(LPCSTR lpszDriver,LPCSTR lpszDevice,LPCSTR lpszOutput,CONST DEVMODE* lpInitData);
HDC WINAPI OnCreateICW(LPCWSTR lpszDriver,LPCWSTR lpszDevice,LPCWSTR lpszOutput,CONST DEVMODE* lpInitData);
HDC WINAPI OnGetDC(HWND hWnd);
HDC WINAPI OnGetDCEx(HWND hWnd,HRGN hrgnClip,DWORD flags);
HDC WINAPI OnGetWindowDC(HWND hWnd);

HFONT WINAPI OnCreateFontA(int nHeight,int nWidth,int nEscapement,int nOrientation,int fnWeight,DWORD fdwItalic,
                              DWORD fdwUnderline,DWORD fdwStrikeOut,DWORD fdwCharSet,DWORD fdwOutputPrecision,
                              DWORD fdwClipPrecision,DWORD fdwQuality,DWORD fdwPitchAndFamily,LPCSTR lpszFace);
HFONT WINAPI OnCreateFontW(int nHeight,int nWidth,int nEscapement,int nOrientation,int fnWeight,DWORD fdwItalic,
                              DWORD fdwUnderline,DWORD fdwStrikeOut,DWORD fdwCharSet,DWORD fdwOutputPrecision,
                              DWORD fdwClipPrecision,DWORD fdwQuality,DWORD fdwPitchAndFamily,LPCWSTR lpszFace);
HFONT WINAPI OnCreateFontIndirectA(CONST LOGFONTA *lplf);
HFONT WINAPI OnCreateFontIndirectW(CONST LOGFONTW *lplf);

HPEN WINAPI OnCreatePen(int iStyle, int cWidth, COLORREF color);
HPEN WINAPI OnCreatePenIndirect( __in CONST LOGPEN *plpen);
HPEN WINAPI OnExtCreatePen(__in DWORD iPenStyle,__in DWORD cWidth,__in CONST LOGBRUSH *plbrush,
                              __in DWORD cStyle,__in_ecount_opt(cStyle) CONST DWORD *pstyle);

HRGN WINAPI OnPathToRegion(__in HDC hdc);
HRGN WINAPI OnCreateEllipticRgn( __in int x1, __in int y1, __in int x2, __in int y2);
HRGN WINAPI OnCreateEllipticRgnIndirect( __in CONST RECT *lprect);
HRGN WINAPI OnCreatePolygonRgn(__in_ecount(cPoint) CONST POINT *pptl,__in int cPoint,__in int iMode);
HRGN WINAPI OnCreatePolyPolygonRgn(  __in CONST POINT *pptl,__in CONST INT  *pc,__in int cPoly,__in int iMode);
HRGN WINAPI OnCreateRectRgn( __in int x1, __in int y1, __in int x2, __in int y2);
HRGN WINAPI OnCreateRectRgnIndirect( __in CONST RECT *lprect);
HRGN WINAPI OnCreateRoundRectRgn( __in int x1, __in int y1, __in int x2, __in int y2, __in int w, __in int h);
HRGN WINAPI OnExtCreateRegion( __in_opt CONST XFORM * lpx, __in DWORD nCount, __in_bcount(nCount) CONST RGNDATA * lpData);

BOOL WINAPI OnDeleteObject(HGDIOBJ ho);
BOOL WINAPI OnDeleteDC( __in HDC hdc);
int  WINAPI OnReleaseDC(__in_opt HWND hWnd,__in HDC hDC);

HICON WINAPI OnCreateIcon(HINSTANCE hInstance,int nWidth,int nHeight,BYTE cPlanes,BYTE cBitsPixel,
                             const BYTE *lpbANDbits,const BYTE *lpbXORbits);
HICON WINAPI OnLoadIcon(HINSTANCE hInstance,LPCTSTR lpIconName);
BOOL  WINAPI OnDestroyIcon(__in HICON hIcon);

MYLOADLIBRARYW      RealLoadLibraryW;
MYLOADLIBRARYEXW    RealLoadLibraryExW;
MYCREATEPROCESSW    RealCreateProcessW;
MYDISPATCHMESSAGE   RealDispatchMessage;
MYSHOWWINDOW        RealShowWindow;
MYCREATEWINDOWEXW   RealCreateWindowExW;
MYCREATEWINDOWW     RealCreateWindowW;

MYSETPROCESSWORKINGSETSIZE  RealSetProcessWorkingSetSize;
MYGETPRIVATEPROFILESTRINGW  RealGetPrivateProfileStringW;
MYGETPRIVATEPROFILEINTW     RealGetPrivateProfileIntW;
MYSETWINDOWPOS              RealSetWindowPos;
MYCREATEPROCESSASUSERW      RealCreateProcessAsUserW;
MYGETCOMMANDLINEW           RealGetCommandLineW;
MYSETUNHANDLEDEXCEPTIONFILTER RealSetUnhandledExceptionFilter;
MYQUEUEUSERWORKITEM         RealQueueUserWorkItem;

MYLOADBITMAPA               RealLoadBitmapA;
MYLOADBITMAPW               RealLoadBitmapW;
MYCREATEBITMAP              RealCreateBitmap;
MYCREATEBITMAPINDIRECT      RealCreateBitmapIndirect;
MYCREATECOMPATIBLEBITMAP    RealCreateCompatibleBitmap;
MyCreateDIBSection          RealCreateDIBSection;
MyCreateDIBitmap            RealCreateDIBitmap;

MyCreateBrushIndirect           RealCreateBrushIndirect;
MyCreateSolidBrush              RealCreateSolidBrush;
MyCreatePatternBrush            RealCreatePatternBrush;
MyCreateDIBPatternBrush         RealCreateDIBPatternBrush;
MyCreateDIBPatternBrushPt       RealCreateDIBPatternBrushPt;
MyCreateHatchBrush              RealCreateHatchBrush;

MyCreateCompatibleDC            RealCreateCompatibleDC;
MyCreateDCA                     RealCreateDCA;
MyCreateDCW                     RealCreateDCW;
MyCreateICA                     RealCreateICA;
MyCreateICW                     RealCreateICW;
MyGetDC                         RealGetDC;
MyGetDCEx                       RealGetDCEx;
MyGetWindowDC                   RealGetWindowDC;

MyCreateFontA                   RealCreateFontA;
MyCreateFontW                   RealCreateFontW;
MyCreateFontIndirectA           RealCreateFontIndirectA;
MyCreateFontIndirectW           RealCreateFontIndirectW;

MyCreatePen                     RealCreatePen;
MyCreatePenIndirect             RealCreatePenIndirect;
MyExtCreatePen                  RealExtCreatePen;

MyPathToRegion                  RealPathToRegion;
MyCreateEllipticRgn             RealCreateEllipticRgn;
MyCreateEllipticRgnIndirect     RealCreateEllipticRgnIndirect;
MyCreatePolygonRgn              RealCreatePolygonRgn;
MyCreatePolyPolygonRgn          RealCreatePolyPolygonRgn;
MyCreateRectRgn                 RealCreateRectRgn;
MyCreateRectRgnIndirect         RealCreateRectRgnIndirect;
MyCreateRoundRectRgn            RealCreateRoundRectRgn;
MyExtCreateRegion               RealExtCreateRegion;

MYDELETEOBJECT                  RealDeleteObject;
MyDeleteDC                      RealDeleteDC;
MyReleaseDC                     RealReleaseDC;

MyCreateIcon                    RealCreateIcon;
MyLoadIcon                      RealLoadIcon;
MyDestroyIcon                   RealDestroyIcon;

HandleWatcher* g_handle_watcher = NULL;
}

//////////////////////////////////////////////////////////////////////////
namespace ProfilerHook{

void Hook()
{
  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());

  //hook

  // common
  RealLoadLibraryW = (MYLOADLIBRARYW)DetourFindFunction("kernel32.dll","LoadLibraryW");
  if(RealLoadLibraryW) {
    DetourAttach(&(PVOID&)RealLoadLibraryW, OnLoadLibraryW);
  }

  RealLoadLibraryExW = (MYLOADLIBRARYEXW)DetourFindFunction("kernel32.dll", "LoadLibraryExW");
  if(RealLoadLibraryExW) {
    DetourAttach(&(PVOID&)RealLoadLibraryExW, OnLoadLibraryExW);
  }

  RealShowWindow = (MYSHOWWINDOW)DetourFindFunction("user32.dll","ShowWindow");
  if(RealShowWindow != NULL){
    DetourAttach(&(PVOID&)RealShowWindow, OnShowWindow);
  }

  RealCreateWindowExW = (MYCREATEWINDOWEXW)DetourFindFunction("user32.dll","CreateWindowExW");
  if( RealCreateWindowExW != NULL){
    DetourAttach(&(PVOID&)RealCreateWindowExW,OnCreateWindowExW);
  }

  RealCreateWindowW = (MYCREATEWINDOWW)DetourFindFunction("user32.dll","CreateWindowW");
  if(RealCreateWindowW != NULL){
    DetourAttach(&(PVOID&)RealCreateWindowW ,OnCreateWindowW);
  }

  RealDispatchMessage = (MYDISPATCHMESSAGE)DetourFindFunction("user32.dll","DispatchMessageW");
  if( RealDispatchMessage != NULL){
    DetourAttach(&(PVOID&)RealDispatchMessage ,OnDispatchMessage);
  }

  RealSetWindowPos = (MYSETWINDOWPOS)DetourFindFunction("user32.dll","SetWindowPos");
  if( RealSetWindowPos != NULL){
    DetourAttach(&(PVOID&)RealSetWindowPos ,OnSetWindowPos);
  }

  RealCreateProcessW = (MYCREATEPROCESSW)DetourFindFunction("kernel32.dll", "CreateProcessW");
  if( RealCreateProcessW != NULL){
    DetourAttach(&(PVOID&)RealCreateProcessW, OnCreateProcessW);
  }

  RealCreateProcessAsUserW = (MYCREATEPROCESSASUSERW)DetourFindFunction("advapi32.dll", "CreateProcessAsUserW");
  if( RealCreateProcessAsUserW != NULL){
    DetourAttach(&(PVOID&)RealCreateProcessAsUserW, OnCreateProcessAsUserW);
  }

  //便于定位ChromeMain和WinMain里面的代码块
  RealGetCommandLineW = (MYGETCOMMANDLINEW)DetourFindFunction("kernel32.dll", "GetCommandLineW");
  if( RealGetCommandLineW != NULL){
    DetourAttach(&(PVOID&)RealGetCommandLineW, OnGetCommandLineW);
  }

  RealSetUnhandledExceptionFilter = (MYSETUNHANDLEDEXCEPTIONFILTER)DetourFindFunction("kernel32.dll", "SetUnhandledExceptionFilter");
  if( RealSetUnhandledExceptionFilter != NULL){
    DetourAttach(&(PVOID&)RealSetUnhandledExceptionFilter, OnSetUnhandledExceptionFilter);
  }

  RealQueueUserWorkItem = (MYQUEUEUSERWORKITEM)DetourFindFunction("kernel32.dll", "QueueUserWorkItem");
  if( RealQueueUserWorkItem != NULL){
    DetourAttach(&(PVOID&)RealQueueUserWorkItem, OnQueueUserWorkItem);
  }

  // gdi
  g_handle_watcher = new HandleWatcher();

  RealLoadBitmapA = (MYLOADBITMAPA)DetourFindFunction("user32.dll","LoadBitmapA");
  if(RealLoadBitmapA!=NULL){
    DetourAttach(&(PVOID&)RealLoadBitmapA,OnLoadBitmapA);
  }

  RealLoadBitmapW = (MYLOADBITMAPW)DetourFindFunction("user32.dll","LoadBitmapW");
  if(RealLoadBitmapW!=NULL){
    DetourAttach(&(PVOID&)RealLoadBitmapW,OnLoadBitmapW);
  }

  RealCreateBitmap = (MYCREATEBITMAP)DetourFindFunction("gdi32.dll","CreateBitmap");
  if(RealCreateBitmap!=NULL){
    DetourAttach(&(PVOID&)RealCreateBitmap,OnCreateBitmap);
  }

  RealCreateBitmapIndirect = (MYCREATEBITMAPINDIRECT)DetourFindFunction("gdi32.dll","CreateBitmapIndirect");
  if(RealCreateBitmapIndirect!=NULL){
    DetourAttach(&(PVOID&)RealCreateBitmapIndirect,OnCreateBitmapIndirect);
  }

  RealCreateCompatibleBitmap = (MYCREATECOMPATIBLEBITMAP)DetourFindFunction("gdi32.dll","CreateCompatibleBitmap");
  if(RealCreateCompatibleBitmap!=NULL){
    DetourAttach(&(PVOID&)RealCreateCompatibleBitmap,OnCreateCompatibleBitmap);
  }

  RealCreateDIBitmap = (MyCreateDIBitmap)DetourFindFunction("gdi32.dll","CreateDIBitmap");
  if(RealCreateDIBitmap!=NULL){
    DetourAttach(&(PVOID&)RealCreateDIBitmap,OnCreateDIBitmap);
  }

  RealCreateDIBSection = (MyCreateDIBSection)DetourFindFunction("gdi32.dll","CreateDIBSection");
  if(RealCreateDIBSection!=NULL){
    DetourAttach(&(PVOID&)RealCreateDIBSection,OnCreateDIBSection);
  }

  RealCreateBrushIndirect = (MyCreateBrushIndirect)DetourFindFunction("gdi32.dll","CreateBrushIndirect");
  if(RealCreateBrushIndirect!=NULL){
    DetourAttach(&(PVOID&)RealCreateBrushIndirect,OnCreateBrushIndirect);
  }

  RealCreateSolidBrush = (MyCreateSolidBrush)DetourFindFunction("gdi32.dll","CreateSolidBrush");
  if(RealCreateSolidBrush != NULL) {
    DetourAttach(&(PVOID&)RealCreateSolidBrush,OnCreateSolidBrush);
  }

  RealCreatePatternBrush = (MyCreatePatternBrush)DetourFindFunction("gdi32.dll","CreatePatternBrush");
  if(RealCreatePatternBrush != NULL) {
    DetourAttach(&(PVOID&)RealCreatePatternBrush,OnCreatePatternBrush);
  }

  RealCreateDIBPatternBrush = (MyCreateDIBPatternBrush)DetourFindFunction("gdi32.dll","CreateDIBPatternBrush");
  if(RealCreateDIBPatternBrush != NULL) {
    DetourAttach(&(PVOID&)RealCreateDIBPatternBrush,OnCreateDIBPatternBrush);
  }

  RealCreateDIBPatternBrushPt = (MyCreateDIBPatternBrushPt)DetourFindFunction("gdi32.dll","CreateDIBPatternBrushPt");
  if(RealCreateDIBPatternBrushPt != NULL) {
    DetourAttach(&(PVOID&)RealCreateDIBPatternBrushPt,OnCreateDIBPatternBrushPt);
  }

  RealCreateHatchBrush = (MyCreateHatchBrush)DetourFindFunction("gdi32.dll","CreateHatchBrush");
  if(RealCreateHatchBrush != NULL) {
    DetourAttach(&(PVOID&)RealCreateHatchBrush,OnCreateHatchBrush);
  }

  RealCreateCompatibleDC = (MyCreateCompatibleDC)DetourFindFunction("gdi32.dll","CreateCompatibleDC");
  if(RealCreateCompatibleDC != NULL) {
    DetourAttach(&(PVOID&)RealCreateCompatibleDC,OnCreateCompatibleDC);
  }

  RealCreateDCA = (MyCreateDCA)DetourFindFunction("gdi32.dll","CreateDCA");
  if(RealCreateDCA != NULL) {
    DetourAttach(&(PVOID&)RealCreateDCA,OnCreateDCA);
  }

  RealCreateDCW = (MyCreateDCW)DetourFindFunction("gdi32.dll","CreateDCW");
  if(RealCreateDCW != NULL) {
    DetourAttach(&(PVOID&)RealCreateDCW,OnCreateDCW);
  }

  RealCreateICA = (MyCreateICA)DetourFindFunction("gdi32.dll","CreateICA");
  if(RealCreateICA != NULL) {
    DetourAttach(&(PVOID&)RealCreateICA,OnCreateICA);
  }

  RealCreateICW = (MyCreateICW)DetourFindFunction("gdi32.dll","CreateICW");
  if(RealCreateICW != NULL) {
    DetourAttach(&(PVOID&)RealCreateICW,OnCreateICW);
  }

  RealGetDC = (MyGetDC)DetourFindFunction("user32.dll","GetDC");
  if(RealGetDC != NULL) {
    DetourAttach(&(PVOID&)RealGetDC,OnGetDC);
  }

  RealGetDCEx = (MyGetDCEx)DetourFindFunction("user32.dll","GetDCEx");
  if(RealGetDCEx != NULL) {
    DetourAttach(&(PVOID&)RealGetDCEx,OnGetDCEx);
  }

  RealGetWindowDC = (MyGetWindowDC)DetourFindFunction("user32.dll","GetWindowDC");
  if(RealGetWindowDC != NULL) {
    DetourAttach(&(PVOID&)RealGetWindowDC,OnGetWindowDC);
  }

  RealCreateFontA = (MyCreateFontA)DetourFindFunction("gdi32.dll","CreateFontA");
  if(RealCreateFontA != NULL) {
    DetourAttach(&(PVOID&)RealCreateFontA,OnCreateFontA);
  }

  RealCreateFontW = (MyCreateFontW)DetourFindFunction("gdi32.dll","CreateFontW");
  if(RealCreateFontW != NULL) {
    DetourAttach(&(PVOID&)RealCreateFontW,OnCreateFontW);
  }

  RealCreateFontIndirectW = (MyCreateFontIndirectW)DetourFindFunction("gdi32.dll","CreateFontIndirectW");
  if(RealCreateFontIndirectW != NULL) {
    DetourAttach(&(PVOID&)RealCreateFontIndirectW,OnCreateFontIndirectW);
  }

  RealCreatePen = (MyCreatePen)DetourFindFunction("gdi32.dll","CreatePen");
  if(RealCreatePen != NULL) {
    DetourAttach(&(PVOID&)RealCreatePen,OnCreatePen);
  }

  RealCreatePenIndirect = (MyCreatePenIndirect)DetourFindFunction("gdi32.dll","CreatePenIndirect");
  if(RealCreatePenIndirect != NULL) {
    DetourAttach(&(PVOID&)RealCreatePenIndirect,OnCreatePenIndirect);
  }

  RealExtCreatePen = (MyExtCreatePen)DetourFindFunction("gdi32.dll","ExtCreatePen");
  if(RealExtCreatePen != NULL) {
    DetourAttach(&(PVOID&)RealExtCreatePen,OnExtCreatePen);
  }

  RealPathToRegion = (MyPathToRegion)DetourFindFunction("gdi32.dll","PathToRegion");
  if(RealPathToRegion != NULL) {
    DetourAttach(&(PVOID&)RealPathToRegion,OnPathToRegion);
  }

  RealCreateEllipticRgn = (MyCreateEllipticRgn)DetourFindFunction("gdi32.dll","CreateEllipticRgn");
  if(RealCreateEllipticRgn != NULL) {
    DetourAttach(&(PVOID&)RealCreateEllipticRgn,OnCreateEllipticRgn);
  }

  RealCreateEllipticRgnIndirect = (MyCreateEllipticRgnIndirect)DetourFindFunction("gdi32.dll","CreateEllipticRgnIndirect");
  if(RealCreateEllipticRgnIndirect != NULL) {
    DetourAttach(&(PVOID&)RealCreateEllipticRgnIndirect,OnCreateEllipticRgnIndirect);
  }

  RealCreatePolygonRgn = (MyCreatePolygonRgn)DetourFindFunction("gdi32.dll","CreatePolygonRgn");
  if(RealCreatePolygonRgn != NULL) {
    DetourAttach(&(PVOID&)RealCreatePolygonRgn,OnCreatePolygonRgn);
  }

  RealCreatePolyPolygonRgn = (MyCreatePolyPolygonRgn)DetourFindFunction("gdi32.dll","CreatePolyPolygonRgn");
  if(RealCreatePolyPolygonRgn != NULL) {
    DetourAttach(&(PVOID&)RealCreatePolyPolygonRgn,OnCreatePolyPolygonRgn);
  }

  RealCreateRectRgn = (MyCreateRectRgn)DetourFindFunction("gdi32.dll","CreateRectRgn");
  if(RealCreateRectRgn != NULL) {
    DetourAttach(&(PVOID&)RealCreateRectRgn,OnCreateRectRgn);
  }

  RealCreateRectRgnIndirect = (MyCreateRectRgnIndirect)DetourFindFunction("gdi32.dll","CreateRectRgnIndirect");
  if(RealCreateRectRgnIndirect != NULL) {
    DetourAttach(&(PVOID&)RealCreateRectRgnIndirect,OnCreateRectRgnIndirect);
  }

  RealCreateRoundRectRgn = (MyCreateRoundRectRgn)DetourFindFunction("gdi32.dll","CreateRoundRectRgn");
  if(RealCreateRoundRectRgn != NULL) {
    DetourAttach(&(PVOID&)RealCreateRoundRectRgn,OnCreateRoundRectRgn);
  }

  RealExtCreateRegion = (MyExtCreateRegion)DetourFindFunction("gdi32.dll","ExtCreateRegion");
  if(RealExtCreateRegion != NULL) {
    DetourAttach(&(PVOID&)RealExtCreateRegion,OnExtCreateRegion);
  }

  RealDeleteObject = (MYDELETEOBJECT)DetourFindFunction("gdi32.dll","DeleteObject");
  if(RealDeleteObject!=NULL){
    DetourAttach(&(PVOID&)RealDeleteObject,OnDeleteObject);
  }

  RealDeleteDC = (MyDeleteDC)DetourFindFunction("gdi32.dll","DeleteDC");
  if(RealDeleteDC!=NULL){
    DetourAttach(&(PVOID&)RealDeleteDC,OnDeleteDC);
  }

  RealReleaseDC = (MyReleaseDC)DetourFindFunction("user32.dll","ReleaseDC");
  if(RealReleaseDC!=NULL){
    DetourAttach(&(PVOID&)RealReleaseDC,OnReleaseDC);
  }
/*
  RealCreateIcon = (MyCreateIcon)DetourFindFunction("user32.dll","CreateIcon");
  if(RealCreateIcon != NULL) {
    DetourAttach(&(PVOID&)RealCreateIcon,OnCreateIcon);
  }

  RealLoadIcon = (MyLoadIcon)DetourFindFunction("user32.dll","LoadIcon");
  if(RealLoadIcon != NULL) {
    DetourAttach(&(PVOID&)RealLoadIcon,OnLoadIcon);
  }

  RealDestroyIcon = (MyDestroyIcon)DetourFindFunction("user32.dll","DestroyIcon");
  if(RealDestroyIcon != NULL) {
    DetourAttach(&(PVOID&)RealDestroyIcon,OnDestroyIcon);
  }
*/
  DetourTransactionCommit();
}

void Unhook()
{
  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());

  //unhook

  // gdi
  if(RealLoadBitmapA!=NULL){
    DetourAttach(&(PVOID&)RealLoadBitmapA,OnLoadBitmapA);
    RealLoadBitmapA = NULL;
  }

  if(RealLoadBitmapW!=NULL){
    DetourAttach(&(PVOID&)RealLoadBitmapW,OnLoadBitmapW);
    RealLoadBitmapW = NULL;
  }

  if(RealCreateBitmap!=NULL){
    DetourAttach(&(PVOID&)RealCreateBitmap,OnCreateBitmap);
    RealCreateBitmap = NULL;
  }

  if(RealCreateBitmapIndirect!=NULL){
    DetourAttach(&(PVOID&)RealCreateBitmapIndirect,OnCreateBitmapIndirect);
    RealCreateBitmapIndirect = NULL;
  }

  if(RealCreateCompatibleBitmap!=NULL){
    DetourAttach(&(PVOID&)RealCreateCompatibleBitmap,OnCreateCompatibleBitmap);
    RealCreateCompatibleBitmap = NULL;
  }

  if(RealCreateDIBitmap!=NULL){
    DetourAttach(&(PVOID&)RealCreateDIBitmap,OnCreateDIBitmap);
    RealCreateDIBitmap = NULL;
  }

  if(RealCreateDIBSection!=NULL){
    DetourAttach(&(PVOID&)RealCreateDIBSection,OnCreateDIBSection);
    RealCreateDIBSection = NULL;
  }

  if(RealCreateBrushIndirect!=NULL){
    DetourAttach(&(PVOID&)RealCreateBrushIndirect,OnCreateBrushIndirect);
    RealCreateBrushIndirect = NULL;
  }

  if(RealCreateSolidBrush) {
    DetourDetach(&(PVOID&)RealCreateSolidBrush,OnCreateSolidBrush);
    RealCreateSolidBrush = 0;
  }

  if(RealCreatePatternBrush) {
    DetourDetach(&(PVOID&)RealCreatePatternBrush,OnCreatePatternBrush);
    RealCreatePatternBrush = 0;
  }

  if(RealCreateDIBPatternBrush) {
    DetourDetach(&(PVOID&)RealCreateDIBPatternBrush,OnCreateDIBPatternBrush);
    RealCreateDIBPatternBrush = 0;
  }

  if(RealCreateDIBPatternBrushPt) {
    DetourDetach(&(PVOID&)RealCreateDIBPatternBrushPt,OnCreateDIBPatternBrushPt);
    RealCreateDIBPatternBrushPt = 0;
  }

  if(RealCreateHatchBrush) {
    DetourDetach(&(PVOID&)RealCreateHatchBrush,OnCreateHatchBrush);
    RealCreateHatchBrush = 0;
  }

  if(RealCreateCompatibleDC) {
    DetourDetach(&(PVOID&)RealCreateCompatibleDC,OnCreateCompatibleDC);
    RealCreateCompatibleDC = 0;
  }

  if(RealCreateDCA) {
    DetourDetach(&(PVOID&)RealCreateDCA,OnCreateDCA);
    RealCreateDCA = 0;
  }

  if(RealCreateDCW) {
    DetourDetach(&(PVOID&)RealCreateDCW,OnCreateDCW);
    RealCreateDCW = 0;
  }

  if(RealCreateICA) {
    DetourDetach(&(PVOID&)RealCreateICA,OnCreateICA);
    RealCreateICA = 0;
  }

  if(RealCreateICW) {
    DetourDetach(&(PVOID&)RealCreateICW,OnCreateICW);
    RealCreateICW = 0;
  }

  if(RealGetDC) {
    DetourDetach(&(PVOID&)RealGetDC,OnGetDC);
    RealGetDC = 0;
  }

  if(RealGetDCEx) {
    DetourDetach(&(PVOID&)RealGetDCEx,OnGetDCEx);
    RealGetDCEx = 0;
  }

  if(RealGetWindowDC) {
    DetourDetach(&(PVOID&)RealGetWindowDC,OnGetWindowDC);
    RealGetWindowDC = 0;
  }

  if(RealCreateFontA) {
    DetourDetach(&(PVOID&)RealCreateFontA,OnCreateFontA);
    RealCreateFontA = 0;
  }

  if(RealCreateFontW) {
    DetourDetach(&(PVOID&)RealCreateFontW,OnCreateFontW);
    RealCreateFontW = 0;
  }

  if(RealCreateFontIndirectW) {
    DetourDetach(&(PVOID&)RealCreateFontIndirectW,OnCreateFontIndirectW);
    RealCreateFontIndirectW = 0;
  }

  if(RealCreatePen) {
    DetourDetach(&(PVOID&)RealCreatePen,OnCreatePen);
    RealCreatePen = 0;
  }

  if(RealCreatePenIndirect) {
    DetourDetach(&(PVOID&)RealCreatePenIndirect,OnCreatePenIndirect);
    RealCreatePenIndirect = 0;
  }

  if(RealExtCreatePen) {
    DetourDetach(&(PVOID&)RealExtCreatePen,OnExtCreatePen);
    RealExtCreatePen = 0;
  }

  if(RealPathToRegion) {
    DetourDetach(&(PVOID&)RealPathToRegion,OnPathToRegion);
    RealPathToRegion = 0;
  }

  if(RealCreateEllipticRgn) {
    DetourDetach(&(PVOID&)RealCreateEllipticRgn,OnCreateEllipticRgn);
    RealCreateEllipticRgn = 0;
  }

  if(RealCreateEllipticRgnIndirect) {
    DetourDetach(&(PVOID&)RealCreateEllipticRgnIndirect,OnCreateEllipticRgnIndirect);
    RealCreateEllipticRgnIndirect = 0;
  }

  if(RealCreatePolygonRgn) {
    DetourDetach(&(PVOID&)RealCreatePolygonRgn,OnCreatePolygonRgn);
    RealCreatePolygonRgn = 0;
  }

  if(RealCreatePolyPolygonRgn) {
    DetourDetach(&(PVOID&)RealCreatePolyPolygonRgn,OnCreatePolyPolygonRgn);
    RealCreatePolyPolygonRgn = 0;
  }

  if(RealCreateRectRgn) {
    DetourDetach(&(PVOID&)RealCreateRectRgn,OnCreateRectRgn);
    RealCreateRectRgn = 0;
  }

  if(RealCreateRectRgnIndirect) {
    DetourDetach(&(PVOID&)RealCreateRectRgnIndirect,OnCreateRectRgnIndirect);
    RealCreateRectRgnIndirect = 0;
  }

  if(RealCreateRoundRectRgn) {
    DetourDetach(&(PVOID&)RealCreateRoundRectRgn,OnCreateRoundRectRgn);
    RealCreateRoundRectRgn = 0;
  }

  if(RealExtCreateRegion) {
    DetourDetach(&(PVOID&)RealExtCreateRegion,OnExtCreateRegion);
    RealExtCreateRegion = 0;
  }

  if(RealDeleteObject!=NULL){
    DetourAttach(&(PVOID&)RealDeleteObject,OnDeleteObject);
    RealDeleteObject = NULL;
  }

  if(RealDeleteDC!=NULL){
    DetourAttach(&(PVOID&)RealDeleteDC,OnDeleteDC);
    RealDeleteDC = NULL;
  }

  if(RealReleaseDC!=NULL){
    DetourAttach(&(PVOID&)RealReleaseDC,OnReleaseDC);
    RealReleaseDC = NULL;
  }
/*
  if(RealCreateIcon) {
    DetourDetach(&(PVOID&)RealCreateIcon,OnCreateIcon);
    RealCreateIcon = 0;
  }

  if(RealLoadIcon) {
    DetourDetach(&(PVOID&)RealLoadIcon,OnLoadIcon);
    RealLoadIcon = 0;
  }

  if(RealDestroyIcon){
    DetourDetach(&(PVOID&)RealDestroyIcon,OnDestroyIcon);
    RealDestroyIcon = 0;
  }
*/
  delete g_handle_watcher;

  // common
  if(RealQueueUserWorkItem != NULL){
    DetourDetach(&(PVOID&)RealQueueUserWorkItem, OnQueueUserWorkItem);
    RealQueueUserWorkItem = NULL;
  }

  if(RealShowWindow != NULL){
    DetourDetach(&(PVOID&)RealShowWindow, OnShowWindow);
    RealShowWindow = NULL;
  }

  if(RealCreateWindowExW != NULL){
    DetourDetach(&(PVOID&)RealCreateWindowExW,OnCreateWindowExW);
    RealCreateWindowExW = NULL;
  }
  
  if(RealCreateWindowW != NULL){
    DetourDetach(&(PVOID&)RealCreateWindowW ,OnCreateWindowW);
    RealCreateWindowW = NULL;
  }

  if( RealDispatchMessage){
    DetourDetach(&(PVOID&)RealDispatchMessage,OnDispatchMessage);
    RealDispatchMessage = 0;
  }

  if( RealSetWindowPos){
    DetourDetach(&(PVOID&)RealSetWindowPos,OnSetWindowPos);
    RealSetWindowPos = 0;
  }

  if( RealCreateProcessW ){
    DetourDetach(&(PVOID&)RealCreateProcessW,OnCreateProcessW);
    RealCreateProcessW = 0;
  }

  if( RealCreateProcessAsUserW ){
    DetourDetach(&(PVOID&)RealCreateProcessAsUserW,OnCreateProcessAsUserW);
    RealCreateProcessAsUserW = 0;
  }

  if( RealGetCommandLineW ){
    DetourDetach(&(PVOID&)RealGetCommandLineW,OnGetCommandLineW);
    RealGetCommandLineW = 0;
  }

  if( RealSetUnhandledExceptionFilter ){
    DetourDetach(&(PVOID&)RealSetUnhandledExceptionFilter,OnSetUnhandledExceptionFilter);
    RealSetUnhandledExceptionFilter = 0;
  }

  if(RealLoadLibraryExW ){
    DetourDetach(&(PVOID&)RealLoadLibraryExW,OnLoadLibraryExW);
    RealLoadLibraryExW = 0;
  }

  if(RealLoadLibraryW ){
    DetourDetach(&(PVOID&)RealLoadLibraryW,OnLoadLibraryW);
    RealLoadLibraryW = 0;
  }

  DetourTransactionCommit();
}


} // namespace ProfilerHook

//////////////////////////////////////////////////////////////////////////

namespace{

HMODULE WINAPI OnLoadLibraryW(LPCWSTR lpFileName)
{
  HMODULE hRet = NULL;
  if(!RealLoadLibraryW)
    return hRet;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  hRet =  RealLoadLibraryW(lpFileName);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();

  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"LoadLibraryW(%03d)",++count),lpFileName);

  return hRet;
}

HMODULE WINAPI OnLoadLibraryExW( LPCWSTR lpFileName , HANDLE hFile, DWORD dwFlags)
{
  HMODULE hRet = NULL;
  if(!RealLoadLibraryExW)
    return hRet;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  hRet =  RealLoadLibraryExW(lpFileName,hFile,dwFlags);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();

  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"LoadLibraryExW(%03d)",++count),lpFileName);

  return hRet;
}

BOOL WINAPI OnShowWindow( HWND hWnd , int nCmdShow)
{
  if(RealShowWindow == NULL)
    return FALSE;

  BOOL bRet = 0;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  bRet = RealShowWindow( hWnd , nCmdShow) ;
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();

  if( hWnd != NULL && nCmdShow!= 0){
    TCHAR szWndClassName[MAX_PATH];
    TCHAR szWndText[MAX_PATH];

    GetClassName(hWnd , szWndClassName , MAX_PATH);
    GetWindowText(hWnd, szWndText , MAX_PATH);

    static int count = 0;
    ProfilerData::Action(ticks_start,ticks_end - ticks_start,
      GetCurrentProcessId(),GetCurrentThreadId(),
      base::StringPrintf(L"ShowWindow(%03d)",++count),base::StringPrintf(L"hWnd=%08x,nCmdShow=%08x,lpClassName=%ls,lpWindowName=%ls",
      hWnd,nCmdShow,szWndClassName,szWndText));
  }

  return bRet;

};

HWND WINAPI OnCreateWindowExW(DWORD dwExStyle,LPCWSTR lpClassName,LPCWSTR lpWindowName,DWORD dwStyle,int x,int y,int nWidth,int nHeight,HWND hWndParent,HMENU hMenu,HINSTANCE hInstance,LPVOID lpParam)
{
  HWND hRet = NULL;
  if( RealCreateWindowExW== NULL)
    return NULL;
  
  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  hRet = RealCreateWindowExW(dwExStyle,lpClassName,lpWindowName,dwStyle,x,y,nWidth,nHeight,hWndParent,hMenu,hInstance,lpParam);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();

  static int count = 0;
  if( HIWORD( lpClassName) != 0){
    ProfilerData::Action(ticks_start,ticks_end - ticks_start,
      GetCurrentProcessId(),GetCurrentThreadId(),
      base::StringPrintf(L"CreateWindowExW(%03d)",++count),base::StringPrintf(L"hWnd=%08x,lpClassName=%ls,lpWindowName=%ls",hRet,lpClassName,lpWindowName));
  }else{
    TCHAR className[128] = {};
    GetClassName(hRet, className, _countof(className));
    ProfilerData::Action(ticks_start,ticks_end - ticks_start,
      GetCurrentProcessId(),GetCurrentThreadId(),
      base::StringPrintf(L"CreateWindowExW(%03d)",++count),base::StringPrintf(L"hWnd=%08x,atom=%04x(%ls),lpWindowName=%ls",hRet,LOWORD(lpClassName),className, lpWindowName));
  }

  return hRet;
};

HWND WINAPI OnCreateWindowW(LPCWSTR lpClassName,LPCWSTR lpWindowName,DWORD dwStyle,int x,int y,int nWidth,int nHeight,HWND hWndParent,HMENU hMenu,HINSTANCE hInstance,LPVOID lpParam)
{
  if( RealCreateWindowW == NULL)
    return NULL;
  
  HWND hRet= 0 ;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  hRet = RealCreateWindowW(lpClassName,lpWindowName,dwStyle,x,y,nWidth,nHeight,hWndParent,hMenu,hInstance,lpParam);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();

  static int count = 0;
  if( HIWORD( lpClassName) != 0){
    ProfilerData::Action(ticks_start,ticks_end - ticks_start,
      GetCurrentProcessId(),GetCurrentThreadId(),
      base::StringPrintf(L"CreateWindowW(%03d)",++count),base::StringPrintf(L"hWnd=%08x,lpClassName=%ls,lpWindowName=%ls",hRet,lpClassName,lpWindowName));
  }else{
    TCHAR className[128] = {};
    GetClassName(hRet, className, _countof(className));
    ProfilerData::Action(ticks_start,ticks_end - ticks_start,
      GetCurrentProcessId(),GetCurrentThreadId(),
      base::StringPrintf(L"CreateWindowW(%03d)",++count),base::StringPrintf(L"hWnd=%08x,atom=%04x(%ls),lpWindowName=%ls",hRet,LOWORD(lpClassName),className, lpWindowName));
  }

  return hRet;
};

// 由于WM_ERASEBKGND由BeginPaint触发，因此可能无法通过DispatchMessage获得
LRESULT WINAPI OnDispatchMessage(const MSG* lpmsg)
{
  if( RealDispatchMessage == NULL)
    return FALSE;
  
  LRESULT lRet = 0;

  if( lpmsg && lpmsg->hwnd &&
    (lpmsg->message == WM_PAINT )
    )
  {
    TCHAR szWndClassName[MAX_PATH];
    TCHAR szWndText[MAX_PATH];

    GetClassName(lpmsg->hwnd , szWndClassName , MAX_PATH);
    GetWindowText(lpmsg->hwnd, szWndText , MAX_PATH);

    int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
    lRet = RealDispatchMessage( lpmsg);
    int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();

    static int count = 0;
    if(count > 10)
      return lRet; //只输出开始的10次
    ProfilerData::Action(ticks_start,ticks_end - ticks_start,
      GetCurrentProcessId(),GetCurrentThreadId(),
      base::StringPrintf(L"WM_PAINT(%03d)",++count),base::StringPrintf(L"hWnd=%08x,lpClassName=%ls,lpWindowName=%ls",lpmsg->hwnd,szWndClassName,szWndText));

    return lRet;
  }
  else{
    return RealDispatchMessage( lpmsg);
  }
};

BOOL    WINAPI OnSetWindowPos(HWND hWnd,HWND hWndInsertAfter,int X,int Y,int cx,int cy,UINT uFlags)
{
  if(RealSetWindowPos == NULL){
    return FALSE;
  }

  BOOL bRet = FALSE;
  if(hWnd){
    TCHAR szWndClassName[MAX_PATH];
    TCHAR szWndText[MAX_PATH];

    GetClassName(hWnd , szWndClassName , MAX_PATH);
    GetWindowText(hWnd, szWndText , MAX_PATH);

    int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
    bRet = RealSetWindowPos(hWnd,hWndInsertAfter,X,Y,cx,cy,uFlags);
    int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
/*
    static int count = 0;
    ProfilerData::Action(ticks_start,ticks_end - ticks_start,
      GetCurrentProcessId(),GetCurrentThreadId(),
      base::StringPrintf(L"SetWindowPos(%03d)",++count),base::StringPrintf(L"hWnd=%08x,lpClassName=%ls,lpWindowName=%ls,X=%04d,Y=%04d,cx=%04d,cy=%04d,uflags=%08x",
      hWnd,szWndClassName,szWndText,X,Y,cx,cy,uFlags));
*/
    return bRet;
  }
  else{
    return RealSetWindowPos(hWnd,hWndInsertAfter,X,Y,cx,cy,uFlags);
  }
}

// TODO:
// 拦掉目标进程的CreateProcess , 
// 保证将自己继续注入进去
// profiler.exe创建为创建的进程创建一个logdaemon-channel，用pid。
// 目标进程的logclient监听channel通过pid，profiler.exe由创建者复制提供此pid。
// 
BOOL WINAPI OnCreateProcessW(LPCWSTR lpApplicationName,
                             LPWSTR lpCommandLine,
                             LPSECURITY_ATTRIBUTES lpProcessAttributes,
                             LPSECURITY_ATTRIBUTES lpThreadAttributes,
                             BOOL bInheritHandles,
                             DWORD dwCreationFlags,
                             LPVOID lpEnvironment,
                             LPCWSTR lpCurrentDirectory,
                             LPSTARTUPINFOW lpStartupInfo,
                             LPPROCESS_INFORMATION lpProcessInformatio)
{
  if( RealCreateProcessW == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  BOOL bRet =  RealCreateProcessW(lpApplicationName,
    lpCommandLine,
    lpProcessAttributes,
    lpThreadAttributes,
    bInheritHandles,
    dwCreationFlags,
    lpEnvironment,
    lpCurrentDirectory,
    lpStartupInfo,
    lpProcessInformatio);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();

  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"CreateProcessW(%03d)",++count),base::StringPrintf(L"lpCommandLine=%ls",lpCommandLine));

  return bRet;
};

BOOL WINAPI OnCreateProcessAsUserW(HANDLE hToken,
                                   LPCWSTR lpApplicationName,
                                   LPWSTR lpCommandLine,
                                   LPSECURITY_ATTRIBUTES lpProcessAttributes,
                                   LPSECURITY_ATTRIBUTES lpThreadAttributes,
                                   BOOL bInheritHandles,
                                   DWORD dwCreationFlags,
                                   LPVOID lpEnvironment,
                                   LPCWSTR lpCurrentDirectory,
                                   LPSTARTUPINFOW lpStartupInfo,
                                   LPPROCESS_INFORMATION lpProcessInformation)
{
  if(RealCreateProcessAsUserW == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  BOOL bRet = RealCreateProcessAsUserW(hToken,
    lpApplicationName,
    lpCommandLine,
    lpProcessAttributes,
    lpThreadAttributes,
    bInheritHandles,
    dwCreationFlags,
    lpEnvironment,
    lpCurrentDirectory,
    lpStartupInfo,
    lpProcessInformation);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();

  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"CreateProcessAsUserW(%03d)",++count),base::StringPrintf(L"lpCommandLine=%ls",lpCommandLine));

  return bRet;
}

LPWSTR  WINAPI OnGetCommandLineW()
{
  if(RealGetCommandLineW == NULL)
    return NULL;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  LPWSTR pRet = RealGetCommandLineW();
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
/*
  if(pRet){
    static int count = 0;
    ProfilerData::Action(ticks_start,ticks_end - ticks_start,
      GetCurrentProcessId(),GetCurrentThreadId(),
      base::StringPrintf(L"GetCommandLineW(%03d)",++count),base::StringPrintf(L"lpCommandLine=%ls",pRet));
  }
*/
  return pRet;
}

LPTOP_LEVEL_EXCEPTION_FILTER WINAPI OnSetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter)
{
  if(RealSetUnhandledExceptionFilter == NULL)
    return NULL;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  LPTOP_LEVEL_EXCEPTION_FILTER pRet = RealSetUnhandledExceptionFilter(lpTopLevelExceptionFilter);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();

  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"SetUnhandledExceptionFilter(%03d)",++count),L"(null)");

  return pRet;
}

BOOL    WINAPI OnQueueUserWorkItem(LPTHREAD_START_ROUTINE Function,PVOID Context,ULONG Flags)
{
  if(RealQueueUserWorkItem == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  BOOL bRet = RealQueueUserWorkItem(Function,Context,Flags);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"QueueUserWorkItem(%03d)",++count),L"null");
*/
  return bRet;
}

//gdi 
HBITMAP WINAPI OnLoadBitmapA(HINSTANCE hInstance,LPCSTR lpBitmapName){
  if(RealLoadBitmapA == NULL)
    return NULL;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HBITMAP hRet = RealLoadBitmapA(hInstance,lpBitmapName);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)hRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnLoadBitmapA(%03d)",++count),L"null");
*/
  return hRet;
  }

HBITMAP WINAPI OnLoadBitmapW(HINSTANCE hInstance,LPCWSTR lpBitmapName){
  if(RealLoadBitmapW == NULL)
    return NULL;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HBITMAP hRet = RealLoadBitmapW(hInstance,lpBitmapName);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)hRet);
/*
    static int count = 0;
    ProfilerData::Action(ticks_start,ticks_end - ticks_start,
      GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnLoadBitmapW(%03d)",++count),L"null");
*/
  return hRet;
  }

// 如果从一个callstack出来的handle一直涨,涨到500就准备崩溃=
// call_stack可以不加载符号,直接在dump里面来分析=
HBITMAP WINAPI OnCreateBitmap(int nWidth,int nHeight,UINT nPlanes,UINT nBitCount,CONST VOID *lpBits){
  if(RealCreateBitmap == NULL)
    return NULL;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HBITMAP hRet = RealCreateBitmap(nWidth,nHeight,nPlanes,nBitCount,lpBits);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)hRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateBitmap(%03d)",++count),L"null");
*/
  return hRet;
}

HBITMAP WINAPI OnCreateBitmapIndirect(CONST BITMAP *pbm){
  if(RealCreateBitmapIndirect == NULL)
    return NULL;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HBITMAP hRet = RealCreateBitmapIndirect(pbm);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)hRet);
/*
    static int count = 0;
    ProfilerData::Action(ticks_start,ticks_end - ticks_start,
      GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateBitmapIndirect(%03d)",++count),L"null");
*/
  return hRet;
  }

HBITMAP WINAPI OnCreateCompatibleBitmap(HDC hdc, int cx,int cy){
  if(RealCreateCompatibleBitmap == NULL)
    return NULL;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HBITMAP hRet = RealCreateCompatibleBitmap(hdc,cx,cy);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)hRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateCompatibleBitmap(%03d)",++count),L"null");
*/
  return hRet;
}

HBITMAP WINAPI OnCreateDIBSection(__in_opt HDC hdc, __in CONST BITMAPINFO *lpbmi, __in UINT usage, __deref_opt_out VOID **ppvBits, __in_opt HANDLE hSection, __in DWORD offset){
  if(RealCreateDIBSection == NULL)
    return NULL;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HBITMAP hRet = RealCreateDIBSection(hdc,lpbmi,usage,ppvBits,hSection,offset);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)hRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateDIBSection(%03d)",++count),L"null");
*/
  return hRet;
}

HBITMAP WINAPI OnCreateDIBitmap( __in HDC hdc, __in_opt CONST BITMAPINFOHEADER *pbmih, __in DWORD flInit, __in_opt CONST VOID *pjBits, __in_opt CONST BITMAPINFO *pbmi, __in UINT iUsage){
  if(RealCreateDIBitmap == NULL)
    return NULL;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HBITMAP hRet = RealCreateDIBitmap(hdc,pbmih,flInit,pjBits,pbmi,iUsage);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)hRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateDIBitmap(%03d)",++count),L"null");
*/
  return hRet;
}

HBRUSH  WINAPI OnCreateBrushIndirect( __in CONST LOGBRUSH *plbrush){
  if(RealCreateBrushIndirect == NULL)
    return NULL;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HBRUSH hRet = RealCreateBrushIndirect(plbrush);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)hRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateBrushIndirect(%03d)",++count),L"null");
*/
  return hRet;
}

HBRUSH WINAPI OnCreateSolidBrush(COLORREF crColor)
{
  if(RealCreateSolidBrush == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HBRUSH bRet=RealCreateSolidBrush(crColor);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateSolidBrush(%03d)",++count),L"null");
*/
  return bRet;
}

HBRUSH WINAPI OnCreatePatternBrush(HBITMAP hbmp)
{
  if(RealCreatePatternBrush == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HBRUSH hRet=RealCreatePatternBrush(hbmp);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)hRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreatePatternBrush(%03d)",++count),L"null");
*/
  return hRet;
}

HBRUSH WINAPI OnCreateDIBPatternBrush(HGLOBAL hglbDIBPacked,UINT fuColorSpec)
{
  if(RealCreateDIBPatternBrush == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HBRUSH bRet=RealCreateDIBPatternBrush(hglbDIBPacked,fuColorSpec);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateDIBPatternBrush(%03d)",++count),L"null");
*/
  return bRet;
}

HBRUSH WINAPI OnCreateDIBPatternBrushPt(CONST VOID *lpPackedDIB,UINT iUsage)
{
  if(RealCreateDIBPatternBrushPt == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HBRUSH bRet=RealCreateDIBPatternBrushPt(lpPackedDIB,iUsage);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateDIBPatternBrushPt(%03d)",++count),L"null");
*/
  return bRet;
}

HBRUSH WINAPI OnCreateHatchBrush(int fnStyle,COLORREF clrref)
{
  if(RealCreateHatchBrush == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HBRUSH bRet=RealCreateHatchBrush(fnStyle,clrref);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateHatchBrush(%03d)",++count),L"null");
*/
  return bRet;
}

HDC WINAPI OnCreateCompatibleDC(HDC hdc)
{
  if(RealCreateCompatibleDC == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HDC hRet=RealCreateCompatibleDC(hdc);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)hRet);

  static int count = 0;
  if(++count%500) 
    return hRet; //调用太频繁了=
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateCompatibleDC(%03d)",++count),L"null");

  return hRet;
}

HDC WINAPI OnCreateDCA(LPCSTR lpszDriver,LPCSTR lpszDevice,LPCSTR lpszOutput,CONST DEVMODE* lpInitData)
{
  if(RealCreateDCA == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HDC bRet=RealCreateDCA(lpszDriver,lpszDevice,lpszOutput,lpInitData);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateDCA(%03d)",++count),L"null");
*/
  return bRet;
}

HDC WINAPI OnCreateDCW(LPCWSTR lpszDriver,LPCWSTR lpszDevice,LPCWSTR lpszOutput,CONST DEVMODE* lpInitData)
{
  if(RealCreateDCW == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HDC bRet=RealCreateDCW(lpszDriver,lpszDevice,lpszOutput,lpInitData);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateDCW(%03d)",++count),L"null");
*/
  return bRet;
}

HDC WINAPI OnCreateICA(LPCSTR lpszDriver,LPCSTR lpszDevice,LPCSTR lpszOutput,CONST DEVMODE* lpInitData)
{
  if(RealCreateICA == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HDC bRet=RealCreateICA(lpszDriver,lpszDevice,lpszOutput,lpInitData);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateICA(%03d)",++count),L"null");
*/
  return bRet;
}

HDC WINAPI OnCreateICW(LPCWSTR lpszDriver,LPCWSTR lpszDevice,LPCWSTR lpszOutput,CONST DEVMODE* lpInitData)
{
  if(RealCreateICW == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HDC bRet=RealCreateICW(lpszDriver,lpszDevice,lpszOutput,lpInitData);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateICW(%03d)",++count),L"null");
*/
  return bRet;
}

HDC WINAPI OnGetDC(HWND hWnd)
{
  if(RealGetDC == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HDC bRet=RealGetDC(hWnd);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnGetDC(%03d)",++count),L"null");
*/
  return bRet;
}

HDC WINAPI OnGetDCEx(HWND hWnd,HRGN hrgnClip,DWORD flags)
{
  if(RealGetDCEx == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HDC bRet=RealGetDCEx( hWnd,hrgnClip,flags);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnGetDCEx(%03d)",++count),L"null");
*/
  return bRet;
}

HDC WINAPI OnGetWindowDC(HWND hWnd)
{
  if(RealGetWindowDC == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HDC bRet=RealGetWindowDC(hWnd);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnGetWindowDC(%03d)",++count),L"null");
*/
  return bRet;
}

HFONT WINAPI OnCreateFontA(int nHeight,int nWidth,int nEscapement,int nOrientation,int fnWeight,DWORD fdwItalic,
                           DWORD fdwUnderline,DWORD fdwStrikeOut,DWORD fdwCharSet,DWORD fdwOutputPrecision,
                           DWORD fdwClipPrecision,DWORD fdwQuality,DWORD fdwPitchAndFamily,LPCSTR lpszFace)
{
  if(RealCreateFontA == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HFONT bRet=RealCreateFontA(nHeight,nWidth,nEscapement,nOrientation,fnWeight,fdwItalic,
                           fdwUnderline,fdwStrikeOut,fdwCharSet,fdwOutputPrecision,
                           fdwClipPrecision,fdwQuality,fdwPitchAndFamily,lpszFace);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateFontA(%03d)",++count),L"null");
*/
  return bRet;
}

HFONT WINAPI OnCreateFontW(int nHeight,int nWidth,int nEscapement,int nOrientation,int fnWeight,DWORD fdwItalic,
                           DWORD fdwUnderline,DWORD fdwStrikeOut,DWORD fdwCharSet,DWORD fdwOutputPrecision,
                           DWORD fdwClipPrecision,DWORD fdwQuality,DWORD fdwPitchAndFamily,LPCWSTR lpszFace)
{
  if(RealCreateFontW == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HFONT bRet=RealCreateFontW(nHeight,nWidth,nEscapement,nOrientation,fnWeight,fdwItalic,
                           fdwUnderline,fdwStrikeOut,fdwCharSet,fdwOutputPrecision,
                           fdwClipPrecision,fdwQuality,fdwPitchAndFamily,lpszFace);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateFontW(%03d)",++count),L"null");
*/
  return bRet;
}

HFONT WINAPI OnCreateFontIndirectA(CONST LOGFONTA *lplf)
{
  if(RealCreateFontIndirectA == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HFONT bRet=RealCreateFontIndirectA(lplf);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateFontIndirectA(%03d)",++count),L"null");
*/
  return bRet;
}

HFONT WINAPI OnCreateFontIndirectW(CONST LOGFONTW *lplf)
{
  if(RealCreateFontIndirectW == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HFONT bRet=RealCreateFontIndirectW(lplf);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateFontIndirectW(%03d)",++count),L"null");
*/
  return bRet;
}

HPEN WINAPI OnCreatePen(int iStyle, int cWidth, COLORREF color)
{
  if(RealCreatePen == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HPEN bRet=RealCreatePen(iStyle, cWidth, color);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreatePen(%03d)",++count),L"null");
*/
  return bRet;
}

HPEN WINAPI OnCreatePenIndirect( __in CONST LOGPEN *plpen)
{
  if(RealCreatePenIndirect == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HPEN bRet=RealCreatePenIndirect(plpen);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreatePenIndirect(%03d)",++count),L"null");
*/
  return bRet;
}

HPEN WINAPI OnExtCreatePen(__in DWORD iPenStyle,__in DWORD cWidth,__in CONST LOGBRUSH *plbrush,
                           __in DWORD cStyle,__in_ecount_opt(cStyle) CONST DWORD *pstyle)
{
  if(RealExtCreatePen == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HPEN bRet=RealExtCreatePen(iPenStyle,cWidth,plbrush,cStyle,pstyle);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnExtCreatePen(%03d)",++count),L"null");
*/
  return bRet;
}

HRGN WINAPI OnPathToRegion(__in HDC hdc)
{
  if(RealPathToRegion == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HRGN bRet=RealPathToRegion(hdc);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnPathToRegion(%03d)",++count),L"null");
*/
  return bRet;
}

HRGN WINAPI OnCreateEllipticRgn( __in int x1, __in int y1, __in int x2, __in int y2)
{
  if(RealCreateEllipticRgn == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HRGN bRet=RealCreateEllipticRgn(x1, y1, x2, y2);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateEllipticRgn(%03d)",++count),L"null");
*/
  return bRet;
}

HRGN WINAPI OnCreateEllipticRgnIndirect( __in CONST RECT *lprect)
{
  if(RealCreateEllipticRgnIndirect == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HRGN bRet=RealCreateEllipticRgnIndirect(lprect);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateEllipticRgnIndirect(%03d)",++count),L"null");
*/
  return bRet;
}

HRGN WINAPI OnCreatePolygonRgn(__in_ecount(cPoint) CONST POINT *pptl,__in int cPoint,__in int iMode)
{
  if(RealCreatePolygonRgn == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HRGN bRet=RealCreatePolygonRgn(pptl,cPoint,iMode);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreatePolygonRgn(%03d)",++count),L"null");
*/
  return bRet;
}

HRGN WINAPI OnCreatePolyPolygonRgn(  __in CONST POINT *pptl,__in CONST INT  *pc,__in int cPoly,__in int iMode)
{
  if(RealCreatePolyPolygonRgn == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HRGN bRet=RealCreatePolyPolygonRgn(pptl,pc,cPoly,iMode);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreatePolyPolygonRgn(%03d)",++count),L"null");
*/
  return bRet;
}

HRGN WINAPI OnCreateRectRgn( __in int x1, __in int y1, __in int x2, __in int y2)
{
  if(RealCreateRectRgn == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HRGN bRet=RealCreateRectRgn(x1,y1,x2,y2);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);

  static int count = 0;
  if(++count%500) 
    return bRet; //调用太频繁了=
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateRectRgn(%03d)",++count),L"null");

  return bRet;
}

HRGN WINAPI OnCreateRectRgnIndirect( __in CONST RECT *lprect)
{
  if(RealCreateRectRgnIndirect == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HRGN bRet=RealCreateRectRgnIndirect(lprect);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateRectRgnIndirect(%03d)",++count),L"null");
*/
  return bRet;
}

HRGN WINAPI OnCreateRoundRectRgn( __in int x1, __in int y1, __in int x2, __in int y2, __in int w, __in int h)
{
  if(RealCreateRoundRectRgn == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HRGN bRet=RealCreateRoundRectRgn(x1, y1, x2, y2, w, h);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateRoundRectRgn(%03d)",++count),L"null");
*/
  return bRet;
}

HRGN WINAPI OnExtCreateRegion( __in_opt CONST XFORM * lpx, __in DWORD nCount, __in_bcount(nCount) CONST RGNDATA * lpData)
{
  if(RealExtCreateRegion == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HRGN bRet=RealExtCreateRegion(lpx,nCount,lpData);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnExtCreateRegion(%03d)",++count),L"null");
*/
  return bRet;
}

BOOL WINAPI OnDeleteObject(HGDIOBJ ho){
  if(RealDeleteObject == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  BOOL bRet = RealDeleteObject(ho);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  int32 live_gdis = g_handle_watcher->OnUntrack((uint32)ho);

  static int count = 0;
  if(++count%500) 
    return bRet; //调用太频繁了=
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnDeleteObject(%03d)(live_gdis:%d)",count,live_gdis),L"null");

  return bRet;
}

BOOL WINAPI OnDeleteDC(__in HDC hdc){
  if(RealDeleteDC == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  BOOL bRet = RealDeleteDC(hdc);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  int32 live_gdis = g_handle_watcher->OnUntrack((uint32)hdc);

  static int count = 0;
  if(++count%500) 
    return bRet; //调用太频繁了=
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnDeleteDC(%03d)(live_gdis:%d)",count,live_gdis),L"null");

  return bRet;
}

int  WINAPI OnReleaseDC(__in_opt HWND hWnd,__in HDC hDC){
  if(RealReleaseDC == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  int iRet = RealReleaseDC(hWnd,hDC);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  int32 live_gdis = g_handle_watcher->OnUntrack((uint32)hDC);

  static int count = 0;
  if(++count%500) 
    return iRet; //调用太频繁了=
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnReleaseDC(%03d)(live_gdis:%d)",count,live_gdis),L"null");

  return iRet;
}

//user object
HICON WINAPI OnCreateIcon(HINSTANCE hInstance,int nWidth,int nHeight,BYTE cPlanes,BYTE cBitsPixel,
                          const BYTE *lpbANDbits,const BYTE *lpbXORbits)
{
  if(RealCreateIcon == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HICON bRet=RealCreateIcon(hInstance,nWidth,nHeight,cPlanes,cBitsPixel,lpbANDbits,lpbXORbits);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnCreateIcon(%03d)",++count),L"null");
*/
  return bRet;
}

HICON WINAPI OnLoadIcon(HINSTANCE hInstance,LPCTSTR lpIconName)
{
  if(RealLoadIcon == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  HICON bRet=RealLoadIcon(hInstance,lpIconName);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  g_handle_watcher->OnTrack((uint32)bRet);
/*
  static int count = 0;
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnLoadIcon(%03d)",++count),L"null");
*/
  return bRet;
}

BOOL WINAPI OnDestroyIcon(__in HICON hIcon){
  if(RealDestroyIcon == NULL)
    return FALSE;

  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  BOOL bRet = RealDestroyIcon(hIcon);
  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  int32 live_gdis = g_handle_watcher->OnUntrack((uint32)hIcon);

  static int count = 0;
  if(++count%500) 
    return bRet; //调用太频繁了=
  ProfilerData::Action(ticks_start,ticks_end - ticks_start,
    GetCurrentProcessId(),GetCurrentThreadId(),
    base::StringPrintf(L"OnDestroyIcon(%03d)(live_gdis:%d)",count,live_gdis),L"null");

  return bRet;
}

}


//////////////////////////////////////////////////////////////////////////