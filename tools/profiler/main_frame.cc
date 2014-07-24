#include "tools/profiler/main_frame.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/process_util.h"
#include "ui/hwnd_util.h"

#include "tools/profiler/profiler_thread.h"

//////////////////////////////////////////////////////////////////////////

const int kButtonWidth = 100;
const int kButtonHeight = 25;
const int kButtonOffset = 5;
const int kClientWidth = 800;
const int kClientHeight = 600;
const int kListColWidth = 150;

//////////////////////////////////////////////////////////////////////////
CMyWindow::CMyWindow()
:ticks_start_(base::TimeTicks::Now())
{
  log_server_.reset(new LogDaemon);
  log_client_.reset(new LogClient);
}

LRESULT CMyWindow::OnClose(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
  VLOG(1) << "vlog:onclose";
  LOG(INFO) << "log:onclose";
  ::DestroyWindow(hwnd());
  return 0;
}

LRESULT CMyWindow::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
  VLOG(1) << "vlog:ondestroy";
  LOG(INFO) << "log:ondestroy";
  return 0;
}

LRESULT CMyWindow::OnFinalMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
  MessageLoop::current()->PostTask(FROM_HERE,new MessageLoop::QuitTask());
  //delete this;
  Release();
  return 0;
}

LRESULT CMyWindow::OnSetCursor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
  static HCURSOR hcur = LoadCursor( NULL,IDC_ARROW);  
  if(hcur){
    SetCursor(hcur);
  }else{
    bHandled=FALSE;
  }
  return 0;
}

LRESULT CMyWindow::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
  //create control
  btn_init_.Create(hwnd(),NULL,L"开启",WS_VISIBLE|WS_CHILD,0,kCommandInit);
  btn_fini_.Create(hwnd(),NULL,L"关闭",WS_VISIBLE|WS_CHILD|WS_DISABLED,0,kCommandFini);
  btn_test_.Create(hwnd(),NULL,L"测试",WS_VISIBLE|WS_CHILD,0,kCommandTest);
  list_log_.Create(hwnd(),NULL,NULL,WS_VISIBLE|WS_CHILD|LVS_REPORT|LVS_SINGLESEL,0,kCommandList);
  list_log_.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);
  list_log_.AddColumn(L"来源",0);
  list_log_.AddColumn(L"时间",1);
  list_log_.AddColumn(L"事件",2);
  list_log_.AddColumn(L"数据",3);
 
  //base::TimeDelta delta = base::TimeDelta::FromMilliseconds(2999);
  //double delta_ms = delta.InSecondsF();
  //std::string delta_str = base::DoubleToString(delta_ms);
  //for(int i=0;i<100;i++)
  //  list_log_.InsertItem(i,ASCIIToUTF16(delta_str).c_str());

  //center
  ui::CenterAndSizeWindow(NULL,hwnd(),gfx::Size(kClientWidth,kClientHeight),true);

  //col size
  list_log_.SetColumnWidth(0,kListColWidth);
  list_log_.SetColumnWidth(1,kListColWidth);
  list_log_.SetColumnWidth(2,kListColWidth);
  list_log_.SetColumnWidth(3,kClientWidth - 
    3*kListColWidth - 2*kButtonOffset - GetSystemMetrics(SM_CXVSCROLL));

  //set window style
  //SetWindowLong(hwnd(),GWL_STYLE,GetWindowLong(hwnd(),GWL_STYLE)&~WS_MAXIMIZEBOX);
  return 0;
}

LRESULT CMyWindow::OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
  //pos ctrls
  int x=kButtonOffset;int y=kButtonOffset;
  btn_init_.MoveWindow(CRect(x,y,x+kButtonWidth,y+kButtonHeight));
  x=x+kButtonOffset+kButtonWidth;
  btn_fini_.MoveWindow(CRect(x,y,x+kButtonWidth,y+kButtonHeight));
  x=x+kButtonOffset+kButtonWidth;
  btn_test_.MoveWindow(CRect(x,y,x+kButtonWidth,y+kButtonHeight));

  //post lv
  RECT list_bounds = {0};
  GetClientRect(hwnd(),&list_bounds);
  list_bounds.left = kButtonOffset;
  list_bounds.top = y + kButtonHeight + kButtonOffset;
  list_bounds.bottom = list_bounds.bottom - kButtonOffset;
  list_bounds.right = list_bounds.right - kButtonOffset;
  list_log_.MoveWindow(&list_bounds);

  return 0;
}

LRESULT CMyWindow::OnEraseBkgnd(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
  bHandled = FALSE;
  return 0;
}

LRESULT CMyWindow::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
  bHandled = FALSE;
  return 0;
}

//////////////////////////////////////////////////////////////////////////
LRESULT CMyWindow::OnCommandFini(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
  if(log_server_.get())
    log_server_.reset();

  if(log_client_.get()){
    log_client_.reset();
  }

  btn_init_.EnableWindow(TRUE);
  btn_fini_.EnableWindow(FALSE);
  //btn_test_.EnableWindow(FALSE);

  return 0;
}

LRESULT CMyWindow::OnCommandInit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
  if(log_server_.get())
    log_server_->Init();

  if(log_client_.get())
    log_client_->Init();

  btn_init_.EnableWindow(FALSE);
  btn_fini_.EnableWindow(TRUE);
  //btn_test_.EnableWindow(TRUE);

  return 0;
}

static std::wstring Get360ChromePath()
{
  BOOL bRet = FALSE;
  TCHAR szValue[MAX_PATH] = {0};
  TCHAR szPath[MAX_PATH] = {0};
  DWORD dwSize = sizeof(szValue);
  DWORD dwType = REG_SZ;
  SHGetValue(HKEY_LOCAL_MACHINE, 
    _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\360Chrome.exe"), _T(""),
    &dwType, szValue, &dwSize);
  return szValue;
};

LRESULT CMyWindow::OnCommandTest(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
  list_log_.DeleteAllItems();
  ticks_start_ = base::TimeTicks::Now();

  ProfilerThread::PostTask(ProfilerThread::UI,FROM_HERE,NewRunnableMethod(this,&CMyWindow::Action,
    ticks_start_.ToInternalValue(),
    std::wstring(L"CreateProcess"),
    std::wstring(L"MainThread"),
    std::wstring(L"Start->360chrome.exe")));

  base::LaunchProcess(Get360ChromePath().c_str(),base::LaunchOptions(),NULL);

  ProfilerThread::PostTask(ProfilerThread::UI,FROM_HERE,NewRunnableMethod(this,&CMyWindow::Action,
    base::TimeTicks::Now().ToInternalValue(),
    std::wstring(L"CreateProcess"),
    std::wstring(L"MainThread"),
    std::wstring(L"End")));
  return 0;
}

void CMyWindow::Action(uint64 ticks,const std::wstring& action,const std::wstring& from,
            const std::wstring& data)
{
  base::TimeDelta delta = base::TimeDelta::FromMicroseconds(ticks - ticks_start_.ToInternalValue());
  double delta_ms = delta.InSecondsF();
  std::string delta_str = base::DoubleToString(delta_ms);

  int index = list_log_.GetItemCount();
  list_log_.InsertItem(index,from.c_str());
  list_log_.AddItem(index,1,ASCIIToUTF16(delta_str).c_str());
  list_log_.AddItem(index,2,action.c_str());
  list_log_.AddItem(index,3,data.c_str());
}
