#include "tools/profiler/profiler_cmds.h"

#include "base/files/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process_iterator.h"
#include "base/process/process_metrics.h"
#include "base/time/time.h"
//#include "base/third_party/360signlib/signlib.h"
#include "base/threading/thread_restrictions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/file_version_info.h"
#include "base/strings/StringPrintf.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/win/registry.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_comptr.h"
#include "tools/profiler/profiler_thread.h"
#include "tools/profiler/profiler_process.h"
#include "tools/profiler/lowpri.h"
#include "tools/profiler/nt_loader.h"
#include "tools/profiler/minidump_prober.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atldlgs.h>
#include <fstream>
#include <dbghelp.h>
#include <shellapi.h>
#include <shlobj.h>
#include <Psapi.h>

//////////////////////////////////////////////////////////////////////////
namespace{

void ShowItemInFolder(const base::FilePath& full_path) {
  DCHECK(ProfilerThread::CurrentlyOn(ProfilerThread::IO));
  base::FilePath dir = full_path.DirName().AsEndingWithSeparator();
  // ParseDisplayName will fail if the directory is "C:", it must be "C:\\".
  if (dir.empty())
    return;

  typedef HRESULT (WINAPI *SHOpenFolderAndSelectItemsFuncPtr)(
      PCIDLIST_ABSOLUTE pidl_Folder,
      UINT cidl,
      PCUITEMID_CHILD_ARRAY pidls,
      DWORD flags);

  static SHOpenFolderAndSelectItemsFuncPtr open_folder_and_select_itemsPtr =
    NULL;
  static bool initialize_open_folder_proc = true;
  if (initialize_open_folder_proc) {
    initialize_open_folder_proc = false;
    // The SHOpenFolderAndSelectItems API is exposed by shell32 version 6
    // and does not exist in Win2K. We attempt to retrieve this function export
    // from shell32 and if it does not exist, we just invoke ShellExecute to
    // open the folder thus losing the functionality to select the item in
    // the process.
    HMODULE shell32_base = GetModuleHandle(L"shell32.dll");
    if (!shell32_base) {
      NOTREACHED() << " " << __FUNCTION__ << "(): Can't open shell32.dll";
      return;
    }
    open_folder_and_select_itemsPtr =
        reinterpret_cast<SHOpenFolderAndSelectItemsFuncPtr>
            (GetProcAddress(shell32_base, "SHOpenFolderAndSelectItems"));
  }
  if (!open_folder_and_select_itemsPtr) {
    ShellExecute(NULL, L"open", dir.value().c_str(), NULL, NULL, SW_SHOW);
    return;
  }

  base::win::ScopedComPtr<IShellFolder> desktop;
  HRESULT hr = SHGetDesktopFolder(desktop.Receive());
  if (FAILED(hr))
    return;

  base::win::ScopedCoMem<ITEMIDLIST> dir_item;
  hr = desktop->ParseDisplayName(NULL, NULL,
                                 const_cast<wchar_t *>(dir.value().c_str()),
                                 NULL, &dir_item, NULL);
  if (FAILED(hr))
    return;

  base::win::ScopedCoMem<ITEMIDLIST> file_item;
  hr = desktop->ParseDisplayName(NULL, NULL,
      const_cast<wchar_t *>(full_path.value().c_str()),
      NULL, &file_item, NULL);
  if (FAILED(hr))
    return;

  const ITEMIDLIST* highlight[] = {
    {file_item},
  };

  hr = (*open_folder_and_select_itemsPtr)(dir_item, arraysize(highlight),
                                          highlight, NULL);

  if (FAILED(hr)) {
    // On some systems, the above call mysteriously fails with "file not
    // found" even though the file is there.  In these cases, ShellExecute()
    // seems to work as a fallback (although it won't select the file).
    if (hr == ERROR_FILE_NOT_FOUND) {
      ShellExecute(NULL, L"open", dir.value().c_str(), NULL, NULL, SW_SHOW);
    } else {
      LPTSTR message = NULL;
      DWORD message_length = FormatMessage(
          FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
          0, hr, 0, reinterpret_cast<LPTSTR>(&message), 0, NULL);
      LOG(WARNING) << " " << __FUNCTION__
                   << "(): Can't open full_path = \""
                   << full_path.value() << "\""
                   << " hr = " << hr
                   << " " << reinterpret_cast<LPTSTR>(&message);
      if (message)
        LocalFree(message);
    }
  }
}

// Show the Windows "Open With" dialog box to ask the user to pick an app to
// open the file with.
bool OpenItemWithExternalApp(const string16& full_path) {
  SHELLEXECUTEINFO sei = { sizeof(sei) };
  sei.fMask = SEE_MASK_FLAG_DDEWAIT;
  sei.nShow = SW_SHOWNORMAL;
  sei.lpVerb = L"openas";
  sei.lpFile = full_path.c_str();
  return (TRUE == ::ShellExecuteExW(&sei));
}

bool OpenItemViaShellNoZoneCheck(const base::FilePath& full_path) {
  SHELLEXECUTEINFO sei = { sizeof(sei) };
  sei.fMask = SEE_MASK_NOZONECHECKS | SEE_MASK_FLAG_DDEWAIT;
  sei.nShow = SW_SHOWNORMAL;
  sei.lpVerb = NULL;
  sei.lpFile = full_path.value().c_str();
  if (::ShellExecuteExW(&sei))
    return true;
  LONG_PTR error = reinterpret_cast<LONG_PTR>(sei.hInstApp);
  if (error == SE_ERR_NOASSOC)
    return OpenItemWithExternalApp(full_path.value());
  return false;
}

// Open an item via a shell execute command. Error code checking and casting
// explanation: http://msdn2.microsoft.com/en-us/library/ms647732.aspx
bool OpenItemViaShell(const base::FilePath& full_path) {
  HINSTANCE h = ::ShellExecuteW(
    NULL, NULL, full_path.value().c_str(), NULL,
    full_path.DirName().value().c_str(), SW_SHOWNORMAL);

  LONG_PTR error = reinterpret_cast<LONG_PTR>(h);
  if (error > 32)
    return true;

  if ((error == SE_ERR_NOASSOC))
    return OpenItemWithExternalApp(full_path.value());
  return false;
}

//会再chrome.dll目录下搜索chrome_dll.pdb，找到后，会显示效果不错的符号哦=
bool ChooseChromeDllFile(HWND parent,std::wstring& file_path){
  CFileDialog fileDlg ( true, L"dll", L"chrome.dll", OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
    L"动态库(*.dll)\0*.dll\0全部文件(*.*)\0*.*\0",parent);

  if ( IDOK == fileDlg.DoModal() ){
    file_path = fileDlg.m_szFileName;
    return true;
  }
  return false;
}

bool SaveGlobalVarFile(HWND parent,std::wstring& file_path){
  CFileDialog fileDlg ( false, L"txt", L"dump_globalvar.txt", OFN_HIDEREADONLY,
    L"文本文件(*.txt)\0*.txt\0全部文件(*.*)\0*.*\0",parent);

  if ( IDOK == fileDlg.DoModal() ){
    file_path = fileDlg.m_szFileName;
    return true;
  }
  return false;
}

bool ChooseMinidumpFile(HWND parent,std::wstring& file_path){
  CFileDialog fileDlg ( true, L"dmp",NULL, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
    L"Dump文件(*.dmp)\0*.dmp\0全部文件(*.*)\0*.*\0",parent);

  if ( IDOK == fileDlg.DoModal() ){
    file_path = fileDlg.m_szFileName;
    return true;
  }
  return false;
}

int CalcLine(const std::wstring& txt_path){
  int total_line = 0;
  std::ifstream file_txt(txt_path.c_str(), std::ios::in);
  if (file_txt.is_open()){
    do {
      std::string line_txt;
      getline(file_txt, line_txt);
      if(file_txt.bad())
        break;
      if(line_txt.length()) //可能有空行在尾部=
        total_line++;
    }while(!file_txt.eof());
  }

  return total_line;
}

std::wstring Today(){
  time_t t = time(NULL);
  struct tm local_time = {0};
#if _MSC_VER >= 1400
  localtime_s(&local_time, &t);
#else
  localtime_r(&t, &local_time);
#endif
  struct tm* tm_time = &local_time;
  return base::StringPrintf(L"%04d-%02d-%02d-%02d-%02d-%02d",tm_time->tm_year+1900,tm_time->tm_mon+1,tm_time->tm_mday,
    tm_time->tm_hour,tm_time->tm_min,tm_time->tm_sec);
}

typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(
  IN HANDLE hProcess,
  IN DWORD ProcessId,
  IN HANDLE hFile,
  IN MINIDUMP_TYPE DumpType,
  IN CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam, OPTIONAL
  IN CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam, OPTIONAL
  IN CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam OPTIONAL
  );

//fulldump:MiniDumpWithFullMemory 
//minidump:MiniDumpNormal
bool CreateDumpFile(const base::FilePath& dump_path, DWORD dwProcessId, int miniDumpType,
                    PMINIDUMP_EXCEPTION_INFORMATION exception_infor)
{
  bool bRet = false;
  base::FilePath file_path;
  PathService::Get(base::DIR_EXE,&file_path);
  file_path = file_path.DirName().Append(L"dbghelp.dll");
  HMODULE dll = ::LoadLibrary(file_path.value().c_str());
  if(!dll){
    dll = ::LoadLibrary(L"dbghelp.dll");
  }
  if (dll) {
    MINIDUMPWRITEDUMP pWriteDumpFun = (MINIDUMPWRITEDUMP)::GetProcAddress(dll,"MiniDumpWriteDump");
    if (pWriteDumpFun) {
      // create the file
      std::wstring dump_file = base::StringPrintf(L"%ls\\day_%ls_pid_%d.dmp",dump_path.value().c_str(),Today().c_str(),dwProcessId);
      file_util::CreateDirectory(dump_path);
      HANDLE hFile = ::CreateFile(dump_file.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL);
      if (hFile != INVALID_HANDLE_VALUE) {
        // write the dump
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ|THREAD_ALL_ACCESS,FALSE,dwProcessId);
        DCHECK(hProcess);
        bRet = !!pWriteDumpFun(hProcess, dwProcessId, hFile, (MINIDUMP_TYPE)miniDumpType, exception_infor, NULL, NULL);
        DCHECK(bRet);
        ::CloseHandle(hProcess);
        ::CloseHandle(hFile);
      }
    }
    ::FreeLibrary(dll);
  }
  return bRet;
}

std::wstring GetFileNameFromHandle(HANDLE hFile) {
  BOOL bSuccess = FALSE;
  TCHAR pszFilename[MAX_PATH+1];
  HANDLE hFileMap;

  std::wstring strFilename;

  // Get the file size.
  DWORD dwFileSizeHi = 0;
  DWORD dwFileSizeLo = GetFileSize(hFile, &dwFileSizeHi);
  if( dwFileSizeLo == 0 && dwFileSizeHi == 0 ){
    return strFilename;
  }

  // Create a file mapping object.
  hFileMap = CreateFileMapping(hFile, 
    NULL, 
    PAGE_READONLY,
    0, 
    1,
    NULL);

  if (hFileMap) {
    // Create a file mapping to get the file name.
    void* pMem = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 1);
    if (pMem){
      if (GetMappedFileName(GetCurrentProcess(), 
        pMem, 
        pszFilename,
        MAX_PATH)){

        // Translate path with device name to drive letters.
        TCHAR szTemp[MAX_PATH];
        szTemp[0] = '\0';

        if (GetLogicalDriveStrings(MAX_PATH-1, szTemp)) {
          TCHAR szName[MAX_PATH];
          TCHAR szDrive[3] = TEXT(" :");
          BOOL bFound = FALSE;
          TCHAR* p = szTemp;

          do {
            // Copy the drive letter to the template string
            *szDrive = *p;

            // Look up each device name
            if (QueryDosDevice(szDrive, szName, MAX_PATH)){
              size_t uNameLen = _tcslen(szName);

              if (uNameLen < MAX_PATH) {
                bFound = _tcsnicmp(pszFilename, szName, 
                  uNameLen) == 0;

                if (bFound) {
                  strFilename = base::StringPrintf(L"%ls%ls",szDrive, pszFilename+uNameLen);
                }
              }
            }

            // Go to the next NULL character.
            while (*p++);
          } while (!bFound && *p); // end of string
        }
      }
      bSuccess = TRUE;
      UnmapViewOfFile(pMem);
    } 

    CloseHandle(hFileMap);
  }

  return(strFilename);
}

//收集一些系统事先隐射到dll路径=
void EnumSharedModule(std::map<DWORD,std::wstring>& module_map){
  nt_loader::EnumAllModule(module_map);
}

//根据地址找到模块和偏移=，可以用于定位插件在没有sym的情况下=
//输入参数pExceptionRecord->ExceptionAddress
// http://www.cppblog.com/fwxjj/archive/2007/12/05/37867.html
//==============================================================================
// Given a linear address, locates the module, section, and offset containing  
// that address.                                                               
//                                                                             
// Note: the szModule paramater buffer is an output buffer of length specified 
// by the len parameter (in characters!)    
//////////////////////////////////////////////////////////////////////////
BOOL GetLogicalAddress(
                       PVOID addr, PTSTR szModule, DWORD len, DWORD& section, DWORD& offset )
{
  MEMORY_BASIC_INFORMATION mbi;

  if ( !VirtualQuery( addr, &mbi, sizeof(mbi) ) )
    return FALSE;

  DWORD hMod = (DWORD)mbi.AllocationBase;

  if ( !GetModuleFileName( (HMODULE)hMod, szModule, len ) )
    return FALSE;

  // Point to the DOS header in memory
  PIMAGE_DOS_HEADER pDosHdr = (PIMAGE_DOS_HEADER)hMod;

  // From the DOS header, find the NT (PE) header
  PIMAGE_NT_HEADERS pNtHdr = (PIMAGE_NT_HEADERS)(hMod + pDosHdr->e_lfanew);

  PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION( pNtHdr );

  DWORD rva = (DWORD)addr - hMod; // RVA is offset from module load address

  // Iterate through the section table, looking for the one that encompasses
  // the linear address.
  for (   unsigned i = 0;
    i < pNtHdr->FileHeader.NumberOfSections;
    i++, pSection++ )
  {
    DWORD sectionStart = pSection->VirtualAddress;
    DWORD sectionEnd = sectionStart
      + std::max(pSection->SizeOfRawData, pSection->Misc.VirtualSize);

    // Is the address in this section???
    if ( (rva >= sectionStart) && (rva <= sectionEnd) )
    {
      // Yes, address is in the section.  Calculate section and offset,
      // and store in the "section" & "offset" params, which were
      // passed by reference.
      section = i+1;
      offset = rva - sectionStart;
      return TRUE;
    }
  }

  return FALSE;   // Should never get here!
}

}

//////////////////////////////////////////////////////////////////////////
namespace ProfilerCmd{

//////////////////////////////////////////////////////////////////////////
//在chrome.dll目录下搜索chrome_dll.pdb
void DumpGlobalVa_IO(std::wstring dll_path,std::wstring log_path){
  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();

  typedef int (__cdecl *FN_DumpGlobalVar)(DWORD magic, wchar_t* log_path);
  BOOL bRet = FALSE;
  int total_line = 0;
  HMODULE dll_module = LoadLibraryExW(dll_path.c_str(),NULL,LOAD_WITH_ALTERED_SEARCH_PATH);
  if(dll_module){
    FN_DumpGlobalVar fn_dump = (FN_DumpGlobalVar)GetProcAddress(dll_module,"DumpGlobalVar");
    if(fn_dump){
      bRet = !!fn_dump(0x37213721,(wchar_t*)log_path.c_str());
      if(bRet){
        total_line = CalcLine(log_path);
      }
    }
    FreeLibrary(dll_module);
  }

  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  int64 delta_ms = ticks_end - ticks_start;

  g_profiler_process->profiler_data()->RawAction(
    delta_ms,
    L"DumpGlobaVar", 
    bRet?base::StringPrintf(L"变量个数:%d,日志路径:%ls",total_line,log_path.c_str()):L"error,请在dll源代码里面包含dump_globavar.cc");
}

//在dll所在目录搜索符号=
void DumpGlobalVar(HWND parent)
{
  std::wstring dll_path;
  if(!ChooseChromeDllFile(parent,dll_path)){
    return;
  }
  //弹出两个对话框太不优雅了=
/*
  std::wstring log_path;
  if(!SaveGlobalVarFile(parent,log_path)){
    return;
  }
*/

  ProfilerThread::PostTask(ProfilerThread::IO,FROM_HERE,base::Bind(DumpGlobalVa_IO,
    dll_path,
    base::FilePath(dll_path).ReplaceExtension(L"txt").value()));
}

//////////////////////////////////////////////////////////////////////////
void RunLow_IO(){
  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  bool run_ok = false;

  base::FilePath file_path;
  file_path = base::FilePath(ProfilerData::GetAppPaths(L"360se6.exe"));
  std::wstring chrome_path = file_path.value();
  if(chrome_path.length()){
    int error_code = 0;
    base::KillProcesses(L"360se.exe",0,NULL);
    base::WaitForProcessesToExit(L"360se.exe",base::TimeDelta::FromMilliseconds(100),NULL);
    if(lowpri::ExecuteFileAsLowPri(chrome_path.c_str(),&error_code,NULL)){
      run_ok = true;
    }
  }

  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  int64 delta_ms = ticks_end - ticks_start;

  g_profiler_process->profiler_data()->RawAction(
    delta_ms,L"RunLow", run_ok?chrome_path:L"error");
}

void RunLow(HWND parent){
  ProfilerThread::PostTask(ProfilerThread::IO,FROM_HERE,base::Bind(RunLow_IO));
}

//////////////////////////////////////////////////////////////////////////

void LimitedJob_IO(){
  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();
  bool run_ok = false;

  base::FilePath file_path;
  file_path = base::FilePath(ProfilerData::GetAppPaths(L"360se6.exe"));
  std::wstring chrome_path = file_path.value();
  if(chrome_path.length()){
    HANDLE job = ::CreateJobObject(NULL,NULL);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION extended_info = {0};
    ::QueryInformationJobObject(
      job,
      ::JobObjectExtendedLimitInformation,
      &extended_info,
      sizeof(extended_info),
      NULL);
    extended_info.BasicLimitInformation.LimitFlags &= ~(JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK | JOB_OBJECT_LIMIT_BREAKAWAY_OK);
    ::SetInformationJobObject(
      job,
      ::JobObjectExtendedLimitInformation,
      &extended_info,
      sizeof(extended_info));

    base::KillProcesses(L"360se.exe",0,NULL);
    base::WaitForProcessesToExit(L"360se.exe",base::TimeDelta::FromMilliseconds(100),NULL);
    base::LaunchOptions options = base::LaunchOptions();
    options.job_handle = job;
    run_ok = base::LaunchProcess(chrome_path,options,NULL);
    CloseHandle(job);
  }

  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  int64 delta_ms = ticks_end - ticks_start;

  g_profiler_process->profiler_data()->RawAction(
    delta_ms,L"LimitedJob", run_ok?chrome_path:L"error");
}

void LimitedJob(HWND parent){
  ProfilerThread::PostTask(ProfilerThread::IO,FROM_HERE,base::Bind(LimitedJob_IO));
}

//////////////////////////////////////////////////////////////////////////

//todo(hege),先suspend所有线程然后抓dump(outofprocess)
//先开个线程然后suspend所有其他线程再抓(inofprocess)

void WriteMinidump_IO(bool fulldump){
  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();

  int total = base::GetProcessCount(L"360se.exe",NULL);
  base::NamedProcessIterator it(L"360se.exe",NULL);
  int count=0;
  base::FilePath file_path;
  PathService::Get(base::DIR_EXE,&file_path);
  file_path = file_path.Append(L"minidump");
  while(const base::ProcessEntry* process_entry = it.NextProcessEntry()){
    if(::CreateDumpFile(file_path,process_entry->pid(),fulldump?MiniDumpWithFullMemory:MiniDumpNormal,NULL)){
      count++;
    }
}

  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  int64 delta_ms = ticks_end - ticks_start;

  g_profiler_process->profiler_data()->RawAction(
    delta_ms,L"writeminidump",base::StringPrintf(L"write %d/%d minidump file to %ls",count,total,file_path.value().c_str()));
}

void WriteMinidump(bool fulldump){
  ProfilerThread::PostTask(ProfilerThread::IO,FROM_HERE,base::Bind(WriteMinidump_IO,fulldump));
}

//////////////////////////////////////////////////////////////////////////

void OpenMinidumpFolder(){
  base::FilePath file_path;
  PathService::Get(base::DIR_EXE,&file_path);
  file_path = file_path.Append(L"minidump");
  OpenItemViaShell(file_path);
}

//////////////////////////////////////////////////////////////////////////
void DebugRun_Log(std::wstring action,std::wstring data){
  g_profiler_process->profiler_data()->DebugAction(action,data);
}

void DebugRun_Info(std::wstring action,std::wstring data){
  //g_profiler_process->profiler_data()->DebugAction(action,data);
}

void DebugRun_OnProcessCreated(DWORD pid,const CREATE_PROCESS_DEBUG_INFO* pInfo) {
  DebugRun_Log(L"DebugRun",base::StringPrintf(L"Debuggee(%d) was created:%ls",
  pid,GetFileNameFromHandle(pInfo->hFile).c_str()));
  CloseHandle(pInfo->hFile);
  //系统关闭这2个句柄=
  //CloseHandle(pInfo->hThread);
  //CloseHandle(pInfo->hProcess);
}

void DebugRun_OnThreadCreated(DWORD tid,const CREATE_THREAD_DEBUG_INFO* pInfo) {
  //系统关闭这个句柄=
  //CloseHandle(pInfo->hThread);
  DebugRun_Info(L"DebugRun",base::StringPrintf(L"A new thread(%d) was created from (0x%08x).",
    tid,pInfo->lpStartAddress));
}

//
//对于breakpoint都用DBG_CONTINUE表示我们已经处理原地继续执行=
//对于非breakpoint，都用NOT_HANDLED处理，让debuggee自己的异常处理功能来处理
//对于second的异常，就是debuggee没能处理掉，重新让debugger来处理，这里肯定要崩溃=
//有调试器时候,SetUnhandleExceptionFilter无效= :-)
//
void DebugRun_OnException(DWORD pid,DWORD tid,const EXCEPTION_DEBUG_INFO* pInfo,DWORD& dwContinueStatus) {
  static DWORD last_addr = 0;
  static DWORD last_code = 0;
  switch(pInfo->ExceptionRecord.ExceptionCode){
  case STATUS_BREAKPOINT:
    DebugRun_Log(L"DebugRun",L"Break point.");
    break;
  default:
    if(pInfo->dwFirstChance == 1){				
      DebugRun_Log(L"DebugRun",base::StringPrintf(L"First chance exception at 0x%08x, exception-code: 0x%08x,pid=%d,tid=%d", 
        pInfo->ExceptionRecord.ExceptionAddress,
        pInfo->ExceptionRecord.ExceptionCode,
        pid,
        tid));
    }else{
      DebugRun_Log(L"DebugRun",L"An exception was occured second,now crash!!!.");
      if(last_addr != (DWORD)pInfo->ExceptionRecord.ExceptionAddress ||
        last_code != (DWORD)pInfo->ExceptionRecord.ExceptionCode){
        last_addr = (DWORD)pInfo->ExceptionRecord.ExceptionAddress;
        last_code = (DWORD)pInfo->ExceptionRecord.ExceptionCode;
        //write one minidump
        base::FilePath file_path;
        PathService::Get(base::DIR_EXE,&file_path);
        file_path = file_path.Append(L"minidump");

        CONTEXT context;
        context.ContextFlags = CONTEXT_FULL;
        HANDLE thread_handle = OpenThread(THREAD_GET_CONTEXT|THREAD_SET_CONTEXT|THREAD_SUSPEND_RESUME,FALSE,tid);
        if(thread_handle!=NULL && thread_handle!=INVALID_HANDLE_VALUE){
          if(::GetThreadContext(thread_handle,&context)){
            EXCEPTION_POINTERS exceptionPointers;
            exceptionPointers.ExceptionRecord = (PEXCEPTION_RECORD)&pInfo->ExceptionRecord;
            exceptionPointers.ContextRecord = &context;

            MINIDUMP_EXCEPTION_INFORMATION exceptionInformation;
            exceptionInformation.ThreadId = tid;
            exceptionInformation.ExceptionPointers = &exceptionPointers;
            exceptionInformation.ClientPointers = FALSE;
            if(CreateDumpFile(file_path,pid,MiniDumpNormal,&exceptionInformation)){
              DebugRun_Log(L"DebugRun",base::StringPrintf(L"Write 1 minidump to %ls",file_path.value().c_str()));
            }
          }
          CloseHandle(thread_handle);
        }
      }
    }
    dwContinueStatus = DBG_EXCEPTION_NOT_HANDLED;	
    break;
  }
}

void DebugRun_OnProcessExited(DWORD pid,const EXIT_PROCESS_DEBUG_INFO* pInfo) {
  DebugRun_Log(L"DebugRun",base::StringPrintf(L"Debuggee(%d) was terminated with exitcode(0x%08x).",
    pid,pInfo->dwExitCode));
}

void DebugRun_OnThreadExited(DWORD tid,const EXIT_THREAD_DEBUG_INFO* pInfo) {
  DebugRun_Info(L"DebugRun",base::StringPrintf(L"The thread(%d) was terminated with exitcode(0x%08x).",
    tid,pInfo->dwExitCode));
}

void DebugRun_OnOutputDebugString(HANDLE process_handle,const OUTPUT_DEBUG_STRING_INFO* pInfo) {
  WCHAR *msg=new WCHAR[pInfo->nDebugStringLength+1];
  ZeroMemory(msg, pInfo->nDebugStringLength);
  ReadProcessMemory(process_handle,pInfo->lpDebugStringData,msg,pInfo->nDebugStringLength, NULL);
  if(pInfo->fUnicode){
    msg[pInfo->nDebugStringLength]=0;
    DebugRun_Info(L"DebugRun",base::StringPrintf(L"Debuggee OutputdDebugStringW(%ls).",msg));
  }
  else{
    char* amsg = (char*)msg;
    msg[pInfo->nDebugStringLength]=0;
    DebugRun_Info(L"DebugRun",UTF8ToWide(base::StringPrintf("Debuggee OutputDebugStringA(%s).",amsg)));
  }
  delete[] msg;
}

void DebugRun_OnRipEvent(const RIP_INFO* pInfo) {
  DebugRun_Info(L"DebugRun",L"A RIP_EVENT occured.");
}

void DebugRun_OnDllLoaded(std::map<DWORD,std::wstring>& module_map,const LOAD_DLL_DEBUG_INFO* pInfo) {
  std::wstring path = GetFileNameFromHandle(pInfo->hFile);
  if(path.length()){
    DebugRun_Info(L"DebugRun",base::StringPrintf(L"A dll(0x%08x) was loaded,path=(%ls)",
      pInfo->lpBaseOfDll,path.c_str()));
    module_map[(DWORD)pInfo->lpBaseOfDll] = path;
  }
  CloseHandle(pInfo->hFile);
}

void DebugRun_OnDllUnloaded(std::map<DWORD,std::wstring>& module_map,const UNLOAD_DLL_DEBUG_INFO* pInfo) {
  if(module_map.find((DWORD)pInfo->lpBaseOfDll)!=module_map.end() && module_map[(DWORD)pInfo->lpBaseOfDll].length()){
    DebugRun_Info(L"DebugRun",base::StringPrintf(L"The dll(0x%08x) was unloaded,path=(%ls)",
      pInfo->lpBaseOfDll,module_map[(DWORD)pInfo->lpBaseOfDll].c_str()));
  }
}

void ContinueDebugEventIsSEHFrame(DWORD pid,DWORD tid,DWORD status){
  __try{
    ContinueDebugEvent(pid,tid,status);
  } __except(EXCEPTION_EXECUTE_HANDLER) {
  }
}

//放到非io线程上，不打扰其他功能执行=
#pragma warning (disable:4509)
void DebugRun_DB(){
  DCHECK(ProfilerThread::CurrentlyOn(ProfilerThread::DB));

  //debug create
  STARTUPINFO si = { 0 };
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = { 0 };
  base::FilePath file_path;
  file_path = base::FilePath(ProfilerData::GetAppPaths(L"360se6.exe"));
  std::wstring chrome_path = file_path.value();
  if (!CreateProcess(
      chrome_path.c_str(),
      NULL,
      NULL,
      NULL,
      FALSE,
      //DEBUG_ONLY_THIS_PROCESS,
      DEBUG_PROCESS,
      NULL,
      NULL,
      &si,
      &pi)) {
    DebugRun_Log(L"DebugRun",base::StringPrintf(L"CreateProcess fail:%ls",chrome_path.c_str()));
    return;
  }
  DebugRun_Log(L"DebugRun",L"enter");
  ::DebugSetProcessKillOnExit(FALSE);

  //debug loop
  bool bContinueDebugging = true;
  DWORD dwContinueStatus = DBG_CONTINUE;
  DEBUG_EVENT debugEvent = {0};
  std::map<DWORD,std::wstring> module_map;
  EnumSharedModule(module_map);
  int debuggee_count = 0;
  while (bContinueDebugging){
    if(!WaitForDebugEvent(&debugEvent, INFINITE))
      break;
    switch (debugEvent.dwDebugEventCode) {
      case CREATE_PROCESS_DEBUG_EVENT:
        DebugRun_OnProcessCreated(debugEvent.dwProcessId,&debugEvent.u.CreateProcessInfo);
        debuggee_count++;
        break;
      case CREATE_THREAD_DEBUG_EVENT:
        DebugRun_OnThreadCreated(debugEvent.dwThreadId,&debugEvent.u.CreateThread);
        break;
      case EXCEPTION_DEBUG_EVENT:
        DebugRun_OnException(debugEvent.dwProcessId,debugEvent.dwThreadId,&debugEvent.u.Exception,dwContinueStatus);
        break;
      case EXIT_PROCESS_DEBUG_EVENT:
        DebugRun_OnProcessExited(debugEvent.dwProcessId,&debugEvent.u.ExitProcess);
        debuggee_count--;
        if(!debuggee_count)
          bContinueDebugging = false;
        break;
      case EXIT_THREAD_DEBUG_EVENT:
        DebugRun_OnThreadExited(debugEvent.dwThreadId,&debugEvent.u.ExitThread);
        break;
      case LOAD_DLL_DEBUG_EVENT:
        DebugRun_OnDllLoaded(module_map,&debugEvent.u.LoadDll);
        break;
      case UNLOAD_DLL_DEBUG_EVENT:
        DebugRun_OnDllUnloaded(module_map,&debugEvent.u.UnloadDll);
        break;
      case OUTPUT_DEBUG_STRING_EVENT:
        DebugRun_OnOutputDebugString(pi.hProcess,&debugEvent.u.DebugString);
        break;
      case RIP_EVENT:
        DebugRun_OnRipEvent(&debugEvent.u.RipInfo);
        break;
      default:
        DebugRun_Log(L"DebugRun",L"Unknown DebugEvent");
        break;
    }
    //可能有无效句柄导致异常=
    ContinueDebugEventIsSEHFrame(debugEvent.dwProcessId,debugEvent.dwThreadId,dwContinueStatus);
    // Reset
    dwContinueStatus = DBG_CONTINUE;
  }

  //debug exit
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  DebugRun_Log(L"DebugRun",L"exit");
}

//每次都重置一下DB线程，多进程调试时候，这样复位系统内部的port=
void DebugRun(){
  DCHECK(ProfilerThread::CurrentlyOn(ProfilerThread::UI));
  base::KillProcesses(L"360se.exe",0,NULL);
  base::WaitForProcessesToExit(L"360se.exe",base::TimeDelta::FromMilliseconds(100),NULL);
  g_profiler_process->reset_dbthread();
  ProfilerThread::PostTask(ProfilerThread::DB,FROM_HERE,base::Bind(DebugRun_DB));
}
//////////////////////////////////////////////////////////////////////////
void ParserMinidump_IO(std::wstring dump_path,std::wstring log_path){
  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();

  int64 ticks_end = base::TimeTicks::HighResNow().ToInternalValue();
  int64 delta_ms = ticks_end - ticks_start;

  CMiniDumpProber* prober = new CMiniDumpProber();
  bool bRet = prober->Open(dump_path,base::FilePath(dump_path).DirName().value());
  if(bRet){
    bRet = prober->Export(log_path);
  }
  delete prober;

  g_profiler_process->profiler_data()->RawAction(
    delta_ms,
    L"ParserMinidump", 
    bRet?base::StringPrintf(L"成功解析dump%d个,路径:%ls",1,log_path.c_str()):L"解析失败");
}

//在minidump所在目录搜索符号=
void ParserMinidump(HWND parent){
  std::wstring dump_path;
  if(!ChooseMinidumpFile(parent,dump_path)){
    return;
  }

  ProfilerThread::PostTask(ProfilerThread::IO,FROM_HERE,base::Bind(ParserMinidump_IO,
    dump_path,
    base::FilePath(dump_path).ReplaceExtension(L"txt").value()));
}

struct MyData
{
  WCHAR sz[64]; 
  DWORD dwMessageBox; 
};

DWORD __stdcall RMTFunc(MyData *pData)
{
  typedef int(__stdcall*MMessageBox)(HWND,LPCWSTR,LPCWSTR,UINT);
  MMessageBox MsgBox = (MMessageBox)pData->dwMessageBox;
  MsgBox(NULL, pData->sz, NULL, MB_OK);
  __asm int 3
  return 0;
}

void CrashIt(DWORD pid){
  if(!pid){
    int setup_count  = base::GetProcessCount(L"setup.exe",NULL);
    if(setup_count!=1)
      return;

    base::NamedProcessIterator iter(L"setup.exe", NULL);
    const base::ProcessEntry* entry = iter.NextProcessEntry();
    if(entry)
      pid = entry->pid();
    if(!pid)
      return;
  }

  HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS,FALSE,pid);
  if(!hProcess)
    return;

  do {
    MyData data;
    ZeroMemory(&data, sizeof (MyData));
    lstrcpy(data.sz, L"准备要崩溃啦");
    HINSTANCE hUser = GetModuleHandle(L"user32.dll");
    if (!hUser)
      break;
    data.dwMessageBox = (DWORD)GetProcAddress(hUser, "MessageBoxW");
    if (!data.dwMessageBox)
      break;
    void *pRemoteThread = VirtualAllocEx(hProcess, 0,1024*4, MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    if (!pRemoteThread)
      break;
    if (!WriteProcessMemory(hProcess, pRemoteThread, &RMTFunc, 1024*4, 0))
      break;
    MyData *pData = (MyData*)VirtualAllocEx(hProcess, 0,sizeof (MyData), MEM_COMMIT,PAGE_READWRITE);
    if (!pData)
      break;
    if (!WriteProcessMemory(hProcess, pData, &data, sizeof (MyData), 0))
      break;
    HANDLE hThread = CreateRemoteThread(hProcess, 0, 0, (LPTHREAD_START_ROUTINE)pRemoteThread,pData, 0, 0);
    if (!hThread)
      break;
    WaitForSingleObject(hThread,INFINITE);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemoteThread, 1024*4, MEM_RELEASE);
    VirtualFreeEx(hProcess, pData, sizeof (MyData), MEM_RELEASE);
  } while (0);

  CloseHandle(hProcess);
}

}