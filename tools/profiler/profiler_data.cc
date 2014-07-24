#include "tools/profiler/profiler_data.h"

#include <Shlwapi.h>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/process/kill.h"
#include "base/process/process_iterator.h"
#include "base/process/process_metrics.h"
#include "base/process/launch.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/StringPrintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"

//////////////////////////////////////////////////////////////////////////
ProfilerData::ProfilerData()
{
  //先搞个初始值
  ticks_start_ = base::TimeTicks::HighResNow().ToInternalValue();
  last_query_tickcount_ = GetTickCount();
}

ProfilerData::~ProfilerData()
{
  //clear proc_metrics
  std::map<uint32,MetricsData>::iterator it = proc_metrics_.begin();
  for(;it!=proc_metrics_.end();it++){
    MetricsData& metrics = it->second;
    if(metrics.proc_handle){
      delete metrics.proc_metrics;
      ::CloseHandle(metrics.proc_handle);
      metrics.proc_handle = 0;
    }
  }
  proc_metrics_.clear();
}

//////////////////////////////////////////////////////////////////////////

void ProfilerData::ProfileApp(size_t id,HWND hwnd)
{
  if(!ProfilerThread::CurrentlyOn(ProfilerThread::IO)){
    ProfilerThread::PostTask(ProfilerThread::IO,FROM_HERE,
      base::Bind(&ProfilerData::ProfileApp,this,id,hwnd));
    return;
  }

  AppData appdata;
  if(!GetAppData(id,appdata))
    return;

  //先全部杀了，好恢复logdaemon和虚拟磁盘，比如notepad占用了channel，然后开启app
  for(size_t i=0;i<appdatas_.size();++i){
    base::KillProcesses(appdatas_[i].app_filename,0,NULL);
    base::WaitForProcessesToExit(appdatas_[i].app_filename,base::TimeDelta::FromMilliseconds(100),NULL);
  }

  //如果有run_in_snapshot.exe在运行,就退出=
  if(base::GetProcessCount(GetSnapShot().BaseName().value(),NULL)){
    base::WaitForProcessesToExit(GetSnapShot().BaseName().value(),base::TimeDelta::FromMilliseconds(500),NULL);
    if(base::GetProcessCount(GetSnapShot().BaseName().value(),NULL)){
      return;
    }
  }

  //===>launch now...
  base::DeleteFile(base::FilePath(L"c:\\userdata"),true);
  ticks_start_ = base::TimeTicks::HighResNow().ToInternalValue();;
  if(appdata.cold_start){
    LaunchCold(appdata,hwnd);
  }else{
    LaunchHot(appdata,hwnd);
  }

  return;
}

void ProfilerData::LaunchHot(const AppData& appdata,HWND hwnd){
  //copy
  if(appdata.copy_file)
    base::CopyFile(base::FilePath(appdata.copyfrom_path),base::FilePath(appdata.copyto_path));

  std::wstring rwd_cmdline;
  base::FilePath file_path;
  PathService::Get(base::DIR_EXE,&file_path);
  rwd_cmdline = file_path.Append(L"run_with_dll.exe").value();
  rwd_cmdline.append(L" --exe=\"");
  rwd_cmdline.append(appdata.exe_path);
  rwd_cmdline.append(L"\" --dll=\"");
  rwd_cmdline.append(appdata.profiler_path);
  rwd_cmdline.append(L"\" --detoured=\"");
  rwd_cmdline.append(appdata.detoured_path);
  rwd_cmdline.append(L"\" --hwnd=");
  rwd_cmdline.append(base::UintToString16((DWORD)hwnd));

  //options for exe
  rwd_cmdline.append(L" -- ");
  rwd_cmdline.append(appdata.exe_params);

  LOG(INFO) << rwd_cmdline<<"\r\n";

  base::LaunchOptions rwd_options;
  rwd_options.start_hidden = true;
  bool launch_ok = base::LaunchProcess(rwd_cmdline,rwd_options,NULL);

  //异步的,先通知给界面=
  Action(
    ticks_start_,
    0,
    GetCurrentProcessId(),
    GetCurrentThreadId(),
    std::wstring(L"=>run_with_dll.exe"),
    launch_ok?L"ok":L"fail");
}

//run_in_snapshot:P
//run_in_snapshot --volume=C:\\ --snapshot=P: [-- run_with_dll.exe --exe --dll --detoured 
//     [-- --memory-profiler --user-data]]
void ProfilerData::LaunchCold(const AppData& appdata,HWND hwnd){
  //copy
  if(appdata.copy_file)
    base::CopyFile(base::FilePath(appdata.copyfrom_path),base::FilePath(appdata.copyto_path));

  // snapshot.exe
  base::FilePath file_path = GetSnapShot();
  std::wstring ris_cmdline = file_path.value();
  ris_cmdline.append(L" --volume=C:\\");
  ris_cmdline.append(L" --snapshot=P:");

  // options for exe
  ris_cmdline.append(L" -- ");
  
  PathService::Get(base::DIR_EXE,&file_path);
  std::wstring rwd_cmdline = file_path.Append(L"run_with_dll.exe").value();
  rwd_cmdline.append(L" --exe=\"");
  std::wstring fake_exe_path = appdata.exe_path;
  fake_exe_path[0]=L'P';//<===run in drive p,:-)
  rwd_cmdline.append(fake_exe_path);
  rwd_cmdline.append(L"\" --dll=\"");
  rwd_cmdline.append(appdata.profiler_path);
  rwd_cmdline.append(L"\" --detoured=\"");
  rwd_cmdline.append(appdata.detoured_path);
  rwd_cmdline.append(L"\" --hwnd=");
  rwd_cmdline.append(base::UintToString16((DWORD)hwnd));
  rwd_cmdline.append(L" -- ");
  rwd_cmdline.append(appdata.exe_params);

  ris_cmdline.append(rwd_cmdline);

  LOG(INFO) << rwd_cmdline<<"\r\n";

  // launch
  base::LaunchOptions ris_options;
  ris_options.start_hidden = true;
  bool launch_ok = base::LaunchProcess(ris_cmdline,ris_options,NULL);

  //异步的,先通知给界面=
  Action(
    ticks_start_,
    0,
    GetCurrentProcessId(),
    GetCurrentThreadId(),
    GetSnapShot().BaseName().value(),
    launch_ok?L"P:":L"fail");
}

void ProfilerData::LaunchOk(int64 ticks_start,int64 ticks_end,bool launch_ok){
  //开始计时=
  ticks_start_ = ticks_start;

  //显示结果=
  Action(
    ticks_start_,
    ticks_end - ticks_start_,
    GetCurrentProcessId(),
    GetCurrentThreadId(),
    std::wstring(L"<=run_with_dll.exe"),
    launch_ok?L"ok":L"fail");
}

base::FilePath ProfilerData::GetSnapShot()
{
  base::FilePath file_path;
  PathService::Get(base::DIR_EXE,&file_path);
  if(base::win::GetVersion()>=base::win::VERSION_VISTA){
    base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
    if(os_info->architecture()==base::win::OSInfo::X64_ARCHITECTURE){
      file_path = file_path.Append(L"run_in_snapshot_x64.exe");
    }
    else{
      file_path = file_path.Append(L"run_in_snapshot.exe");
    }
  }else{
    file_path = file_path.Append(L"run_in_snapshot_xp.exe");
  }

  return file_path;
}

std::wstring ProfilerData::DeltaToString(int64 delta_ms){
  base::TimeDelta delta_mm = base::TimeDelta::FromMicroseconds(delta_ms);
  return ASCIIToUTF16(base::DoubleToString(delta_mm.InMillisecondsF()));
}

void ProfilerData::Action(int64 time_ms,int64 delta_ms,
                          int pid,int tid,
                          const std::wstring& action,const std::wstring& data)
{
  if(!ProfilerThread::CurrentlyOn(ProfilerThread::UI)){
    ProfilerThread::PostTask(ProfilerThread::UI,FROM_HERE,
      base::Bind(&ProfilerData::Action,this,
      time_ms,
      delta_ms,
      pid,
      tid,
      action,
      data));
    return;
  }

  if(delegate_){
    delegate_->OnAction(
      DeltaToString(time_ms - ticks_start_),
      DeltaToString(delta_ms),
      base::IntToString16(pid),
      base::IntToString16(tid),
      action,
      data);
  }
  return;
}

void ProfilerData::RawAction(int64 delta_ms,const std::wstring& action,const std::wstring& data)
{
  if(!ProfilerThread::CurrentlyOn(ProfilerThread::UI)){
    ProfilerThread::PostTask(ProfilerThread::UI,FROM_HERE,
      base::Bind(&ProfilerData::RawAction,this,
      delta_ms,
      action,
      data));
    return;
  }

  if(delegate_){
    delegate_->OnAction(
      L"",
      DeltaToString(delta_ms),
      L"",
      L"",
      action,
      data);
  }
  return;
}

void ProfilerData::DebugAction(const std::wstring& action,const std::wstring& data)
{
  if(!ProfilerThread::CurrentlyOn(ProfilerThread::UI)){
    ProfilerThread::PostTask(ProfilerThread::UI,FROM_HERE,
      base::Bind(&ProfilerData::DebugAction,this,
      action,
      data));
    return;
  }

  if(delegate_){
    delegate_->OnAction(
      L"",
      L"",
      L"",
      L"",
      action,
      data);
  }
  return;
}

void ProfilerData::HandleProcData(const std::vector<ProcData>& proc_data){
  if(!ProfilerThread::CurrentlyOn(ProfilerThread::UI)){
    ProfilerThread::PostTask(ProfilerThread::UI,FROM_HERE,
      base::Bind(&ProfilerData::HandleProcData,this,
      proc_data));
    return;
  }

  if(delegate_){
    delegate_->OnProcData(proc_data);
  }
  return;
}


void ProfilerData::Init(Delegate* delegate)
{
  delegate_ = delegate;

  BuildAppData();
}

int ProfilerData::GetAppCount()
{
  return appdatas_.size();
}

bool ProfilerData::GetAppData(size_t index,AppData& appdata)
{
  if(index >= appdatas_.size())
    return false;

  appdata = appdatas_[index];
  return true;
}

void ProfilerData::GetAppDatas(std::vector<AppData>& appdatas)
{
  appdatas = appdatas_;
}

//////////////////////////////////////////////////////////////////////////

//static
std::wstring ProfilerData::GetAppPaths(int key,const std::wstring& appname)
{
  base::FilePath file_path;
  PathService::Get(key,&file_path);
  return file_path.Append(appname).value();
};

//static
std::wstring ProfilerData::GetAppPaths(const std::wstring& appname)
{
  BOOL bRet = FALSE;
  TCHAR szValue[MAX_PATH] = {0};
  TCHAR szPath[MAX_PATH] = {0};
  DWORD dwSize = sizeof(szValue);
  DWORD dwType = REG_SZ;
  std::wstring key = 
    base::StringPrintf(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\%ls",appname.c_str());
  //w_char*用 %ls,char*用%s
  SHGetValue(HKEY_LOCAL_MACHINE, 
    key.c_str(), L"",
    &dwType, szValue, &dwSize);

  if(!base::PathExists(base::FilePath(szValue))){
    if(appname==L"360se6.exe")
      return GetAppPaths(base::DIR_APP_DATA,L"360se6\\application\\360se.exe");
    if(appname==L"360chrome.exe")
      return GetAppPaths(base::DIR_LOCAL_APP_DATA,L"360chrome\\chrome\\application\\360chrome.exe");
    if(appname==L"chrome.exe")
      return GetAppPaths(base::DIR_LOCAL_APP_DATA,L"google\\chrome\\application\\chrome.exe");
  }
  return szValue;
};

void ProfilerData::BuildAppData()
{
  appdatas_.clear();

  AppData appdata;
  base::FilePath file_path;

  //360se6
  appdata.app_name = L"360se-热";
  appdata.app_filename = L"360se.exe";
  file_path = base::FilePath(GetAppPaths(L"360se6.exe"));
  appdata.exe_path = file_path.value();
  appdata.exe_params = L"http://hao.360.cn --memory-profile --user-data-dir=c:\\userdata\\360se6";
  appdata.copyto_path = file_path.DirName().Append(L"memory_watcher.dll").value();
  PathService::Get(base::DIR_EXE,&file_path);
  appdata.copyfrom_path = file_path.Append(L"memory_watcher.dll").value();
  appdata.profiler_path = file_path.Append(L"profiler.dll").value();
  appdata.detoured_path = file_path.Append(L"detoured.dll").value();
  appdata.cold_start = false;
  appdata.copy_file = true;
  appdatas_.push_back(appdata);

  //360chrome
  appdata.app_name = L"360极速-热";
  appdata.app_filename = L"360chrome.exe";
  file_path = base::FilePath(GetAppPaths(L"360chrome.exe"));
  appdata.exe_path = file_path.value();
  appdata.exe_params = L"http://hao.360.cn --memory-profile --user-data-dir=c:\\userdata\\360chrome";
  appdata.copyto_path = file_path.DirName().Append(L"memory_watcher.dll").value();
  PathService::Get(base::DIR_EXE,&file_path);
  appdata.copyfrom_path = file_path.Append(L"memory_watcher.dll").value();
  appdata.profiler_path = file_path.Append(L"profiler.dll").value();
  appdata.detoured_path = file_path.Append(L"detoured.dll").value();
  appdata.cold_start = false;
  appdata.copy_file = true;
  appdatas_.push_back(appdata);

  //chrome
  appdata.app_name = L"Google-热";
  appdata.app_filename = L"chrome.exe";
  file_path = base::FilePath(GetAppPaths(L"chrome.exe"));
  appdata.exe_path = file_path.value();
  appdata.exe_params = L"http://hao.360.cn --memory-profile --user-data-dir=c:\\userdata\\google";
  appdata.copyto_path = file_path.DirName().Append(L"memory_watcher.dll").value();
  PathService::Get(base::DIR_EXE,&file_path);
  appdata.copyfrom_path = file_path.Append(L"memory_watcher.dll").value();
  appdata.profiler_path = file_path.Append(L"profiler.dll").value();
  appdata.detoured_path = file_path.Append(L"detoured.dll").value();
  appdata.cold_start = false;
  appdata.copy_file = true;
  appdatas_.push_back(appdata);

  //360se6
  appdata.app_name = L"360se-冷";
  appdata.app_filename = L"360se.exe";
  file_path = base::FilePath(GetAppPaths(L"360se6.exe"));
  appdata.exe_path = file_path.value();
  appdata.exe_params = L"http://hao.360.cn --memory-profile --user-data-dir=c:\\userdata\\360se6";
  appdata.copyto_path = file_path.DirName().Append(L"memory_watcher.dll").value();
  PathService::Get(base::DIR_EXE,&file_path);
  appdata.copyfrom_path = file_path.Append(L"memory_watcher.dll").value();
  appdata.profiler_path = file_path.Append(L"profiler.dll").value();
  appdata.detoured_path = file_path.Append(L"detoured.dll").value();
  appdata.cold_start = true;
  appdata.copy_file = true;
  appdatas_.push_back(appdata);

  //360chrome
  appdata.app_name = L"360极速-冷";
  appdata.app_filename = L"360chrome.exe";
  file_path = base::FilePath(GetAppPaths(L"360chrome.exe"));
  appdata.exe_path = file_path.value();
  appdata.exe_params = L"http://hao.360.cn --memory-profile --user-data-dir=c:\\userdata\\360chrome";
  appdata.copyto_path = file_path.DirName().Append(L"memory_watcher.dll").value();
  PathService::Get(base::DIR_EXE,&file_path);
  appdata.copyfrom_path = file_path.Append(L"memory_watcher.dll").value();
  appdata.profiler_path = file_path.Append(L"profiler.dll").value();
  appdata.detoured_path = file_path.Append(L"detoured.dll").value();
  appdata.cold_start = true;
  appdata.copy_file = true;
  appdatas_.push_back(appdata);


  //chrome
  appdata.app_name = L"Google-冷";
  appdata.app_filename = L"chrome.exe";
  file_path = base::FilePath(GetAppPaths(L"chrome.exe"));
  appdata.exe_path = file_path.value();
  appdata.exe_params = L"http://hao.360.cn  --memory-profile --user-data-dir=c:\\userdata\\google";
  appdata.copyto_path = file_path.DirName().Append(L"memory_watcher.dll").value();
  PathService::Get(base::DIR_EXE,&file_path);
  appdata.copyfrom_path = file_path.Append(L"memory_watcher.dll").value();
  appdata.profiler_path = file_path.Append(L"profiler.dll").value();
  appdata.detoured_path = file_path.Append(L"detoured.dll").value();
  appdata.cold_start = true;
  appdata.copy_file = true;
  appdatas_.push_back(appdata);
}

//static
BOOL ProfilerData::GetDebugPrivilege(HANDLE hProcess )
{
  BOOL              bResult = FALSE;
  HANDLE            hToken;
  TOKEN_PRIVILEGES  TokenPrivileges;

  if(OpenProcessToken(hProcess, TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES,&hToken))
  {
    TokenPrivileges.PrivilegeCount           = 1;
    LookupPrivilegeValue(NULL,SE_DEBUG_NAME,&TokenPrivileges.Privileges[0].Luid);

    TokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    bResult = AdjustTokenPrivileges(hToken,FALSE,&TokenPrivileges,sizeof(TOKEN_PRIVILEGES),NULL,NULL);
    CloseHandle(hToken);
  }

  return bResult;
}

//////////////////////////////////////////////////////////////////////////namespace{

std::wstring FormatToKByte(size_t bytes){
  size_t total_k = bytes/1000;
  size_t total_m = total_k/1000;
  size_t reset_k = total_k%1000;
  if(total_m)
    return base::StringPrintf(L"%d,%03dK",total_m,reset_k);
  else
    return base::StringPrintf(L"%dK",reset_k);
}

typedef struct
{
  USHORT Length;
  USHORT MaximumLength;
  PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef struct
{
  ULONG          AllocationSize;
  ULONG          ActualSize;
  ULONG          Flags;
  ULONG          Unknown1;
  UNICODE_STRING Unknown2;
  HANDLE         InputHandle;
  HANDLE         OutputHandle;
  HANDLE         ErrorHandle;
  UNICODE_STRING CurrentDirectory;
  HANDLE         CurrentDirectoryHandle;
  UNICODE_STRING SearchPaths;
  UNICODE_STRING ApplicationName;
  UNICODE_STRING CommandLine;
  PVOID          EnvironmentBlock;
  ULONG          Unknown[9];
  UNICODE_STRING Unknown3;
  UNICODE_STRING Unknown4;
  UNICODE_STRING Unknown5;
  UNICODE_STRING Unknown6;
} PROCESS_PARAMETERS, *PPROCESS_PARAMETERS;

typedef struct
{
  ULONG               AllocationSize;
  ULONG               Unknown1;
  HINSTANCE           ProcessHinstance;
  PVOID               ListDlls;
  PPROCESS_PARAMETERS ProcessParameters;
  ULONG               Unknown2;
  HANDLE              Heap;
} PEB, *PPEB;

typedef struct
{
  DWORD ExitStatus;
  PPEB  PebBaseAddress;
  DWORD AffinityMask;
  DWORD BasePriority;
  ULONG UniqueProcessId;
  ULONG InheritedFromUniqueProcessId;
}   PROCESS_BASIC_INFORMATION;

typedef LONG (WINAPI *NtQueryInformationProcess_T)(HANDLE,UINT,PVOID,ULONG,PULONG);

static BOOL GetProcessCmdLine(DWORD dwId,std::wstring& cmd_line)
{
  LONG                      status;
  HANDLE                    hProcess;
  PROCESS_BASIC_INFORMATION pbi;
  PEB                       Peb;
  PROCESS_PARAMETERS        ProcParam;
  DWORD                     dwDummy;
  DWORD                     dwSize;
  LPVOID                    lpAddress;
  BOOL                      bRet = FALSE;
  std::vector<BYTE>         buf;

  // Get process handle
  hProcess = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,dwId);
  if (!hProcess)
    return FALSE;

  // Retrieve information
  NtQueryInformationProcess_T NtQueryInformationProcess = (NtQueryInformationProcess_T)GetProcAddress(
    GetModuleHandle(L"ntdll.dll"),"NtQueryInformationProcess");
  if(!NtQueryInformationProcess)
    goto cleanup;

  status = NtQueryInformationProcess( hProcess,
    0,//ProcessBasicInformation = 0,
    (PVOID)&pbi,
    sizeof(PROCESS_BASIC_INFORMATION),
    NULL
    );

  if (status)
    goto cleanup;

  if (!ReadProcessMemory( hProcess,
      pbi.PebBaseAddress,
      &Peb,
      sizeof(PEB),
      &dwDummy
      )
    )
    goto cleanup;

  if (!ReadProcessMemory( hProcess,
      Peb.ProcessParameters,
      &ProcParam,
      sizeof(PROCESS_PARAMETERS),
      &dwDummy
      )
    )
    goto cleanup;

  lpAddress = ProcParam.CommandLine.Buffer;
  dwSize = ProcParam.CommandLine.Length;
  buf.resize(dwSize+2);

  if (!ReadProcessMemory( hProcess,
      lpAddress,
      &buf[0],
      dwSize,
      &dwDummy
      )
    )
   goto cleanup;

  buf[dwSize]=0;
  buf[dwSize+1]=0;
  cmd_line.assign((wchar_t*)&buf[0]);
  bRet = TRUE;

cleanup:
  CloseHandle(hProcess);
  return bRet;
} 

static void GetProcessStartupTime(HANDLE process_handle,base::Time& startup_time){
  FILETIME startup_filetime,temp1_ft,temp2_ft,temp3_ft;
  if(GetProcessTimes(process_handle,&startup_filetime,&temp1_ft,&temp2_ft,&temp3_ft)){
    startup_time = base::Time::FromFileTime(startup_filetime);
  }
}

static bool IsMainProcess(const std::wstring& command_Line){
  CommandLine cmd = CommandLine::FromString(command_Line);
  if(cmd.GetSwitchValueASCII("type").empty())
    return true;
  return false;
}

//////////////////////////////////////////////////////////////////////////

void ProfilerData::EnumProcByName(const std::wstring& proc_name){
  base::NamedProcessIterator iter(proc_name, NULL);
  while (const base::ProcessEntry* entry = iter.NextProcessEntry()) {
    if(proc_metrics_.find(entry->pid())==proc_metrics_.end()){
      HANDLE processhandle = ::OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE, FALSE, entry->pid());
      if(processhandle){
        MetricsData metrics;
        metrics.proc_metrics = base::ProcessMetrics::CreateProcessMetrics(processhandle);
        metrics.proc_id = entry->pid();
        metrics.proc_handle = processhandle;
        GetProcessCmdLine(entry->pid(),metrics.cmd_line);
        metrics.is_mainprocess = IsMainProcess(metrics.cmd_line);
        GetProcessStartupTime(metrics.proc_handle,metrics.startup_time);
        metrics.count = 0;
        proc_metrics_[entry->pid()]= metrics;
      }
    }
  }
}

void ProfilerData::QueryProcData(){
  if(!ProfilerThread::CurrentlyOn(ProfilerThread::IO)){
    ProfilerThread::PostTask(ProfilerThread::IO,FROM_HERE,
      base::Bind(&ProfilerData::QueryProcData,this));
    return;
  }

  //防止io挂起后的一堆调试问题
  if(last_query_tickcount_ + 100 > GetTickCount() ){
    return;
  }
  last_query_tickcount_ = GetTickCount();
  std::vector<ProcData> data;

  //enum进程如果不在列表里面的，初始化并加入=
  EnumProcByName(L"360se.exe");
  EnumProcByName(L"360chrome.exe");
  EnumProcByName(L"chrome.exe");

  //收集数据，如果是第一次收集，就不收集；如果收集时候，进程不存在了，就清除=
  std::map<uint32,MetricsData>::iterator it = proc_metrics_.begin();
  while(it!=proc_metrics_.end()){
    MetricsData& metrics = it->second;
    //进程退了=或者这个句柄无效了,如在调试时候结束=
    int temp_code = 0;
    DWORD retWait = ::WaitForSingleObject(metrics.proc_handle,0);
    if(retWait == WAIT_FAILED || retWait == WAIT_OBJECT_0){
      delete metrics.proc_metrics;
      if(retWait == WAIT_FAILED ){
        DWORD last_err = GetLastError();
        if(last_err ==  ERROR_INVALID_HANDLE){
          ::CloseHandle(metrics.proc_handle);
        }
      }
      metrics.proc_metrics = 0;
      metrics.proc_handle = 0;
      metrics.count = 0;
      std::map<uint32,MetricsData>::iterator it_remove = it;
      it++;
      proc_metrics_.erase(it_remove);
      continue;
    }

    //进程在，但count=0
    metrics.count++;
    if(1==metrics.count){
      metrics.proc_metrics->GetCPUUsage();
      it++;
      continue;
    }
    //收集数据=
    ProcData item;
    item.pid = base::StringPrintf(L"%d",metrics.proc_id);
    size_t p_mm,s_mm;
    metrics.proc_metrics->GetMemoryBytes(&p_mm,&s_mm);
    item.private_memory = base::StringPrintf(L"%ls",FormatToKByte(p_mm).c_str());
    item.shared_memory = base::StringPrintf(L"%ls",FormatToKByte(s_mm).c_str());
    item.cpu = base::StringPrintf(L"%1.0lf%%",metrics.proc_metrics->GetCPUUsage());
    item.cmd_line = metrics.cmd_line;
    item.is_mainprocess = metrics.is_mainprocess;
    item.startup_time = metrics.startup_time;

    data.push_back(item);
    it++;
  }

  //计算启动时间=
  std::vector<ProcData>::const_iterator main_it = data.begin();
  for(;main_it!=data.end();main_it++){
    if(main_it->is_mainprocess){
      break;
    }
  }
  if(main_it!=data.end()){
    std::vector<ProcData>::iterator it = data.begin();
    for(;it!=data.end();it++){
      base::TimeDelta startup_delta = it->startup_time - main_it->startup_time;
      it->startup_ms = UTF8ToUTF16(base::Int64ToString(startup_delta.InMilliseconds()));
    }
  }
  HandleProcData(data);
}

//////////////////////////////////////////////////////////////////////////