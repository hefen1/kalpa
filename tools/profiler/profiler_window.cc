#include "tools/profiler/profiler_window.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "ui/hwnd_util.h"

#include "tools/profiler/profiler_thread.h"
#include "tools/profiler/profiler_process.h"
#include "tools/profiler/profiler_cmds.h"

//////////////////////////////////////////////////////////////////////////

const int kButtonWidth = 80;
const int kButtonHeight = 25;
const int kButtonOffset = 5;
const int kClientWidth = 1160;
const int kClientHeight = 780;
const int kListProcHeight = 200;
const int kListColWidth = 100;

const int kProcCheckId = 3001;
const int kProcCheckWaitTimeout = 1000;

//////////////////////////////////////////////////////////////////////////
ProfilerWindow::ProfilerWindow()
{
  pid_selected_ = 0;
  log_server_.reset(new LogDaemon);
  log_server_->Init();
}

ProfilerWindow::~ProfilerWindow()
{
  for(size_t i=0;i<btn_test_.size();i++){
    delete btn_test_[i];
  }
  btn_test_.clear();

  log_server_.reset();
}

//////////////////////////////////////////////////////////////////////////
LRESULT ProfilerWindow::OnClose(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
  //kill timer
  ::KillTimer(hwnd(),kProcCheckId);

  //shit window
  ::DestroyWindow(hwnd());
  return 0;
}

LRESULT ProfilerWindow::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
  return 0;
}

LRESULT ProfilerWindow::OnFinalMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
  base::MessageLoop::current()->PostTask(FROM_HERE,base::MessageLoop::QuitClosure());
  delete this;
  return 0;
}

LRESULT ProfilerWindow::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
  //create cmd button
  btn_export_.Create(hwnd(),NULL,L"导出全局变量",WS_VISIBLE|WS_CHILD,0,kCommandExport);
  btn_dump_mini_.Create(hwnd(),NULL,L"抓小Dump",WS_VISIBLE|WS_CHILD,0,kCommandDumpMini);
  btn_dump_full_.Create(hwnd(),NULL,L"抓大Dump",WS_VISIBLE|WS_CHILD,0,kCommandDumpFull);
  btn_dump_open_.Create(hwnd(),NULL,L"打开Dump目录",WS_VISIBLE|WS_CHILD,0,kCommandDumpOpen);
  btn_runlow_.Create(hwnd(),NULL,L"低权限运行",WS_VISIBLE|WS_CHILD,0,kCommandRunLow);
  btn_limitedjob_.Create(hwnd(),NULL,L"受限Job运行",WS_VISIBLE|WS_CHILD,0,kCommandLimitedJob);
  btn_debugrun_.Create(hwnd(),NULL,L"调试运行",WS_VISIBLE|WS_CHILD,0,kCommandDebugRun);
  btn_parser_dump_.Create(hwnd(),NULL,L"解析Dump",WS_VISIBLE|WS_CHILD,0,kCommandParserDump);
  btn_crash_.Create(hwnd(),NULL,L"崩溃吧",WS_VISIBLE|WS_CHILD,0,kCommandCrashIt);
  
  //create test button
  BuildAppButton();

  //create loglist
  list_log_.Create(hwnd(),NULL,NULL,WS_VISIBLE|WS_CHILD|LVS_REPORT|LVS_SINGLESEL,0,kCommandListLog);
  list_log_.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);
  list_log_.AddColumn(L"时间(毫秒)",0);
  list_log_.AddColumn(L"耗时(毫秒)",1);
  list_log_.AddColumn(L"进程",2);
  list_log_.AddColumn(L"线程",3);
  list_log_.AddColumn(L"动作",4);
  list_log_.AddColumn(L"数据",5);

  //create proclist
  list_proc_.Create(hwnd(),NULL,NULL,WS_VISIBLE|WS_CHILD|LVS_REPORT|LVS_SINGLESEL,0,kCommandListProc);
  list_proc_.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);
  list_proc_.AddColumn(L"进程",0);
  list_proc_.AddColumn(L"私有内存",1);
  list_proc_.AddColumn(L"公用内存",2);
  list_proc_.AddColumn(L"CPU",3);
  list_proc_.AddColumn(L"启动时间",4);
  list_proc_.AddColumn(L"命令行",5);

  //init
  HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);//(HFONT)::SendMessage(hwnd(), WM_GETFONT, 0, 0);
  btn_export_.SetFont(font);
  btn_dump_mini_.SetFont(font);
  btn_dump_full_.SetFont(font);
  btn_dump_open_.SetFont(font);
  btn_runlow_.SetFont(font);
  btn_limitedjob_.SetFont(font);
  btn_debugrun_.SetFont(font);
  btn_parser_dump_.SetFont(font);
  btn_crash_.SetFont(font);
  list_log_.SetFont(font);
  list_proc_.SetFont(font);
 
  //center
  ui::CenterAndSizeWindow(NULL,hwnd(),gfx::Size(kClientWidth,kClientHeight),true);

  //col size of listlog
  list_log_.SetColumnWidth(0,kListColWidth);
  list_log_.SetColumnWidth(1,kListColWidth);
  list_log_.SetColumnWidth(2,kListColWidth/2);
  list_log_.SetColumnWidth(3,kListColWidth/2);
  list_log_.SetColumnWidth(4,kListColWidth*2);
  list_log_.SetColumnWidth(5,kClientWidth - 5*kListColWidth - 2*kButtonOffset - GetSystemMetrics(SM_CXVSCROLL));

  //col size of listproc
  list_proc_.SetColumnWidth(0,kListColWidth/2);
  list_proc_.SetColumnWidth(1,kListColWidth);
  list_proc_.SetColumnWidth(2,kListColWidth);
  list_proc_.SetColumnWidth(3,kListColWidth/2);
  list_proc_.SetColumnWidth(4,kListColWidth);
  list_proc_.SetColumnWidth(5,kClientWidth - 4*kListColWidth - 2*kButtonOffset - GetSystemMetrics(SM_CXVSCROLL));

  //set window style
  //SetWindowLong(hwnd(),GWL_STYLE,GetWindowLong(hwnd(),GWL_STYLE)&~WS_MAXIMIZEBOX);

  //init timer
  ::SetTimer(hwnd(),kProcCheckId,kProcCheckWaitTimeout,NULL);
  return 0;
}

LRESULT ProfilerWindow::OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
  //pos ctrls
  int x=kButtonOffset;int y=kButtonOffset;
  btn_export_.MoveWindow(CRect(x,y,x+kButtonWidth,y+kButtonHeight));
  x=x+kButtonOffset+kButtonWidth;
  btn_dump_mini_.MoveWindow(CRect(x,y,x+kButtonWidth,y+kButtonHeight));
  x=x+kButtonOffset+kButtonWidth;
  btn_dump_full_.MoveWindow(CRect(x,y,x+kButtonWidth,y+kButtonHeight));
  x=x+kButtonOffset+kButtonWidth;
  btn_dump_open_.MoveWindow(CRect(x,y,x+kButtonWidth,y+kButtonHeight));
  x=x+kButtonOffset+kButtonWidth;
  btn_runlow_.MoveWindow(CRect(x,y,x+kButtonWidth,y+kButtonHeight));
  x=x+kButtonOffset+kButtonWidth;
  btn_limitedjob_.MoveWindow(CRect(x,y,x+kButtonWidth,y+kButtonHeight));
    x=x+kButtonOffset+kButtonWidth;
  btn_debugrun_.MoveWindow(CRect(x,y,x+kButtonWidth,y+kButtonHeight));
  x=x+kButtonOffset+kButtonWidth;
  btn_parser_dump_.MoveWindow(CRect(x,y,x+kButtonWidth,y+kButtonHeight));
  x=x+kButtonOffset+kButtonWidth;
  btn_crash_.MoveWindow(CRect(x,y,x+kButtonWidth,y+kButtonHeight));
  for(size_t i=0;i<btn_test_.size();i++){
    x=x+kButtonOffset+kButtonWidth*(!i?4:3)/4;
    btn_test_[i]->MoveWindow(CRect(x,y,x+kButtonWidth*3/4,y+kButtonHeight));
  }

  //posistion lv-log
  RECT list_bounds = {0};
  GetClientRect(hwnd(),&list_bounds);
  list_bounds.left = kButtonOffset;
  list_bounds.top = y + kButtonHeight + kButtonOffset;
  list_bounds.bottom = list_bounds.bottom - 2*kButtonOffset - kListProcHeight;
  list_bounds.right = list_bounds.right - kButtonOffset;
  list_log_.MoveWindow(&list_bounds);

  //posistion lv-proc
  list_bounds.top = list_bounds.bottom + kButtonOffset;
  list_bounds.bottom = list_bounds.bottom + kButtonOffset + kListProcHeight;
  list_proc_.MoveWindow(&list_bounds);

  return 0;
}

LRESULT ProfilerWindow::OnEraseBkgnd(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
  bHandled = FALSE;
  return 0;
}

LRESULT ProfilerWindow::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
  bHandled = FALSE;
  return 0;
}

LRESULT ProfilerWindow::OnCopyData(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
  HWND hwnd = reinterpret_cast<HWND>(wParam);
  COPYDATASTRUCT* cds = reinterpret_cast<COPYDATASTRUCT*>(lParam);

  struct cp_tag{
    int64 ticks_start;
    int64 ticks_end;
    int launch_ok;
  };

  if(cds->cbData!=sizeof(cp_tag) || !cds->lpData){
    return 0;
  }

  cp_tag* cpdata = (cp_tag*)cds->lpData;
  ProfilerData* profiler_data = g_profiler_process->profiler_data();
  profiler_data->LaunchOk(cpdata->ticks_start,cpdata->ticks_end,!!cpdata->launch_ok);
  return 0;
}

LRESULT ProfilerWindow::OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled){
  int timer_id = (int)wParam;
  if(timer_id == kProcCheckId){
    GetProfilerData()->QueryProcData();
    return 0;
  }

  return 0;
}

//////////////////////////////////////////////////////////////////////////
LRESULT ProfilerWindow::OnCommandExport(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
  ProfilerCmd::DumpGlobalVar(hwnd());
  return 0;
}

LRESULT ProfilerWindow::OnCommandDumpMini(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
  ProfilerCmd::WriteMinidump(false);
  return 0;
}

LRESULT ProfilerWindow::OnCommandDumpFull(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
  ProfilerCmd::WriteMinidump(true);
  return 0;
}

LRESULT ProfilerWindow::OnCommandDumpOpen(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
  ProfilerCmd::OpenMinidumpFolder();
  return 0;
}

LRESULT ProfilerWindow::OnCommandRunLow(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
  ProfilerCmd::RunLow(hwnd());
  return 0;
}

LRESULT ProfilerWindow::OnCommandLimitedJob(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
  ProfilerCmd::LimitedJob(hwnd());
  return 0;
}

LRESULT ProfilerWindow::OnCommandDebugRun(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
  list_log_.DeleteAllItems();
  ProfilerCmd::DebugRun();
  return 0;
}

LRESULT ProfilerWindow::OnCommandParserDump(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
  ProfilerCmd::ParserMinidump(hwnd());
  return 0;
}

LRESULT ProfilerWindow::OnCommandCrashIt(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
  ProfilerCmd::CrashIt(pid_selected_);
  return 0;
}

LRESULT ProfilerWindow::OnCommandTest(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
  list_log_.DeleteAllItems();

  GetProfilerData()->ProfileApp(size_t(wID - kCommandTestBegin),hwnd());

  return 0;
}

//////////////////////////////////////////////////////////////////////////

void ProfilerWindow::OnAction(const std::wstring& time_ms,const std::wstring& delta_ms,
                              const std::wstring& pid,const std::wstring& tid,
                             const std::wstring& action,const std::wstring& data)
{
  //--enable-logging --v=1
  LOG(INFO) << time_ms << L"," << delta_ms << L"," << pid << L","<< tid << L","<< action << L"," << data<<"\r\n";

  int index = list_log_.GetItemCount();
  list_log_.InsertItem(index,time_ms.c_str());
  list_log_.AddItem(index,1,delta_ms.c_str());
  list_log_.AddItem(index,2,pid.c_str());
  list_log_.AddItem(index,3,tid.c_str());
  list_log_.AddItem(index,4,action.c_str());
  list_log_.AddItem(index,5,data.c_str()); //只能显示前260个字符,windows控件真垃圾!
}

void ProfilerWindow::OnProcData(const std::vector<ProfilerData::ProcData>& proc_data){
  int selectd_index = list_proc_.GetSelectedIndex();
  list_proc_.DeleteAllItems();
  for(size_t index=0;index<proc_data.size();index++){
    list_proc_.InsertItem(index,proc_data[index].pid.c_str());
    list_proc_.AddItem(index,1,proc_data[index].private_memory.c_str());
    list_proc_.AddItem(index,2,proc_data[index].shared_memory.c_str());
    list_proc_.AddItem(index,3,proc_data[index].cpu.c_str());
    list_proc_.AddItem(index,4,proc_data[index].startup_ms.c_str());
    list_proc_.AddItem(index,5,proc_data[index].cmd_line.c_str()); //只能显示前260个字符,windows控件真垃圾!
  }
  pid_selected_ = 0;
  if(selectd_index<list_proc_.GetItemCount()){
    list_proc_.SelectItem(selectd_index);
    if(selectd_index>=0 && selectd_index<(int)proc_data.size())
      base::StringToUint(proc_data[selectd_index].pid.c_str(),(unsigned int*)&pid_selected_);
  }
}

void ProfilerWindow::BuildAppButton()
{
  btn_test_.clear();
  GetProfilerData()->Init(this);
  int app_count = GetProfilerData()->GetAppCount();
  for(int i=0;i<app_count;i++){
    ProfilerData::AppData appdata;
    GetProfilerData()->GetAppData(i,appdata);
    CButton* btn = new CButton;
    btn_test_.push_back(btn);
    btn->Create(hwnd(),NULL,appdata.app_name.c_str(),WS_VISIBLE|WS_CHILD/*|WS_DISABLED*/,0,kCommandTestBegin+i);
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);//(HFONT)::SendMessage(hwnd(), WM_GETFONT, 0, 0);
    btn->SetFont(font);
  }
}

ProfilerData* ProfilerWindow::GetProfilerData()
{
  ProfilerData* profiler_data = g_profiler_process->profiler_data();
  DCHECK(profiler_data);
  return profiler_data;
}

//////////////////////////////////////////////////////////////////////////