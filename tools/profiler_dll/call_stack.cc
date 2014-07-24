// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/profiler_dll/call_stack.h"
#include "base/strings/string_number_conversions.h"

//////////////////////////////////////////////////////////////////////////

// Typedefs for explicit dynamic linking with functions exported from
// dbghelp.dll.
typedef BOOL (__stdcall *t_StackWalk64)(DWORD, HANDLE, HANDLE,
                                        LPSTACKFRAME64, PVOID,
                                        PREAD_PROCESS_MEMORY_ROUTINE64,
                                        PFUNCTION_TABLE_ACCESS_ROUTINE64,
                                        PGET_MODULE_BASE_ROUTINE64,
                                        PTRANSLATE_ADDRESS_ROUTINE64);
typedef PVOID (__stdcall *t_SymFunctionTableAccess64)(HANDLE, DWORD64);
typedef DWORD64 (__stdcall *t_SymGetModuleBase64)(HANDLE, DWORD64);

static t_StackWalk64 pStackWalk64 = NULL;
static t_SymFunctionTableAccess64 pSymFunctionTableAccess64 = NULL;
static t_SymGetModuleBase64 pSymGetModuleBase64 = NULL;

#define LOADPROC(module, name)  do {                                    \
  p##name = reinterpret_cast<t_##name>(GetProcAddress(module, #name));  \
  if (p##name == NULL) return false;                                    \
} while (0)

static void UltraSafeDebugBreak() {
  _asm int(3);
}

//////////////////////////////////////////////////////////////////////////

// static
base::Lock CallStack::dbghelp_lock_;
bool CallStack::dbghelp_loaded_ = false;

// static
bool CallStack::LoadDbgHelp() {
  if (!dbghelp_loaded_) {
    base::AutoLock Lock(dbghelp_lock_);

    // Re-check if we've loaded successfully now that we have the lock.
    if (dbghelp_loaded_)
      return true;

    // Load dbghelp.dll, and obtain pointers to the exported functions that we
    // will be using.
    HMODULE dbghelp_module = LoadLibrary(L"dbghelp.dll");
    if (dbghelp_module) {
      LOADPROC(dbghelp_module, StackWalk64);
      LOADPROC(dbghelp_module, SymFunctionTableAccess64);
      LOADPROC(dbghelp_module, SymGetModuleBase64);
      dbghelp_loaded_ = true;
    } else {
      UltraSafeDebugBreak();
      return false;
    }
  }
  return dbghelp_loaded_;
}

bool CallStack::Initialize() {
  return LoadDbgHelp();
}

//////////////////////////////////////////////////////////////////////////

CallStack::CallStack() {
  static LONG callstack_id = 0;
  frame_count_ = 0;
  hash_ = 0;
  id_ = InterlockedIncrement(&callstack_id);
  valid_ = false;

  if (!dbghelp_loaded_) {
    UltraSafeDebugBreak();  // Initialize should have been called.
    return;
  }

  GetStackTrace();
}

bool CallStack::IsEqual(const CallStack &target) {
  if (frame_count_ != target.frame_count_)
    return false;  // They can't be equal if the sizes are different.

  // Walk the frames array until we
  // either find a mismatch, or until we reach the end of the call stacks.
  for (int index = 0; index < frame_count_; index++) {
    if (frames_[index] != target.frames_[index])
      return false;  // Found a mismatch. They are not equal.
  }

  // Reached the end of the call stacks. They are equal.
  return true;
}

void CallStack::AddFrame(DWORD_PTR pc) {
  DCHECK(frame_count_ < kMaxTraceFrames);
  frames_[frame_count_++] = pc;

  // Create a unique id for this CallStack.
  pc = pc + (frame_count_ * 13);  // Alter the PC based on position in stack.
  hash_ = ~hash_ + (pc << 15);
  hash_ = hash_ ^ (pc >> 12);
  hash_ = hash_ + (pc << 2);
  hash_ = hash_ ^ (pc >> 4);
  hash_ = hash_ * 2057;
  hash_ = hash_ ^ (pc >> 16);
}

//http://jpassing.com/2008/03/12/walking-the-stack-of-the-current-thread/
#ifdef _M_IX86
#pragma optimize( "g", off )
#pragma warning( push )
#pragma warning( disable : 4748 )
#endif

bool CallStack::GetStackTrace() {
  // Initialize the context record.
  CONTEXT context;
  memset(&context, 0, sizeof(context));
  context.ContextFlags = CONTEXT_FULL;
  __asm    call x
  __asm x: pop eax
  __asm    mov context.Eip, eax
  __asm    mov context.Ebp, ebp
  __asm    mov context.Esp, esp

  STACKFRAME64 frame;
  memset(&frame, 0, sizeof(frame));

#ifdef _M_IX86
  DWORD image_type = IMAGE_FILE_MACHINE_I386;
  frame.AddrPC.Offset    = context.Eip;
  frame.AddrPC.Mode      = AddrModeFlat;
  frame.AddrFrame.Offset = context.Ebp;
  frame.AddrFrame.Mode   = AddrModeFlat;
  frame.AddrStack.Offset = context.Esp;
  frame.AddrStack.Mode   = AddrModeFlat;
#elif
  NOT IMPLEMENTED!
#endif

  HANDLE current_process = GetCurrentProcess();
  HANDLE current_thread = GetCurrentThread();

  // Walk the stack.
  unsigned int count = 0;
  {
    AutoDbgHelpLock thread_monitoring_lock;

    while (count < kMaxTraceFrames) {
      count++;
      if (!pStackWalk64(image_type,
                        current_process,
                        current_thread,
                        &frame,
                        &context,
                        0,
                        pSymFunctionTableAccess64,
                        pSymGetModuleBase64,
                        NULL))
        break;  // Couldn't trace back through any more frames.

      if (frame.AddrFrame.Offset == 0)
        continue;  // End of stack.

      // Push this frame's program counter onto the provided CallStack.
      AddFrame((DWORD_PTR)frame.AddrPC.Offset);
    }
    valid_ = true;
  }
  return true;
}

#ifdef _M_IX86
#pragma warning( pop )
#pragma optimize( "g", on )
#endif

