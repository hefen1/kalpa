#include <windows.h>
#include "tools/profiler_dll/profiler_stub.h"

extern "C" {
BOOL WINAPI DllMain(HINSTANCE dll_instance, DWORD reason,
                              LPVOID reserved) {
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      ProfilerStub::ProcessAttach(dll_instance);
      break;
    case DLL_PROCESS_DETACH:
      ProfilerStub::ProcessDetach();
      break;
  }
  return TRUE;
}

//���뵼��һ����������Ȼ��detours�ͱ���fuck!!!!
__declspec(dllexport) void __cdecl DetoursNeed() {
}

}  // extern "C"
