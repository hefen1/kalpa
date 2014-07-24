#include "tools/profiler_dll/profiler_stub.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"

#include "tools/profiler_dll/profiler_thread.h"
#include "tools/profiler_dll/profiler_process.h"
#include "tools/profiler_dll/profiler_data.h"
#include "tools/profiler_dll/profiler_hook.h"

#include <new.h>

extern int _cdecl ProfilerMain(const CommandLine& command_line);

//////////////////////////////////////////////////////////////////////////

namespace{

  HINSTANCE g_dll_instance;
  HANDLE g_main_thread = INVALID_HANDLE_VALUE;

  DWORD WINAPI ThreadMain(LPVOID) {

    int argc = 0;
    char** argv = NULL;

    //profiler_main::LowLevelInit(hInstance);

    base::AtExitManager exit_manager;
    CommandLine::Init(argc, argv);
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();

    //logging::OldFileDeletionState file_state = logging::APPEND_TO_OLD_LOG_FILE;
    //logging::InitProfilerLogging(command_line, file_state);

    int exit_code = ProfilerMain(command_line);

    //profiler_main::LowLevelShutdown();
    //logging::CleanupProfilerLogging();

    return exit_code;
  }

  void CreateBackgroundThread() {
    g_main_thread = CreateThread(0,
      0,
      ThreadMain,
      0,
      0,
      0);
    DCHECK(g_main_thread != NULL);
  }

  void StopBackgroundThread() {
    DCHECK(g_profiler_process);
    DCHECK(g_main_thread);

    g_profiler_process->profiler_data()->Fini(
      base::TimeTicks::HighResNow().ToInternalValue(),GetCurrentProcessId(),GetCurrentThreadId());

    //让ui thread退出，做清理工作
    ProfilerThread::PostTask(ProfilerThread::UI,FROM_HERE,base::MessageLoop::QuitClosure());

    DWORD rv = WaitForSingleObject(g_main_thread, INFINITE);
    DCHECK(rv == WAIT_OBJECT_0);

    CloseHandle(g_main_thread);
  }

#pragma optimize("", off)
  void InvalidParameter(const wchar_t* expression, const wchar_t* function,
    const wchar_t* file, unsigned int line,
    uintptr_t reserved) 
  {
    __debugbreak();
    _exit(1);
  }

  void PureCall() 
  {
    __debugbreak();
    _exit(1);
  }

#pragma warning(push)
#pragma warning(disable : 4748)
  void OnNoMemory() {
    __debugbreak();
    _exit(1);
  }
#pragma warning(pop)
#pragma optimize("", on)

  void RegisterInvalidParamHandler() {
    _set_invalid_parameter_handler(InvalidParameter);
    _set_purecall_handler(PureCall);
    std::set_new_handler(&OnNoMemory);
    _set_new_mode(1);
  }

}

//////////////////////////////////////////////////////////////////////////

namespace ProfilerStub
{

  void ProcessAttach(HINSTANCE hInstance)
  {
    RegisterInvalidParamHandler();
    ProfilerData::InitCache();
    g_dll_instance = hInstance;
    CreateBackgroundThread();
    ProfilerHook::Hook();

    //等ui ipc线程和logclient初始化好，这里就sleep不能wait
    //faint,dllmain锁导致processattach里面创建的线程不能执行
    //因为threadattach也需要得到这个锁，所以不要做任何wait
    //和线程相关的指望了；可以先做一个list用于保存没有init
    //之前的log，等init好了后直接发给logclient；这里先
    //做个带锁的list
    //while(!ProfilerData::Inited()) 
    //  Sleep(100);
  }

  void ProcessDetach()
  {
    ProfilerHook::Unhook();
    //这里太晚了，自己开的线程都已经被干掉了
    StopBackgroundThread();
    g_dll_instance = NULL;
    ProfilerData::FiniCache();
  }

}

//////////////////////////////////////////////////////////////////////////