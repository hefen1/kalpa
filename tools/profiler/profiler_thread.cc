// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/profiler/profiler_thread.h"

#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy_impl.h"
#include "base/threading/thread_restrictions.h"

// Friendly names for the well-known threads.
static const char* profiler_thread_names[ProfilerThread::ID_COUNT] = {
  "",  // UI (name assembled in profiler_main.cc).
  "Profiler_IOThread",  // IO
  "Profiler_DBThread",  // DB
  "Profiler_FileThread",  // FILE
  "Profiler_ProcessLauncherThread",  // PROCESS_LAUNCHER
};

// An implementation of MessageLoopProxy to be used in conjunction
// with ProfilerThread.
class ProfilerThreadMessageLoopProxy : public base::MessageLoopProxy {
public:
  explicit ProfilerThreadMessageLoopProxy(ProfilerThread::ID identifier)
    : id_(identifier) {
  }

  // MessageLoopProxy implementation.
/*
  virtual bool PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task, int64 delay_ms) OVERRIDE {
      return ProfilerThread::PostDelayedTask(id_, from_here, task, delay_ms);
  }
*/
  virtual bool PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task, base::TimeDelta delay) OVERRIDE {
      return ProfilerThread::PostDelayedTask(id_, from_here, task, delay);
  }
/*
  virtual bool PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    int64 delay_ms) OVERRIDE {
      return ProfilerThread::PostNonNestableDelayedTask(id_, from_here, task,
        delay_ms);
  }
*/
  virtual bool PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) OVERRIDE {
      return ProfilerThread::PostNonNestableDelayedTask(id_, from_here, task,
        delay);
  }

  virtual bool RunsTasksOnCurrentThread() const OVERRIDE {
    return ProfilerThread::CurrentlyOn(id_);
  }

protected:
  virtual ~ProfilerThreadMessageLoopProxy() {}

private:
  ProfilerThread::ID id_;
  DISALLOW_COPY_AND_ASSIGN(ProfilerThreadMessageLoopProxy);
};


base::Lock ProfilerThread::lock_;

ProfilerThread* ProfilerThread::profiler_threads_[ID_COUNT];

ProfilerThread::ProfilerThread(ProfilerThread::ID identifier)
    : Thread(profiler_thread_names[identifier]),
      identifier_(identifier) {
  Initialize();
}

ProfilerThread::ProfilerThread(ID identifier, base::MessageLoop* message_loop)
    : Thread(message_loop->thread_name().c_str()),
      identifier_(identifier) {
  set_message_loop(message_loop);
  Initialize();
}

void ProfilerThread::Initialize() {
  base::AutoLock lock(lock_);
  DCHECK(identifier_ >= 0 && identifier_ < ID_COUNT);
  DCHECK(profiler_threads_[identifier_] == NULL);
  profiler_threads_[identifier_] = this;
}

ProfilerThread::~ProfilerThread() {
  // Stop the thread here, instead of the parent's class destructor.  This is so
  // that if there are pending tasks that run, code that checks that it's on the
  // correct ProfilerThread succeeds.
  Stop();

  base::AutoLock lock(lock_);
  profiler_threads_[identifier_] = NULL;
#ifndef NDEBUG
  // Double check that the threads are ordered correctly in the enumeration.
  for (int i = identifier_ + 1; i < ID_COUNT; ++i) {
    DCHECK(!profiler_threads_[i]) <<
        "Threads must be listed in the reverse order that they die";
  }
#endif
}

// static
bool ProfilerThread::IsWellKnownThread(ID identifier) {
  base::AutoLock lock(lock_);
  return (identifier >= 0 && identifier < ID_COUNT &&
          profiler_threads_[identifier]);
}

// static
bool ProfilerThread::CurrentlyOn(ID identifier) {
  // We shouldn't use MessageLoop::current() since it uses LazyInstance which
  // may be deleted by ~AtExitManager when a WorkerPool thread calls this
  // function.
  // http://crbug.com/63678
  base::ThreadRestrictions::ScopedAllowSingleton allow_singleton;
  base::AutoLock lock(lock_);
  DCHECK(identifier >= 0 && identifier < ID_COUNT);
  return profiler_threads_[identifier] &&
         profiler_threads_[identifier]->message_loop() == base::MessageLoop::current();
}

// static
bool ProfilerThread::IsMessageLoopValid(ID identifier) {
  base::AutoLock lock(lock_);
  DCHECK(identifier >= 0 && identifier < ID_COUNT);
  return profiler_threads_[identifier] &&
         profiler_threads_[identifier]->message_loop();
}

// static
bool ProfilerThread::PostTask(ID identifier,
                            const tracked_objects::Location& from_here,
                            const base::Closure& task) {
  return PostTaskHelper(identifier, from_here, task, base::TimeDelta(), true);
}
/*
// static
bool ProfilerThread::PostDelayedTask(ID identifier,
                                   const tracked_objects::Location& from_here,
                                   const base::Closure& task,
                                   int64 delay_ms) {
  return PostTaskHelper(identifier, from_here, task, delay_ms, true);
}
*/
// static
bool ProfilerThread::PostDelayedTask(ID identifier,
                                    const tracked_objects::Location& from_here,
                                    const base::Closure& task,
                                    base::TimeDelta delay) {
  return PostTaskHelper( identifier, from_here, task, delay, true);
}

// static
bool ProfilerThread::PostNonNestableTask(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  return PostTaskHelper(identifier, from_here, task, base::TimeDelta(), false);
}
/*
// static
bool ProfilerThread::PostNonNestableDelayedTask(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    int64 delay_ms) {
  return PostTaskHelper(identifier, from_here, task, delay_ms, false);
}
*/
// static
bool ProfilerThread::PostNonNestableDelayedTask(
  ID identifier,
  const tracked_objects::Location& from_here,
  const base::Closure& task,
  base::TimeDelta delay) {
    return PostTaskHelper(identifier, from_here, task, delay, false);
}

// static
bool ProfilerThread::GetCurrentThreadIdentifier(ID* identifier) {
  // We shouldn't use MessageLoop::current() since it uses LazyInstance which
  // may be deleted by ~AtExitManager when a WorkerPool thread calls this
  // function.
  // http://crbug.com/63678
  base::ThreadRestrictions::ScopedAllowSingleton allow_singleton;
  base::MessageLoop* cur_message_loop = base::MessageLoop::current();
  for (int i = 0; i < ID_COUNT; ++i) {
    if (profiler_threads_[i] &&
        profiler_threads_[i]->message_loop() == cur_message_loop) {
      *identifier = profiler_threads_[i]->identifier_;
      return true;
    }
  }

  return false;
}

// static
scoped_refptr<base::MessageLoopProxy>
ProfilerThread::GetMessageLoopProxyForThread(
    ID identifier) {
  scoped_refptr<base::MessageLoopProxy> proxy(
      new ProfilerThreadMessageLoopProxy(identifier));
  return proxy;
}
/*
// static
bool ProfilerThread::PostTaskHelper(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    int64 delay_ms,
    bool nestable) {
  DCHECK(identifier >= 0 && identifier < ID_COUNT);
  // Optimization: to avoid unnecessary locks, we listed the ID enumeration in
  // order of lifetime.  So no need to lock if we know that the other thread
  // outlives this one.
  // Note: since the array is so small, ok to loop instead of creating a map,
  // which would require a lock because std::map isn't thread safe, defeating
  // the whole purpose of this optimization.
  ID current_thread;
  bool guaranteed_to_outlive_target_thread =
      GetCurrentThreadIdentifier(&current_thread) &&
      current_thread >= identifier;

  if (!guaranteed_to_outlive_target_thread)
    lock_.Acquire();

  MessageLoop* message_loop = profiler_threads_[identifier] ?
      profiler_threads_[identifier]->message_loop() : NULL;
  if (message_loop) {
    if (nestable) {
      message_loop->PostDelayedTask(from_here, task, delay_ms);
    } else {
      message_loop->PostNonNestableDelayedTask(from_here, task, delay_ms);
    }
  }

  if (!guaranteed_to_outlive_target_thread)
    lock_.Release();

  return !!message_loop;
}
*/
// static
bool ProfilerThread::PostTaskHelper(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay,
    bool nestable) {
  DCHECK(identifier >= 0 && identifier < ID_COUNT);
  // Optimization: to avoid unnecessary locks, we listed the ID enumeration in
  // order of lifetime.  So no need to lock if we know that the other thread
  // outlives this one.
  // Note: since the array is so small, ok to loop instead of creating a map,
  // which would require a lock because std::map isn't thread safe, defeating
  // the whole purpose of this optimization.
  ID current_thread;
  bool guaranteed_to_outlive_target_thread =
      GetCurrentThreadIdentifier(&current_thread) &&
      current_thread >= identifier;

  if (!guaranteed_to_outlive_target_thread)
    lock_.Acquire();

  base::MessageLoop* message_loop = profiler_threads_[identifier] ?
      profiler_threads_[identifier]->message_loop() : NULL;
  if (message_loop) {
    if (nestable) {
      message_loop->PostDelayedTask(from_here, task, delay);
    } else {
      message_loop->PostNonNestableDelayedTask(from_here, task, delay);
    }
  }

  if (!guaranteed_to_outlive_target_thread)
    lock_.Release();

  return !!message_loop;
}
