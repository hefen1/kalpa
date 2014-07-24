#pragma once

#include <atlbase.h>
#include <atlapp.h>
#include <atlctrls.h>

#include "base/scoped_ptr.h"
#include "base/time.h"
#include "ui/window_impl.h"
#include "tools/profiler/log_daemon.h"
#include "tools/profiler/log_client.h"
#include "tools/profiler/profiler_thread.h"

//////////////////////////////////////////////////////////////////////////

class CMyWindow :  public base::RefCountedThreadSafe<CMyWindow,
  ProfilerThread::DeleteOnUIThread>,public ui::WindowImpl
{
public:
  CMyWindow();
  virtual ~CMyWindow(){}
  void Action(uint64 delta_ms,const std::wstring& action,const std::wstring& from,
    const std::wstring& data);

  enum{
    kCommandInit=1000,
    kCommandFini,
    kCommandTest,
    kCommandList,
  };

  BEGIN_MSG_MAP(CMyWindow)
    MESSAGE_HANDLER(WM_CLOSE, OnClose)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    MESSAGE_HANDLER(WM_NCDESTROY,OnFinalMessage)
    MESSAGE_HANDLER(WM_CREATE,OnCreate)
    MESSAGE_HANDLER(WM_SIZE,OnSize)
    MESSAGE_HANDLER(WM_ERASEBKGND,OnEraseBkgnd)
    MESSAGE_HANDLER(WM_PAINT,OnPaint)
    MESSAGE_HANDLER(WM_SETCURSOR,OnSetCursor)
    COMMAND_HANDLER(kCommandInit,BN_CLICKED,OnCommandInit)
    COMMAND_HANDLER(kCommandFini,BN_CLICKED,OnCommandFini)
    COMMAND_HANDLER(kCommandTest,BN_CLICKED,OnCommandTest)
  END_MSG_MAP()

private:
  LRESULT OnClose(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnEraseBkgnd(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnFinalMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnSetCursor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnCommandFini(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
  LRESULT OnCommandInit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
  LRESULT OnCommandTest(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

private:
  CButton btn_fini_,btn_init_,btn_test_; //¿ªÆô/¹Ø±ÕLogDaemon,Æô¶¯LogClient(360chrome,googlechrome)
  CListViewCtrl list_log_;

  scoped_ptr<LogDaemon> log_server_;
  scoped_ptr<LogClient> log_client_;

  base::TimeTicks ticks_start_;
};
