#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "base/threading/non_thread_safe.h"
#include "tools/profiler_dll/profiler_data.h"

class CommandLine;

class ProfilerProcess : public base::NonThreadSafe{
public:
  explicit ProfilerProcess(const CommandLine& cmdline);
  virtual ~ProfilerProcess();

public:
  virtual base::Thread* io_thread();
  virtual base::Thread* file_thread();
  virtual base::Thread* db_thread();
  virtual base::Thread* launcher_thread();
  virtual ProfilerData* profiler_data();

private:
  void CreateIoThread();
  bool created_io_thread_;

  void CreateFileThread();
  bool created_file_thread_;

  void CreateLauncherThread();
  bool created_launcher_thread_;

  void CreateDbThread();
  bool created_db_thread_;

private:
  scoped_ptr<base::Thread> io_thread_;
  scoped_ptr<base::Thread> file_thread_;
  scoped_ptr<base::Thread> db_thread_;
  scoped_ptr<base::Thread> launcher_thread_;
  scoped_refptr<ProfilerData> profiler_data_;

  DISALLOW_COPY_AND_ASSIGN(ProfilerProcess);
};

extern ProfilerProcess* g_profiler_process;