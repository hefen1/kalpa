#include <windows.h>

bool IsChromeExe() {
#if 0
  return GetModuleHandleA("Chrome.exe") != NULL;
#else
  return (GetModuleHandleA("Chrome.exe") != NULL || GetModuleHandleA("360Chrome.exe") != NULL || GetModuleHandleA("360se.exe") != NULL);
#endif 
}

extern "C" {
BOOL WINAPI DllMain(HINSTANCE dll_instance, DWORD reason,
                              LPVOID reserved) {
  if (!IsChromeExe())
    return FALSE;

  switch (reason) {
    case DLL_PROCESS_ATTACH:
      break;
    case DLL_PROCESS_DETACH:
      break;
  }
  return TRUE;
}

__declspec(dllexport) void __cdecl SetLogName(char* name) {
}

}  // extern "C"
