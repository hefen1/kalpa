// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Parts of this module come from:
//  http://www.codeproject.com/KB/applications/visualleakdetector.aspx
//       by Dan Moulding.
//  http://www.codeproject.com/KB/threads/StackWalker.aspx
//       by Jochen Kalmbach

#ifndef TOOLS_PROFILER_DLL_CALL_STACK_H_
#define TOOLS_PROFILER_DLL_CALL_STACK_H_

#include <windows.h>
#include <dbghelp.h>
#include <functional>
#include <map>
#include <string>

#include "base/logging.h"
#include "base/synchronization/lock.h"

// The CallStack Class
// A stack where gdi-handle has been allocated.
class CallStack {
 public:
  // Initialize for tracing CallStacks.
  static bool Initialize();

  CallStack();
  virtual ~CallStack() {}

  // Get a hash for this CallStack.
  // Identical stack traces will have matching hashes.
  int32 hash() { return hash_; }

  // Get a unique ID for this CallStack.
  // No two CallStacks will ever have the same ID.  The ID is a monotonically
  // increasing number.  Newer CallStacks always have larger IDs.
  int32 id() { return id_; }

  // Retrieves the frame at the specified index.
  DWORD_PTR frame(int32 index) {
    DCHECK(index < frame_count_ && index >= 0);
    return frames_[index];
  }

  // Compares the CallStack to another CallStack
  // for equality. Two CallStacks are equal if they are the same size and if
  // every frame in each is identical to the corresponding frame in the other.
  bool IsEqual(const CallStack &target);

  // valid?
  bool Valid() const { return valid_; }

 private:
  // The maximum number of frames to trace.
  static const int kMaxTraceFrames = 32;

  // Pushes a frame's program counter onto the CallStack.
  void AddFrame(DWORD_PTR programcounter);

  // Traces the stack, starting from this function, up to kMaxTraceFrames
  // frames.
  bool GetStackTrace();

  // Functions for manipulating the frame list.
  void ClearFrames();

  // Dynamically load the DbgHelp library and supporting routines that we
  // will use.
  static bool LoadDbgHelp();

  static void LockDbgHelp() {
    dbghelp_lock_.Acquire();
  }

  static void UnlockDbgHelp() {
    dbghelp_lock_.Release();
  }

  class AutoDbgHelpLock {
  public:
    AutoDbgHelpLock() {
      CallStack::LockDbgHelp();
    }
    ~AutoDbgHelpLock() {
      CallStack::UnlockDbgHelp();
    }
  };

  // According to http://msdn2.microsoft.com/en-us/library/ms680650(VS.85).aspx
  // "All DbgHelp functions, such as this one, are single threaded.  Therefore,
  // calls from more than one thread to this function will likely result in
  // unexpected behavior or memory corruption.  To avoid this, you must
  // synchromize all concurrent calls from one thread to this function."
  //
  // dbghelp_lock_ is used to serialize access across all calls to the DbgHelp
  // library.  This may be overly conservative (serializing them all together),
  // but does guarantee correctness.
  static base::Lock dbghelp_lock_;

  // Record the fact that dbghelp has been loaded.
  // Changes to this variable are protected by dbghelp_lock_.
  // It will only changes once... from false to true.
  static bool dbghelp_loaded_;

  int frame_count_;  // Current size (in frames)
  DWORD_PTR frames_[kMaxTraceFrames];
  int32 hash_;
  int32 id_;

  // Indicate is this is a valid stack.
  // This is false if recursion precluded a real stack generation.
  bool valid_;

  // Cache ProgramCounter -> Symbol lookups.
  // This cache is not thread safe.
  //typedef std::map<int32, std::string> SymbolCache;
  //static SymbolCache* symbol_cache_;

  DISALLOW_COPY_AND_ASSIGN(CallStack);
};

#endif  // TOOLS_PROFILER_DLL_CALL_STACK_H_
