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

    //��ui thread�˳�����������
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

    //��ui ipc�̺߳�logclient��ʼ���ã������sleep����wait
    //faint,dllmain������processattach���洴�����̲߳���ִ��
    //��ΪthreadattachҲ��Ҫ�õ�����������Բ�Ҫ���κ�wait
    //���߳���ص�ָ���ˣ���������һ��list���ڱ���û��init
    //֮ǰ��log����init���˺�ֱ�ӷ���logclient��������
    //����������list
    //while(!ProfilerData::Inited()) 
    //  Sleep(100);
  }

  void ProcessDetach()
  {
    ProfilerHook::Unhook();
    //����̫���ˣ��Լ������̶߳��Ѿ����ɵ���
    StopBackgroundThread();
    g_dll_instance = NULL;
    ProfilerData::FiniCache();
  }

}

//////////////////////////////////////////////////////////////////////////