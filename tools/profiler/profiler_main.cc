#include "tools/profiler/profiler_main.h"

#include <windows.h>
#include <tchar.h>
#include <shlobj.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/scoped_com_initializer.h"
#include "base/path_service.h"
#include "base/files/file_path.h"

#include "tools/profiler/profiler_logging.h"
#include "tools/profiler/profiler_window.h"
#include "tools/profiler/profiler_thread.h"
#include "tools/profiler/profiler_process.h"
#include "tools/profiler/result_codes.h"
#include "tools/profiler/profiler_init.h"
#include "tools/profiler/elevate.h"

//////////////////////////////////////////////////////////////////////////

static BOOL GetDebugPrivilege(HANDLE hProcess )
{
  BOOL              bResult = FALSE;
  HANDLE            hToken;
  TOKEN_PRIVILEGES  TokenPrivileges;

  if(OpenProcessToken(hProcess, TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES,&hToken))
  {
    TokenPrivileges.PrivilegeCount           = 1;
    LookupPrivilegeValue(NULL,SE_DEBUG_NAME,&TokenPrivileges.Privileges[0].Luid);

    TokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    bResult = AdjustTokenPrivileges(hToken,FALSE,&TokenPrivileges,sizeof(TOKEN_PRIVILEGES),NULL,NULL);
    CloseHandle(hToken);
  }

  return bResult;
}

ProfilerMainDelegate* ProfilerMainDelegate::CreateProfilerMainDelegate(
  const CommandLine& command_line) {
    return new ProfilerMainDelegate(command_line);
}

ProfilerMainDelegate::ProfilerMainDelegate(const CommandLine& command_line)
:parsed_command_line_(command_line) {
}

ProfilerMainDelegate::~ProfilerMainDelegate() {
}

void ProfilerMainDelegate::EarlyInitialization() {
  PreEarlyInitialization();

  //使用HighRes Clock
  base::Time::EnableHighResolutionTimer(true);

  PostEarlyInitialization();
}

void ProfilerMainDelegate::MainMessageLoopStart() {
  PreMainMessageLoopStart();
 
  main_message_loop_.reset(new base::MessageLoop(base::MessageLoop::TYPE_UI));
  InitializeMainThread();

  PostMainMessageLoopStart();
}

void ProfilerMainDelegate::InitializeMainThread() {
  const char* kThreadName = "Profiler_MainThread";
  base::PlatformThread::SetName(kThreadName);
  main_message_loop().set_thread_name(kThreadName);

  // Register the main thread by instantiating it, but don't call any methods.
  // 有点意思=
  main_thread_.reset(new ProfilerThread(ProfilerThread::UI,
    base::MessageLoop::current()));
}

//////////////////////////////////////////////////////////////////////////

namespace {
  //注意顺序，这里是柱塞调用，不能在dllmain里面=
  void CreateChildThreads(ProfilerProcess* process) {
    process->io_thread();
    process->db_thread();
    //process->file_thread();
    //process->launcher_thread();
  }

  void Shutdown() {
    base::ThreadRestrictions::SetIOAllowed(true);

    delete g_profiler_process;
    g_profiler_process = NULL;
  }

  void RunUIMessageLoop(ProfilerProcess* profiler_process) {
    base::ThreadRestrictions::SetIOAllowed(false);
    base::MessageLoopForUI::current()->Run();
  }

}

//////////////////////////////////////////////////////////////////////////

int _cdecl ProfilerMain(const CommandLine& command_line)
{
  //互斥=
  HANDLE hMutex = NULL;
  hMutex = CreateMutex( NULL, TRUE, L"{D21C5833-8693-4948-8E6E-43511F38726D}" );
  if( NULL != hMutex ){
    if( GetLastError() == ERROR_ALREADY_EXISTS ){
      CloseHandle(hMutex);
      return 0;
    }
  }

  //main thread
  scoped_ptr<ProfilerMainDelegate> parts(ProfilerMainDelegate::CreateProfilerMainDelegate(command_line));
  parts->EarlyInitialization();
  parts->MainMessageLoopStart();

  //sub threads
  scoped_ptr<ProfilerProcess> profiler_process(new ProfilerProcess(command_line));
  DCHECK(g_profiler_process);
  ProfilerInit profiler_init;
  CreateChildThreads(profiler_process.get());

  //COM
  base::win::ScopedCOMInitializer com_initializer;

  //ui
  int result_code = ResultCodes::NORMAL_EXIT;
  if(profiler_init.Start(command_line,&result_code)){
    RunUIMessageLoop(profiler_process.get());
  }

  //showdown
  ignore_result(profiler_process.release());
  Shutdown();

  //互斥=
  if(NULL!=hMutex){
    CloseHandle(hMutex);
  }

  return result_code;
}

int _cdecl IsNeedElevate(bool needelevate)
{
  //提权=
  if(needelevate)  {
    if(!CElevate::AutoElevate())    {
      return 0;
    }
  }
  
  //提权成功，检查运行路径=
  base::FilePath exe_path;
  PathService::Get(base::FILE_EXE,&exe_path);
  const wchar_t* p = exe_path.value().data();
  if(p && p[0]==L'\\'){
    MessageBox(GetActiveWindow(),L"请拷贝到本地运行",L"Profiler",MB_OK);
    return 0;
  }

  //获取调试权限=
  GetDebugPrivilege(GetCurrentProcess());
  return 1;
}

//////////////////////////////////////////////////////////////////////////