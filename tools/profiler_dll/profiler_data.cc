#include "tools/profiler_dll/profiler_data.h"

#include <Shlwapi.h>
#include "base/synchronization/lock_impl.h"
#include "base/strings/stringprintf.h"
#include "base/process/kill.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "tools/profiler_dll/profiler_ipc_messages.h"
#include "tools/profiler_dll/profiler_process.h"
#include "tools/profiler_dll/nt_loader.h"

//////////////////////////////////////////////////////////////////////////
namespace{
  bool g_logclient_init = false;
  std::queue<IPC::Message*>* g_message_cache = NULL;
  base::internal::LockImpl* g_cache_lock = NULL;
}

void ProfilerData::InitCache()
{
  g_message_cache = new std::queue<IPC::Message*>;
  g_cache_lock = new base::internal::LockImpl;
}

void ProfilerData::FiniCache()
{
  g_cache_lock->Lock();
  while(!g_message_cache->empty()){
    delete g_message_cache->front();
    g_message_cache->pop();
  }
  delete g_message_cache;
  g_cache_lock->Unlock();
  delete g_cache_lock;
}

//////////////////////////////////////////////////////////////////////////
void ProfilerData::Action(int64 time_ms,int64 delta_ms,
                          int pid,int tid,
                          const std::wstring& action,const std::wstring& data)
{
  if(g_logclient_init){
    g_profiler_process->profiler_data()->DoAction(time_ms,delta_ms,pid,tid,action,data);
  }else{
    g_cache_lock->Lock();
    int64 id = pid;
    id = (id<<32)+tid;
    g_message_cache->push(new ProfilerAction(time_ms,delta_ms,id,action,data));
    g_cache_lock->Unlock();
  }
}

//////////////////////////////////////////////////////////////////////////
ProfilerData::ProfilerData()
{
}

ProfilerData::~ProfilerData()
{
  log_client_.reset();
}

//////////////////////////////////////////////////////////////////////////
void ProfilerData::DoAction(int64 time_ms,int64 delta_ms,
                            int pid,int tid,
                            const std::wstring& action,const std::wstring& data)
{
  if(!ProfilerThread::CurrentlyOn(ProfilerThread::UI))
  {
    ProfilerThread::PostTask(ProfilerThread::UI,FROM_HERE,
      base::Bind(&ProfilerData::DoAction,this,time_ms,delta_ms,pid,tid,action,data));
    return;
  }

  int64 id = pid;
  id = (id<<32)+tid;
  log_client_->Send(new ProfilerAction(time_ms,delta_ms,id,action,data));
  return;
}

void ProfilerData::Init(int64 ticks_ms,int pid,int tid)
{
  if(!ProfilerThread::CurrentlyOn(ProfilerThread::UI)){
    ProfilerThread::PostTask(ProfilerThread::UI,FROM_HERE,
      base::Bind(&ProfilerData::Init,this,ticks_ms,pid,tid));
    return;
  }

  log_client_.reset(new LogClient);
  log_client_->Init();

  //���ñ�ǣ���֮��Ϳ���ipc����Ϣ��
  g_logclient_init = true;

  //Ȼ��messsagecache����Ķ���������g_memssage_cache��Ҫ������
  g_cache_lock->Lock();
  while(!g_message_cache->empty()){
    log_client_->Send(g_message_cache->front());
    g_message_cache->pop();
  }
  g_cache_lock->Unlock();

  //��һ������
  base::FilePath file_path;
  PathService::Get(base::FILE_EXE,&file_path);
  int64 id = pid;
  id = (id<<32)+tid;
  log_client_->Send(new ProfilerAction(ticks_ms,0,id,L"WinMain",file_path.value()));

  //ֻ����ʾǰ260���ַ�,windows�ؼ�������!�ּ��η��ɣ�ÿ�η�10��dll
  std::vector<std::wstring> modules;
  nt_loader::EnumAllModule(modules);

  int send_times = (modules.size()+9)/10;
  for(int send_index=0;send_index<send_times;++send_index){
    std::wstring dll_lists;
    dll_lists = base::StringPrintf(L"dlls(%d-%d):",send_index*10+1,std::min(send_index*10+10,(int)modules.size()));
    for(int i=send_index*10;i<std::min(send_index*10+10,(int)modules.size());++i){
      dll_lists.append(modules[i]);
      dll_lists.append(L",");
    }
    log_client_->Send(new ProfilerAction(ticks_ms,0,id,L"WinMain",dll_lists));
  }
}

void ProfilerData::Fini(int64 ticks_ms,int pid,int tid)
{
  if(!ProfilerThread::CurrentlyOn(ProfilerThread::UI)){
    ProfilerThread::PostTask(ProfilerThread::UI,FROM_HERE,
      base::Bind(&ProfilerData::Fini,this,ticks_ms,pid,tid));
    return;
  }

  base::FilePath file_path;
  PathService::Get(base::FILE_EXE,&file_path);
  int64 id = pid;
  id = (id<<32)+tid;
  log_client_->Send(new ProfilerAction(ticks_ms,0,id,L"�˳�WinMain",file_path.value()));

  //��100ms��ص���ûЧ��?���ⲿ��QuitTask���˳��������������
  //Sleep(100);
  //log_client_.reset();
}

//////////////////////////////////////////////////////////////////////////