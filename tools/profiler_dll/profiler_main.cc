#include "tools/profiler_dll/profiler_main.h"

#include <windows.h>
#include <tchar.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/scoped_com_initializer.h"

//#include "tools/profiler_dll/profiler_logging.h"
//#include "tools/profiler_dll/profiler_window.h"
#include "tools/profiler_dll/profiler_thread.h"
#include "tools/profiler_dll/profiler_process.h"
#include "tools/profiler_dll/result_codes.h"
#include "tools/profiler_dll/profiler_init.h"

//////////////////////////////////////////////////////////////////////////

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
  // 有点意思
  main_thread_.reset(new ProfilerThread(ProfilerThread::UI,
    base::MessageLoop::current()));
}

//////////////////////////////////////////////////////////////////////////

namespace {
  //注意顺序，这里是柱塞调用，不能在dllmain里面
  void CreateChildThreads(ProfilerProcess* process) {
    process->io_thread();
    //process->db_thread();
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

  return result_code;
}

//////////////////////////////////////////////////////////////////////////