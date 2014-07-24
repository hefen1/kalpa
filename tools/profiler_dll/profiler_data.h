#pragma once

#include <vector>
#include <string>
#include "base/memory/ref_counted.h"
#include "base/time/time.h"

#include "tools/profiler_dll/profiler_thread.h"
#include "tools/profiler_dll/log_client.h"

//不好做判断在UI线程上，这里是dll啦;开始就开一个线程当成UI线程
//在dllmain里面waitevent.wait死锁，先搞出一个thread好一些
class ProfilerData : public base::RefCountedThreadSafe<ProfilerData,
  ProfilerThread::DeleteOnUIThread>
{
public:
  ProfilerData();
  virtual ~ProfilerData();

  //切到转发线程FileThread上
  void Init(int64 ticks_ms,int pid,int tid);
  void Fini(int64 ticks_ms,int pid,int tid);
  //static bool Inited();

public:
  //可以在任意线程上调用
  static void Action(int64 time_ms,int64 delta_ms,
    int pid,int tid,
    const std::wstring& action,const std::wstring& data);
  static void InitCache();
  static void FiniCache();

private:
  //可以在任意线程上调用
  void DoAction(int64 ticks_ms,int64 delta_ms,int pid,int tid,
    const std::wstring& action,const std::wstring& data);

private:
  scoped_ptr<LogClient> log_client_;
};