#pragma once

#include "base/logging.h"

class CommandLine;

namespace logging {

  void InitProfilerLogging(const CommandLine& command_line,
    OldFileDeletionState delete_old_log_file);

  // Call when done using logging for Profiler.
  void CleanupProfilerLogging();

}