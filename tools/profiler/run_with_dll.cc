#include <iostream>
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "third_party/detours/src/detours.h"

//WM_COPYDATA....
void NotifyHWND(std::wstring hwnd_str,int64 ticks_start,int64 ticks_end,bool launch_ok){
  struct cp_tag{
    int64 ticks_start;
    int64 ticks_end;
    int launch_ok;
  };
  cp_tag cp_data = {ticks_start,ticks_end,launch_ok};

  HWND hwnd = NULL;
  base::StringToUint(hwnd_str,(UINT*)&hwnd);
  COPYDATASTRUCT cds;
  cds.dwData = 0;
  cds.cbData = static_cast<DWORD>(sizeof(cp_data));
  cds.lpData = (VOID*)(&cp_data);
  
  DWORD_PTR result = 0;
  SendMessageTimeout(hwnd,
    WM_COPYDATA,
    NULL,
    reinterpret_cast<LPARAM>(&cds),
    SMTO_ABORTIFHUNG,
    1 * 1000,
    &result);
}

BOOL RunWithDLL(std::wstring exe_path, std::wstring dll_path, std::wstring detoured_path,std::wstring hwnd_str)
{
  STARTUPINFO si = {0};
  PROCESS_INFORMATION pi = {0};

  si.cb= sizeof(si);
  GetStartupInfo( &si );
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_SHOW;

  //开始计时
  int64 ticks_start = base::TimeTicks::HighResNow().ToInternalValue();

  BOOL bRet =  DetourCreateProcessWithDllW(
    NULL,
    (LPWSTR)exe_path.c_str(),
    NULL,
    NULL,
    FALSE,
    CREATE_DEFAULT_ERROR_MODE | CREATE_SUSPENDED,
    NULL,
    NULL,
    &si,
    &pi,
    WideToASCII(detoured_path).c_str(),
    WideToASCII(dll_path).c_str(),
    NULL);
  if(bRet){
    ResumeThread(pi.hThread);

    //create ok
    int64  tick_end  = base::TimeTicks::HighResNow().ToInternalValue();
    NotifyHWND(hwnd_str,ticks_start,tick_end,true);

    WaitForSingleObject(pi.hProcess,INFINITE);

    CloseHandle( pi.hThread );
    CloseHandle( pi.hProcess );

    return TRUE;
  }
  
  //create fail
  int64  tick_end  = base::TimeTicks::HighResNow().ToInternalValue();
  NotifyHWND(hwnd_str,ticks_start,tick_end,false);

  return FALSE;
}

int Usage() {
  CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  std::cout << "Usage: " << cmd_line->GetProgram().BaseName().value().c_str()
            << " like this" << std::endl;
  std::cout << "--exe=%s --dll=%s --detoured=%s -- exe_args" << std::endl;

  return 1;
}

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  CommandLine::Init(argc, argv);

  base::Time::EnableHighResolutionTimer(true);

  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  std::wstring exe_path = cmd_line->GetSwitchValueNative("exe");
  std::wstring hwnd_str = cmd_line->GetSwitchValueNative("hwnd");
  std::wstring dll_path = cmd_line->GetSwitchValueNative("dll");
  std::wstring detoured_path = cmd_line->GetSwitchValueNative("detoured");
  CommandLine::StringVector exe_args = cmd_line->GetArgs();
  if (exe_path.empty() || dll_path.empty() || detoured_path.empty()) {
    return Usage();
  }

  base::FilePath file_path(exe_path);
  CommandLine new_cmd(file_path);
  for (size_t i = 0; i < exe_args.size(); ++i){
    new_cmd.AppendArgNative(exe_args[i]);
  }

  RunWithDLL(new_cmd.GetCommandLineString(),dll_path,detoured_path,hwnd_str);
  return 0;
}
