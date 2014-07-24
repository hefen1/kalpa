#include "tools/profiler/profiler_init.h"
#include "tools/profiler/profiler_window.h"
#include "tools/profiler/result_codes.h"

ProfilerInit::ProfilerInit()
{

}

ProfilerInit::~ProfilerInit()
{

}

bool ProfilerInit::Start(const CommandLine& cmdline,int* result_code)
{
  ProfilerWindow* profiler_window = new ProfilerWindow;
  if(!profiler_window){
    *result_code = ResultCodes::OUTOFMEM_EXIT;
    return false;
  }
  //profiler_window->set_window_style(profiler_window->window_style()|WS_EX_TOPMOST);

  profiler_window->Init(NULL, gfx::Rect());
  ::SetWindowText(profiler_window->hwnd(),L"360ä¯ÀÀÆ÷ Profiler");

  ::ShowWindow(profiler_window->hwnd(),SW_SHOWNORMAL);
  ::UpdateWindow(profiler_window->hwnd());

  return true;
}
