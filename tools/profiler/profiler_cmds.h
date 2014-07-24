#pragma once

//暂时给ui直接调用的一些命令，以后重构到io线程上，数据由python线程来提供，
//ui操作数据触发命令，然后把结果给ui. 对于命令触发到ipc，ipc收到数据后给ui来显示
#include <windows.h>

namespace ProfilerCmd{
void DumpGlobalVar(HWND parent);
void WriteMinidump(bool fulldump);
void RunLow(HWND parent);
void LimitedJob(HWND parent);
void OpenMinidumpFolder();
void DebugRun();
void ParserMinidump(HWND parent);
void CrashIt(DWORD pid);
}