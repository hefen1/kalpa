#pragma once

//��ʱ��uiֱ�ӵ��õ�һЩ����Ժ��ع���io�߳��ϣ�������python�߳����ṩ��
//ui�������ݴ������Ȼ��ѽ����ui. �����������ipc��ipc�յ����ݺ��ui����ʾ
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