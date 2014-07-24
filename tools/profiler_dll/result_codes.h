#pragma once

//#include "base/process_util.h"

class ResultCodes {
 public:
  enum ExitCode {
    NORMAL_EXIT = 0,            // Process terminated normally.
    OUTOFMEM_EXIT,

    NORMAL_EXIT_EXP1,           // The EXP1, EXP2, EXP3, EXP4 are generic codes
    NORMAL_EXIT_EXP2,           // used to communicate some simple outcome back
    NORMAL_EXIT_EXP3,           // to the process that launched us. This is
    NORMAL_EXIT_EXP4,           // used for experiments and the actual meaning
                                // depends on the experiment.

    EXIT_LAST_CODE              // Last return code (keep it last).
  };
};
