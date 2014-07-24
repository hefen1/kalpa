#include "tools/profiler/nt_loader.h"

#include <winnt.h>
#include <winternl.h>
#include "base/logging.h"

namespace {
  void AssertIsCriticalSection(CRITICAL_SECTION* critsec) {
    // Assert on some of the internals of the debug info if it has one.
    RTL_CRITICAL_SECTION_DEBUG* debug = critsec->DebugInfo;
    if (debug) {
      CHECK_EQ(RTL_CRITSECT_TYPE, debug->Type);
      CHECK_EQ(critsec, debug->CriticalSection);
    }
  }

  class ScopedEnterCriticalSection {
  public:
    explicit ScopedEnterCriticalSection(CRITICAL_SECTION* critsec)
      : critsec_(critsec) {
        AssertIsCriticalSection(critsec_);
        ::EnterCriticalSection(critsec_);
    }

    ~ScopedEnterCriticalSection() {
      ::LeaveCriticalSection(critsec_);
    }

  private:
    CRITICAL_SECTION* critsec_;
  };

  std::wstring FromUnicodeString(const UNICODE_STRING& str) {
    return std::wstring(str.Buffer, str.Length / sizeof(str.Buffer[0]));
  }

}  // namespace

namespace nt_loader {

LDR_DATA_TABLE_ENTRY* GetLoaderEntry(HMODULE module) {
  // Make sure we own the loader's lock on entry here.
  CHECK(OwnsCriticalSection(GetLoaderLock()));
  PEB* peb = GetCurrentPeb();

  LIST_ENTRY* head = &peb->Ldr->InLoadOrderModuleList;
  for (LIST_ENTRY* entry = head->Flink; entry != head; entry = entry->Flink) {
    LDR_DATA_TABLE_ENTRY* ldr_entry =
        CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

    if (reinterpret_cast<HMODULE>(ldr_entry->DllBase) == module)
      return ldr_entry;
  }

  return NULL;
}

void EnumAllModule(std::map<DWORD,std::wstring>& module_map)
{
  ScopedEnterCriticalSection lock(GetLoaderLock());

  PEB* peb = GetCurrentPeb();
  LIST_ENTRY* head = &peb->Ldr->InLoadOrderModuleList;
  for (LIST_ENTRY* entry = head->Flink; entry != head; entry = entry->Flink) {
    LDR_DATA_TABLE_ENTRY* ldr_entry =
      CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

    ULONG flags = ldr_entry->Flags;
    if(0 == (flags & LDRP_ENTRY_PROCESSED))
      continue;
    if (0 == (flags & LDRP_IMAGE_DLL)) {
      continue; //exe
    }
    if(ldr_entry->EntryPoint == NULL){
      CHECK(!(flags & LDRP_PROCESS_ATTACH_CALLED));
      continue;
    }

    module_map[(DWORD)ldr_entry->DllBase] = FromUnicodeString(ldr_entry->BaseDllName);
  }
}

}  // namespace nt_loader
