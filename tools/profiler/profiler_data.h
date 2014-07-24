#pragma once

#include <vector>
#include <map>
#include <string>
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/files/file_path.h"

#include "tools/profiler/profiler_thread.h"

namespace base{
  class ProcessMetrics;
}

class ProfilerData : public base::RefCountedThreadSafe<ProfilerData,
  ProfilerThread::DeleteOnUIThread>
{
public:
  struct ProcData{
    std::wstring pid;
    std::wstring private_memory; //M
    std::wstring shared_memory;
    std::wstring cpu; //%
    std::wstring startup_ms;
    std::wstring cmd_line;

  public:
    bool is_mainprocess;
    base::Time startup_time;
  };
  class Delegate{
  public:
    virtual void OnAction(const std::wstring& time_ms,const std::wstring& delta_ms,
      const std::wstring& pid,const std::wstring& tid,
      const std::wstring& action,const std::wstring& data) = 0;
    virtual void OnProcData(const std::vector<ProcData>& proc_data) = 0;
  };
  ProfilerData();
  virtual ~ProfilerData();

  //给ui delegate
  void Init(Delegate* delegate);
  struct AppData{
    std::wstring app_name;
    std::wstring app_filename;
    std::wstring exe_path;
    std::wstring exe_params;
    std::wstring profiler_path;
    std::wstring copyfrom_path;
    std::wstring copyto_path;
    std::wstring detoured_path;
    bool copy_file;
    bool cold_start;
  };
  int GetAppCount();
  bool GetAppData(size_t index,AppData& appdata);
  void GetAppDatas(std::vector<AppData>& appdatas);
  void ProfileApp(size_t id,HWND hwnd);
  void LaunchOk(int64 ticks_start,int64 ticks_end,bool launch_ok);//<==WM_COPYDATA后转给IO
  void QueryProcData();
  void EnumProcByName(const std::wstring& proc_name);

  //给ipcdaemon用
public:
  //可以在任意线程上调用
  void Action(int64 time_ms,int64 delta_ms,
    int pid,int tid,
    const std::wstring& action,const std::wstring& data);

  //给内部独立命令用,可以在任意线程上调用
  void RawAction(int64 delta_ms,const std::wstring& action,
    const std::wstring& data);

  //专门给debug用
  void DebugAction(const std::wstring& action,const std::wstring& data);

  //可以在任意线程上调用
  void HandleProcData(const std::vector<ProcData>& proc_data);

  //helper functions
public:
  static BOOL GetDebugPrivilege(HANDLE hProcess );
  static std::wstring GetAppPaths(const std::wstring& appname);
  static std::wstring GetAppPaths(int key,const std::wstring& appname);

private:
  void BuildAppData();
  std::wstring DeltaToString(int64 delta_ms);
  void LaunchCold(const AppData& appdata,HWND hwnd);
  void LaunchHot(const AppData& appdata,HWND hwnd);
  base::FilePath GetSnapShot();

private:
  Delegate* delegate_;
  std::vector<AppData> appdatas_;
  int64 ticks_start_;

private:
  struct MetricsData{
    base::ProcessMetrics* proc_metrics;
    HANDLE proc_handle;
    uint32 proc_id;
    std::wstring cmd_line;
    uint32 count;
    base::Time startup_time;
    bool is_mainprocess;
  };
  std::map<uint32,MetricsData> proc_metrics_;
  DWORD last_query_tickcount_;
};