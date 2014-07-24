#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

class CommandLine;
namespace base{
class MessageLoop;
}
class ProfilerThread;

class ProfilerMainDelegate {
public:
  static ProfilerMainDelegate* CreateProfilerMainDelegate(const CommandLine& command_line);
  virtual ~ProfilerMainDelegate();

  // Parts to be called by |ProfilerMain()|.
  void EarlyInitialization();
  void MainMessageLoopStart();

protected:
  explicit ProfilerMainDelegate(const CommandLine& command_line);

  base::MessageLoop& main_message_loop() const {
    return *main_message_loop_;
  }

  // Methods to be overridden to provide platform-specific code; these
  // correspond to the "parts" above.
  virtual void PreEarlyInitialization() {}
  virtual void PostEarlyInitialization() {}
  virtual void PreMainMessageLoopStart() {}
  virtual void PostMainMessageLoopStart() {}

private:
  // Methods for |EarlyInitialization()| ---------------------------------------

  // Methods for |MainMessageLoopStart()| --------------------------------------

  void InitializeMainThread();

  // Members initialized on construction ---------------------------------------

  const CommandLine& parsed_command_line_;

  // Members initialized in |MainMessageLoopStart()| ---------------------------
  scoped_ptr<base::MessageLoop> main_message_loop_;
  scoped_ptr<ProfilerThread> main_thread_;

  // Initialized in SetupMetricsAndFieldTrials.
  DISALLOW_COPY_AND_ASSIGN(ProfilerMainDelegate);
};
