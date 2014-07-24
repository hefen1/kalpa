#pragma once

#include "base/bind.h"
#include "base/callback.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"

#if defined(UNIT_TEST)
#include "base/logging.h"
#endif  // UNIT_TEST

namespace base {
class MessageLoopProxy;
}

///////////////////////////////////////////////////////////////////////////////
// ProfilerThread
//
// This class represents a thread that is known by a browser-wide name.  For
// example, there is one IO thread for the entire browser process, and various
// pieces of code find it useful to retrieve a pointer to the IO thread's
// Invoke a task by thread ID:
//
//   ProfilerThread::PostTask(ProfilerThread::IO, FROM_HERE, task);
//
// The return value is false if the task couldn't be posted because the target
// thread doesn't exist.  If this could lead to data loss, you need to check the
// result and restructure the code to ensure it doesn't occur.
//
// This class automatically handles the lifetime of different threads.
// It's always safe to call PostTask on any thread.  If it's not yet created,
// the task is deleted.  There are no race conditions.  If the thread that the
// task is posted to is guaranteed to outlive the current thread, then no locks
// are used.  You should never need to cache pointers to MessageLoops, since
// they're not thread safe.
class ProfilerThread : public base::Thread {
 public:
  // An enumeration of the well-known threads.
  // NOTE: threads must be listed in the order of their life-time, with each
  // thread outliving every other thread below it.
  enum ID {
    // The main thread in the browser.
    UI,

    // This is the thread that processes IPC and network messages.
    IO,

    // This is the thread that interacts with the database.
    DB,

    // This is the thread that interacts with the file system.
    FILE,

    // Used to launch and terminate Profiler processes.
    PROCESS_LAUNCHER,

    // This identifier does not represent a thread.  Instead it counts the
    // number of well-known threads.  Insert new well-known threads before this
    // identifier.
    ID_COUNT
  };

  // Construct a ProfilerThread with the supplied identifier.  It is an error
  // to construct a ProfilerThread that already exists.
  explicit ProfilerThread(ID identifier);

  // Special constructor for the main (UI) thread and unittests. We use a dummy
  // thread here since the main thread already exists.
  ProfilerThread(ID identifier, base::MessageLoop* message_loop);

  virtual ~ProfilerThread();

  // These are the same methods in message_loop.h, but are guaranteed to either
  // get posted to the MessageLoop if it's still alive, or be deleted otherwise.
  // They return true iff the thread existed and the task was posted.  Note that
  // even if the task is posted, there's no guarantee that it will run, since
  // the target thread may already have a Quit message in its queue.
  static bool PostTask(ID identifier,
                       const tracked_objects::Location& from_here,
                       const base::Closure& task);
/*
  static bool PostDelayedTask(ID identifier,
                              const tracked_objects::Location& from_here,
                              const base::Closure& task,
                              int64 delay_ms);
*/
  static bool PostDelayedTask(ID identifier,
                            const tracked_objects::Location& from_here,
                            const base::Closure& task,
                            base::TimeDelta delay);
  static bool PostNonNestableTask(ID identifier,
                                  const tracked_objects::Location& from_here,
                                  const base::Closure& task);
/*
  static bool PostNonNestableDelayedTask(
      ID identifier,
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      int64 delay_ms);
*/
  static bool PostNonNestableDelayedTask(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay);

  template <class T>
  static bool DeleteSoon(ID identifier,
                         const tracked_objects::Location& from_here,
                         const T* object) {
    return GetMessageLoopProxyForThread(identifier)->DeleteSoon(from_here,object);
  }

  template <class T>
  static bool ReleaseSoon(ID identifier,
                          const tracked_objects::Location& from_here,
                          const T* object) {
    return GetMessageLoopProxyForThread(identifier)->ReleaseSoon(from_here,object);
  }

  // Callable on any thread.  Returns whether the given ID corresponds to a well
  // known thread.
  static bool IsWellKnownThread(ID identifier);

  // Callable on any thread.  Returns whether you're currently on a particular
  // thread.
  static bool CurrentlyOn(ID identifier);

  // Callable on any thread.  Returns whether the threads message loop is valid.
  // If this returns false it means the thread is in the process of shutting
  // down.
  static bool IsMessageLoopValid(ID identifier);

  // If the current message loop is one of the known threads, returns true and
  // sets identifier to its ID.  Otherwise returns false.
  static bool GetCurrentThreadIdentifier(ID* identifier);

  // Callers can hold on to a refcounted MessageLoopProxy beyond the lifetime
  // of the thread.
  static scoped_refptr<base::MessageLoopProxy> GetMessageLoopProxyForThread(
      ID identifier);

  // Use these templates in conjuction with RefCountedThreadSafe when you want
  // to ensure that an object is deleted on a specific thread.  This is needed
  // when an object can hop between threads (i.e. IO -> FILE -> IO), and thread
  // switching delays can mean that the final IO tasks executes before the FILE
  // task's stack unwinds.  This would lead to the object destructing on the
  // FILE thread, which often is not what you want (i.e. to unregister from
  // NotificationService, to notify other objects on the creating thread etc).
  template<ID thread>
  struct DeleteOnThread {
    template<typename T>
    static void Destruct(const T* x) {
      if (CurrentlyOn(thread)) {
        delete x;
      } else {
        if (!DeleteSoon(thread, FROM_HERE, x)) {
#if defined(UNIT_TEST)
          // Only logged under unit testing because leaks at shutdown
          // are acceptable under normal circumstances.
          LOG(ERROR) << "DeleteSoon failed on thread " << thread;
#endif  // UNIT_TEST
        }
      }
    }
  };

  // Sample usage:
  // class Foo
  //     : public base::RefCountedThreadSafe<
  //           Foo, ProfilerThread::DeleteOnIOThread> {
  //
  // ...
  //  private:
  //   friend struct ProfilerThread::DeleteOnThread<ProfilerThread::IO>;
  //   friend class DeleteTask<Foo>;
  //
  //   ~Foo();
  struct DeleteOnUIThread : public DeleteOnThread<UI> { };
  struct DeleteOnIOThread : public DeleteOnThread<IO> { };
  struct DeleteOnFileThread : public DeleteOnThread<FILE> { };
  struct DeleteOnDBThread : public DeleteOnThread<DB> { };

 private:
  // Common initialization code for the constructors.
  void Initialize();
/*
  static bool PostTaskHelper(
      ID identifier,
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      int64 delay_ms,
      bool nestable);
*/
  static bool PostTaskHelper(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay,
    bool nestable);

  // The identifier of this thread.  Only one thread can exist with a given
  // identifier at a given time.
  ID identifier_;

  // This lock protects |profiler_threads_|.  Do not read or modify that array
  // without holding this lock.  Do not block while holding this lock.
  static base::Lock lock_;

  // An array of the ProfilerThread objects.  This array is protected by |lock_|.
  // The threads are not owned by this array.  Typically, the threads are owned
  // on the UI thread by the g_profiler_process object.  BrowserThreads remove
  // themselves from this array upon destruction.
  static ProfilerThread* profiler_threads_[ID_COUNT];
};
