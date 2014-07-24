#pragma once

#include "base/basictypes.h"

class CommandLine;

class ProfilerInit{
public:
  ProfilerInit();
  ~ProfilerInit();

public:
  bool Start(const CommandLine& cmdline,int* result_code);

private:
  DISALLOW_COPY_AND_ASSIGN(ProfilerInit);
};