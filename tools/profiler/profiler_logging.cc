#include "tools/profiler/profiler_logging.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/files/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/base_switches.h"
#include "base/strings/string_number_conversions.h"

namespace{
  bool profiler_logging_initialized_    = false;
  bool profiler_logging_failed_         = false;
}

namespace switches{
  const char kEnableLogging[]    = "enable-logging";
  const char kDisableLogging[]   = "disable-logging";
  const char kLoggingLevel[]     = "logging-level";
}

namespace logging{

  LoggingDestination DetermineLogMode(const CommandLine& command_line) 
  {
    // only use OutputDebugString in debug mode
#ifdef NDEBUG
    bool enable_logging = false;
    const char *kInvertLoggingSwitch = switches::kEnableLogging;
    const logging::LoggingDestination kDefaultLoggingMode =
      logging::LOG_TO_FILE;
#else
    bool enable_logging = true;
    const char *kInvertLoggingSwitch = switches::kDisableLogging;
    const logging::LoggingDestination kDefaultLoggingMode =
      logging::LOG_TO_ALL;
#endif

    if (command_line.HasSwitch(kInvertLoggingSwitch))
      enable_logging = !enable_logging;

    logging::LoggingDestination log_mode;
    if (enable_logging) {
      // Let --enable-logging=stderr force only stderr, particularly useful for
      // non-debug builds where otherwise you can't get logs to stderr at all.
      if (command_line.GetSwitchValueASCII(switches::kEnableLogging) == "stderr")
        log_mode = logging::LOG_TO_SYSTEM_DEBUG_LOG;
      else
        log_mode = kDefaultLoggingMode;
    } else {
      log_mode = logging::LOG_NONE;
    }
    return log_mode;
  }

  void InitProfilerLogging(const CommandLine& command_line,
    OldFileDeletionState delete_old_log_file) 
  {
    DCHECK(!profiler_logging_initialized_) <<
      "Attempted to initialize logging when it was already initialized.";

    LoggingDestination logging_dest = DetermineLogMode(command_line);

    base::FilePath log_path;
    if (logging_dest == LOG_TO_FILE ||
      logging_dest == LOG_TO_ALL) {
        PathService::Get(base::DIR_EXE,&log_path);
        std::wstring profile_name(L"profiler_log.txt");
        log_path = log_path.Append(profile_name);
        int i=1;
        while(base::PathExists(log_path))
        {
          log_path=log_path.DirName();
          log_path=log_path.Append(profile_name);
          log_path=log_path.InsertBeforeExtension(base::IntToString16(i));
          i++;
        }
    }

    logging::DcheckState dcheck_state =
      command_line.HasSwitch(switches::kEnableDCHECK) ?
      logging::ENABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS :
    logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS;

		LoggingSettings log_setting;
		log_setting.log_file = log_path.value().c_str();
		log_setting.logging_dest = DetermineLogMode(command_line);
    log_setting.lock_log = logging::LOCK_LOG_FILE;
    log_setting.delete_old = delete_old_log_file;
    log_setting.dcheck_state = dcheck_state;

		bool success = InitLogging(log_setting);
    if (!success) {
      PLOG(ERROR) << "Unable to initialize logging to " << log_path.value();
      profiler_logging_failed_ = true;
      return;
    }

    // Default to showing error dialogs.
    if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoErrorDialogs))
      logging::SetShowErrorDialogs(true);

    // we want process and thread IDs because we have a lot of things running
    logging::SetLogItems(true,  // enable_process_id
      true,  // enable_thread_id
      false, // enable_timestamp
      true); // enable_tickcount

    // Use a minimum log level if the command line asks for one,
    // otherwise leave it at the default level (INFO).
    if (command_line.HasSwitch(switches::kLoggingLevel)) {
      std::string log_level = command_line.GetSwitchValueASCII(
        switches::kLoggingLevel);
      int level = 0;
      if (base::StringToInt(log_level, &level) &&
        level >= 0 && level < LOG_NUM_SEVERITIES) {
          logging::SetMinLogLevel(level);
      } else {
        LOG(WARNING) << "Bad log level: " << log_level;
      }
    }

    profiler_logging_initialized_ = true;
  }

  // This is a no-op, but we'll keep it around in case
  // we need to do more cleanup in the future.
  void CleanupProfilerLogging() {
    if (profiler_logging_failed_)
      return;  // We failed to initiailize logging, no cleanup.

    DCHECK(profiler_logging_initialized_) <<
      "Attempted to clean up logging when it wasn't initialized.";

    CloseLogFile();

    profiler_logging_initialized_ = false;
  }

}
