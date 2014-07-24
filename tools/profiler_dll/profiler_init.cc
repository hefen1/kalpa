#include "tools/profiler_dll/profiler_init.h"

#include "base/time/time.h"
//#include "tools/profiler_dll/profiler_window.h"
#include "tools/profiler_dll/result_codes.h"
#include "tools/profiler_dll/profiler_process.h"

ProfilerInit::ProfilerInit()
{

}

ProfilerInit::~ProfilerInit()
{

}

bool ProfilerInit::Start(const CommandLine& cmdline,int* result_code)
{
/*
  ProfilerWindow* profiler_window = new ProfilerWindow;
  if(!profiler_window){
    *result_code = ResultCodes::OUTOFMEM_EXIT;
    return false;
  }

  profiler_window->Init(NULL, gfx::Rect());
  ::SetWindowText(profiler_window->hwnd(),L"Chrome Profiler");

  ::ShowWindow(profiler_window->hwnd(),SW_SHOWNORMAL);
  ::UpdateWindow(profiler_window->hwnd());
*/

  g_profiler_process->profiler_data()->Init(
    base::TimeTicks::HighResNow().ToInternalValue(),GetCurrentProcessId(),GetCurrentThreadId());
  return true;
}