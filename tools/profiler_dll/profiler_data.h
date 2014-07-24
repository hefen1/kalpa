#pragma once

#include <vector>
#include <string>
#include "base/memory/ref_counted.h"
#include "base/time/time.h"

#include "tools/profiler_dll/profiler_thread.h"
#include "tools/profiler_dll/log_client.h"

//�������ж���UI�߳��ϣ�������dll��;��ʼ�Ϳ�һ���̵߳���UI�߳�
//��dllmain����waitevent.wait�������ȸ��һ��thread��һЩ
class ProfilerData : public base::RefCountedThreadSafe<ProfilerData,
  ProfilerThread::DeleteOnUIThread>
{
public:
  ProfilerData();
  virtual ~ProfilerData();

  //�е�ת���߳�FileThread��
  void Init(int64 ticks_ms,int pid,int tid);
  void Fini(int64 ticks_ms,int pid,int tid);
  //static bool Inited();

public:
  //�����������߳��ϵ���
  static void Action(int64 time_ms,int64 delta_ms,
    int pid,int tid,
    const std::wstring& action,const std::wstring& data);
  static void InitCache();
  static void FiniCache();

private:
  //�����������߳��ϵ���
  void DoAction(int64 ticks_ms,int64 delta_ms,int pid,int tid,
    const std::wstring& action,const std::wstring& data);

private:
  scoped_ptr<LogClient> log_client_;
};