#pragma once

#include <atlbase.h>
#include <atlapp.h>
#include <atlctrls.h>

#include "base/memory/scoped_ptr.h"
#include "ui/window_impl.h"
#include "tools/profiler/log_daemon.h"
#include "tools/profiler/profiler_thread.h"
#include "tools/profiler/profiler_data.h"

//////////////////////////////////////////////////////////////////////////

class ProfilerWindow : public ui::WindowImpl,public ProfilerData::Delegate
{
public:
  ProfilerWindow();
  virtual ~ProfilerWindow();
  
  //ProfilerData::Delegate
  virtual void OnAction(const std::wstring& time_ms,const std::wstring& delta_ms,
    const std::wstring& pid,const std::wstring& tid,
    const std::wstring& action,const std::wstring& data);

  virtual void OnProcData(const std::vector<ProfilerData::ProcData>& proc_data);
  //ui::WindowImpl
  //virtual HICON GetDefaultWindowIcon()const;

  enum{
    kCommandExport=1000,
    kCommandDumpMini,
    kCommandDumpFull,
    kCommandDumpOpen,
    kCommandRunLow,
    kCommandLimitedJob,
    kCommandDebugRun,
    kCommandParserDump,
    kCommandCrashIt,
    kCommandListLog,
    kCommandListProc,
    kCommandTestBegin=2000,
    kCommandTestEnd=2100,
  };

  BEGIN_MSG_MAP(ProfilerWindow)
    MESSAGE_HANDLER(WM_CLOSE, OnClose)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    MESSAGE_HANDLER(WM_NCDESTROY,OnFinalMessage)
    MESSAGE_HANDLER(WM_CREATE,OnCreate)
    MESSAGE_HANDLER(WM_SIZE,OnSize)
    MESSAGE_HANDLER(WM_ERASEBKGND,OnEraseBkgnd)
    MESSAGE_HANDLER(WM_PAINT,OnPaint)
    MESSAGE_HANDLER(WM_COPYDATA,OnCopyData)
    MESSAGE_HANDLER(WM_TIMER, OnTimer)
    //MESSAGE_HANDLER(WM_SETCURSOR,OnSetCursor)
    COMMAND_HANDLER(kCommandExport,BN_CLICKED,OnCommandExport)
    COMMAND_HANDLER(kCommandDumpMini,BN_CLICKED,OnCommandDumpMini)
    COMMAND_HANDLER(kCommandDumpFull,BN_CLICKED,OnCommandDumpFull)
    COMMAND_HANDLER(kCommandDumpOpen,BN_CLICKED,OnCommandDumpOpen)
    COMMAND_HANDLER(kCommandRunLow,BN_CLICKED,OnCommandRunLow)
    COMMAND_HANDLER(kCommandLimitedJob,BN_CLICKED,OnCommandLimitedJob)
    COMMAND_HANDLER(kCommandDebugRun,BN_CLICKED,OnCommandDebugRun)
    COMMAND_HANDLER(kCommandParserDump,BN_CLICKED,OnCommandParserDump)
    COMMAND_HANDLER(kCommandCrashIt,BN_CLICKED,OnCommandCrashIt)
    COMMAND_RANGE_CODE_HANDLER(kCommandTestBegin,kCommandTestEnd,BN_CLICKED,OnCommandTest)
  END_MSG_MAP()

private:
  LRESULT OnClose(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnEraseBkgnd(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnFinalMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnCopyData(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnTimer(UINT msg, WPARAM wp, LPARAM lp, BOOL& handled);
  //LRESULT OnSetCursor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnCommandExport(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
  LRESULT OnCommandDumpMini(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
  LRESULT OnCommandDumpFull(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
  LRESULT OnCommandDumpOpen(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
  LRESULT OnCommandRunLow(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
  LRESULT OnCommandLimitedJob(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
  LRESULT OnCommandDebugRun(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
  LRESULT OnCommandParserDump(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
  LRESULT OnCommandCrashIt(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
  LRESULT OnCommandTest(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
  
private:
  void BuildAppButton();
  ProfilerData* GetProfilerData();

private:
  CButton btn_export_,btn_dump_mini_,btn_dump_full_,btn_dump_open_;
  CButton btn_runlow_,btn_limitedjob_,btn_debugrun_,btn_parser_dump_,btn_crash_;
  std::vector<CButton*> btn_test_;
  CListViewCtrl list_log_;
  CListViewCtrl list_proc_;
  DWORD pid_selected_;

  scoped_ptr<LogDaemon> log_server_;
};
