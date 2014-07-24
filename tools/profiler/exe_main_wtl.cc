#include <atlbase.h>
#include <atlapp.h>
#include <malloc.h>
#include <new.h>
#include <shlobj.h>

namespace {

  CAppModule _Module;

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
}  // namespace

namespace profiler_main {

  void LowLevelInit(void* instance) {
    RegisterInvalidParamHandler();

    _Module.Init(NULL, static_cast<HINSTANCE>(instance));
  }

  void LowLevelShutdown() {
    _Module.Term();
  }
}
