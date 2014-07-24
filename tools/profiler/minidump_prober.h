#pragma once

#include <string>
#include <stdio.h>

class CMiniDumpReader;
class CMiniDumpProber{
public:
  explicit CMiniDumpProber();
  ~CMiniDumpProber();

  bool Open(const std::wstring& dump_path,const std::wstring& sym_path);
  bool Export(const std::wstring& log_path);

private:
  void doExport(FILE* f);

  CMiniDumpReader* reader_;
};
