#pragma once

//gdi��Դй©���=

#include <map>
#include <functional>

#include "base/synchronization/lock.h"

class CallStack;

class HandleWatcher {
public:
  struct StackTrack {
    CallStack* stack;
    int count;
  };

  typedef std::map<uint32, CallStack*> CallStackMap;
  typedef std::map<uint32, StackTrack> CallStackIdMap;

  HandleWatcher();
  virtual ~HandleWatcher();

  void OnTrack(uint32 id);
  int32 OnUntrack(uint32 id);

private:
  // MAX_VALID_HANDLE_COUNT�������ʼ����й©,������·�����ϲ�,
  // ����͵�һ��һ��,ֱ�ӱ���=
  static const int MAX_VALID_HANDLE_COUNT = 1000;
  bool FindLeak(CallStack* current_stack);

  // Either 0, or else the threadID for a thread that is actively working on
  // a stack track.  Used to avoid recursive tracking.
  base::Lock block_map_lock_;

  // The block_map provides quick lookups based on the allocation
  // pointer.  This is important for having fast round trips through
  // malloc/free.
  CallStackMap *block_map_;

  int32 topleak_callstack_hash_; 
};
