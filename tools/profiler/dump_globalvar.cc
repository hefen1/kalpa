//////////////////////////////////////////////////////////////////////////
#include <windows.h>
#include <DbgHelp.h>
#include <strstream>
#include <vector>
#include "base/file_util.h"
#include "base/threading/thread_restrictions.h"

#pragma comment(lib,"dbghelp.lib")

typedef int  (__cdecl *_PIFV)(void);
typedef void (__cdecl *_PVFV)(void);

/* C initializers */
extern "C" _PIFV __xi_a[];
extern "C" _PIFV __xi_z[];    

/* C++ initializers */
extern "C" _PVFV __xc_a[];
extern "C" _PVFV __xc_z[];    

void __cdecl initterm_c (
                         _PIFV * pfbegin,
                         _PIFV * pfend,
                         std::vector<void*>& addrs
                         )
{
  while ( pfbegin < pfend)
  {
    if ( *pfbegin != NULL ){
      addrs.push_back( *pfbegin);
    }
    ++pfbegin;
  }
}

void __cdecl initterm_cpp (
                           _PVFV * pfbegin,
                           _PVFV * pfend,
                           std::vector<void*>& addrs
                           )
{
  while ( pfbegin < pfend )
  {
    if ( *pfbegin != NULL ){
      addrs.push_back( *pfbegin);
    }
    ++pfbegin;
  }
}

void DumpSymbolToStream(const void* const* trace,
                        int count,
                        std::ostream* os) 
{
  for (int i = 0; (i < count) && os->good(); ++i) {
    const int kMaxNameLength = 256;
    DWORD_PTR frame = reinterpret_cast<DWORD_PTR>(trace[i]);

    ULONG64 buffer[
      (sizeof(SYMBOL_INFO) +
        kMaxNameLength * sizeof(wchar_t) +
        sizeof(ULONG64) - 1) /
        sizeof(ULONG64)];
      memset(buffer, 0, sizeof(buffer));

      // Initialize symbol information retrieval structures.
      DWORD64 sym_displacement = 0;
      PSYMBOL_INFO symbol = reinterpret_cast<PSYMBOL_INFO>(&buffer[0]);
      symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
      symbol->MaxNameLen = kMaxNameLength - 1;
      BOOL has_symbol = SymFromAddr(GetCurrentProcess(), frame,
        &sym_displacement, symbol);

      // Attempt to retrieve line number information.
      DWORD line_displacement = 0;
      IMAGEHLP_LINE64 line = {};
      line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
      BOOL has_line = SymGetLineFromAddr64(GetCurrentProcess(), frame,
        &line_displacement, &line);

      // Output the backtrace line.
      (*os) << "\t";
      if (has_symbol) {
        (*os) << symbol->Name << " [0x" << trace[i] << "+"
          << sym_displacement << "]";
      } else {
        // If there is no symbol informtion, add a spacer.
        (*os) << "(No symbol) [0x" << trace[i] << "]";
      }
      if (has_line) {
        (*os) << " (" << line.FileName << ":" << line.LineNumber << ")";
      }
      (*os) << "\n";
  }
}

// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;
inline DWORD64 GetCurrentModuleBase() {
  return reinterpret_cast<DWORD64>(&__ImageBase);
}
inline DWORD GetCurrentModuleSize(){
  const void* start = &__ImageBase;
  const char* s = reinterpret_cast<const char*>(start);
  const IMAGE_NT_HEADERS32* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>
    (s + __ImageBase.e_lfanew);
  //const void* end = s + nt->OptionalHeader.SizeOfImage;
	return nt->OptionalHeader.SizeOfImage;
}

BOOL InitSymbol(HANDLE* process_out,DWORD64* sym_moudle_out)
{
  wchar_t file_dir[MAX_PATH] = {0};
  GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase),file_dir, MAX_PATH);
  wchar_t* file_name = wcsrchr(file_dir,L'\\');
  if(file_name) {
    file_name[0] = 0;
    file_name++;
  }else{
    return FALSE;
  }

  DWORD options = SymGetOptions();
  options |= SYMOPT_LOAD_ANYTHING;
  options |= SYMOPT_FAIL_CRITICAL_ERRORS;
  options |= SYMOPT_UNDNAME;
  options = SymSetOptions(options);

  HANDLE hProcess = GetCurrentProcess();
  if (!SymInitializeW(hProcess, NULL, FALSE)){
    return FALSE;
  }
  SymSetSearchPathW(hProcess,file_dir);

  wchar_t exe_path[MAX_PATH]={0};
  GetModuleFileNameW(0,exe_path, MAX_PATH);
  DWORD64 sym_moudle = SymLoadModule64(hProcess,0,(LPCSTR)exe_path,(LPCSTR)file_name,GetCurrentModuleBase(),GetCurrentModuleSize());
  if(!sym_moudle){
      SymCleanup(hProcess);
      return FALSE;
  }
  IMAGEHLP_MODULE64 moudle_info;
  moudle_info.SizeOfStruct = sizeof(moudle_info);
  if(!SymGetModuleInfo64(hProcess,sym_moudle,&moudle_info)){
    SymCleanup(hProcess);
    return FALSE;
  }

  *process_out = hProcess;
  *sym_moudle_out = sym_moudle;
  return TRUE;
}

//return 1 ok,if fail return 0
extern "C" __declspec(dllexport) int __cdecl DumpGlobalVar(DWORD magic,wchar_t* file_path) 
{
  std::strstream os;
  std::vector<void*> addrs;

  if(magic!=0x37213721 || !file_path)
    return 0;

  initterm_c( __xi_a,__xi_z,addrs);
  initterm_cpp( __xc_a, __xc_z,addrs);

  HANDLE process;
  DWORD64 sym_module;
  if(addrs.size() > 0  && InitSymbol(&process,&sym_module) ){
    base::ThreadRestrictions::ScopedAllowIO allow;
    DumpSymbolToStream(&addrs[0],addrs.size(),&os);
    file_util::WriteFile(FilePath(file_path),os.str(),os.pcount());
    SymUnloadModule64(process,sym_module);
    SymCleanup(process);
    return 1;
  }
  return 0;
}

//////////////////////////////////////////////////////////////////////////