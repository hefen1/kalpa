#include "tools/profiler_dll/handle_watcher.h"

#include "base/base_paths.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/files/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "tools/profiler_dll/call_stack.h"
#include "tools/profiler_dll/profiler_thread.h"

#include <vector>
#include <algorithm>

// ---------------------------------------------------------------------
namespace{
  std::wstring Today(){
    time_t t = time(NULL);
    struct tm local_time = {0};
#if _MSC_VER >= 1400
    localtime_s(&local_time, &t);
#else
    localtime_r(&t, &local_time);
#endif
    struct tm* tm_time = &local_time;
    return base::StringPrintf(L"%04d-%02d-%02d-%02d-%02d-%02d",tm_time->tm_year+1900,tm_time->tm_mon+1,tm_time->tm_mday,
      tm_time->tm_hour,tm_time->tm_min,tm_time->tm_sec);
  }

  typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(
    IN HANDLE hProcess,
    IN DWORD ProcessId,
    IN HANDLE hFile,
    IN MINIDUMP_TYPE DumpType,
    IN CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam, OPTIONAL
    IN CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam, OPTIONAL
    IN CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam OPTIONAL
    );

  //fulldump:MiniDumpWithFullMemory 
  //minidump:MiniDumpNormal
  void CreateDumpFile(const base::FilePath& dump_path, DWORD dwProcessId, int miniDumpType,MINIDUMP_EXCEPTION_INFORMATION* exception_infor)
  {
    bool bRet = false;
    base::FilePath file_path;
    PathService::Get(base::DIR_EXE,&file_path);
    file_path = file_path.DirName().Append(L"dbghelp.dll");
    HMODULE dll = ::LoadLibrary(file_path.value().c_str());
    if(!dll){
      dll = ::LoadLibrary(L"dbghelp.dll");
    }
    if (dll) {
      MINIDUMPWRITEDUMP pWriteDumpFun = (MINIDUMPWRITEDUMP)::GetProcAddress(dll,"MiniDumpWriteDump");
      if (pWriteDumpFun) {
        // create the file
        base::ThreadRestrictions::ScopedAllowIO allow_io;

        std::wstring dump_file = base::StringPrintf(L"%ls\\day_%ls_pid_%d.dmp",dump_path.value().c_str(),Today().c_str(),dwProcessId);
        file_util::CreateDirectory(dump_path);
        HANDLE hFile = ::CreateFile(dump_file.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
          FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
          // write the dump
          HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ|THREAD_ALL_ACCESS,FALSE,dwProcessId);
          DCHECK(hProcess);
          bRet = !!pWriteDumpFun(hProcess, dwProcessId, hFile, (MINIDUMP_TYPE)miniDumpType, exception_infor, NULL, NULL);
          DCHECK(bRet);
          ::CloseHandle(hProcess);
          ::CloseHandle(hFile);
        }
      }
      ::FreeLibrary(dll);
    }
    return ;
  }
}


// ---------------------------------------------------------------------

HandleWatcher::HandleWatcher()
: topleak_callstack_hash_(0) {
  CallStack::Initialize();

  block_map_ = new CallStackMap();
}

HandleWatcher::~HandleWatcher() {
  delete block_map_;
}

void HandleWatcher::OnTrack(uint32 id) {
  if (id == 0)
    return;

  CallStack* stack = new CallStack();
  if (!stack->Valid()) 
    return;

  {
    base::AutoLock lock(block_map_lock_);

    CallStackMap::iterator block_it = block_map_->find(id);
    if (block_it != block_map_->end()) {
      delete block_it->second;
    }
    (*block_map_)[id] = stack;
  }

  //crash after unlock,avoid try
  if(FindLeak(stack)){
    static int times = 0;
    if(times%100)
      return;
    times++;

    base::FilePath file_path;
    PathService::Get(base::DIR_EXE,&file_path);
    file_path = file_path.Append(L"minidump");
    MessageBox(GetActiveWindow(),file_path.value().c_str(),L"GdiLeak dump:",MB_OK);
    ProfilerThread::PostTask(ProfilerThread::IO,FROM_HERE,
      base::Bind(&CreateDumpFile,file_path,GetCurrentProcessId(),MiniDumpNormal,(MINIDUMP_EXCEPTION_INFORMATION *)NULL));
    Sleep(3*1000);
  }
}

int32 HandleWatcher::OnUntrack(uint32 id) {
  int32 live_gdis = 0;
  if (id == 0)
    return live_gdis;

  {
    base::AutoLock lock(block_map_lock_);

    CallStackMap::iterator it = block_map_->find(id);
    if (it != block_map_->end()) {
      CallStack* stack = it->second;
      block_map_->erase(id);
      delete stack;
    }
    live_gdis = block_map_->size();
  }

  return live_gdis;
}

static bool CompareCallStackIdItems(HandleWatcher::StackTrack* left,
                                    HandleWatcher::StackTrack* right) {
  return left->count > right->count;
}

bool HandleWatcher::FindLeak(CallStack* current_stack) {
  base::AutoLock lock(block_map_lock_);

  if(block_map_->size() < MAX_VALID_HANDLE_COUNT){
    return false;
  }

  if(topleak_callstack_hash_){
    if(current_stack->hash()==topleak_callstack_hash_){
      return true;
    }
  }

  CallStackIdMap stack_map;
  for (CallStackMap::iterator block_it = block_map_->begin();
    block_it != block_map_->end(); ++block_it) {
      CallStack* stack = block_it->second;
      int32 stack_hash = stack->hash();
      CallStackIdMap::iterator it = stack_map.find(stack_hash);
      if (it == stack_map.end()) {
        StackTrack tracker;
        tracker.count = 1;
        tracker.stack = stack; 
        stack_map[stack_hash] = tracker;
      } else {
        it->second.count++;
      }
  }

  std::vector<StackTrack*> stack_tracks(stack_map.size());
  CallStackIdMap::iterator it = stack_map.begin();
  for (size_t i = 0; i < stack_tracks.size(); ++i) {
    stack_tracks[i] = &(it->second);
    ++it;
  }
  std::sort(stack_tracks.begin(), stack_tracks.end(), CompareCallStackIdItems);

  //如果泄漏最多的占了MAX_VALID_HANDLE_COUNT的五分之一，标记为要崩溃的stack=
  if(stack_tracks[0]->count > (MAX_VALID_HANDLE_COUNT/5)){
    topleak_callstack_hash_ = stack_tracks[0]->stack->hash();
    if(current_stack->hash()==topleak_callstack_hash_){
      return true;
    }
  }

  return false;
}
