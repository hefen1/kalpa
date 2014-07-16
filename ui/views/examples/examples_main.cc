// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "examples_window.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/message_loop/message_loop.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/memory.h"

#include "ui/base/clipboard/clipboard.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/win/scoped_ole_initializer.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gfx/gfx_paths.h"

int main(int argc, char** argv) {
  // Env
	base::AtExitManager exit_manager;
  CommandLine::Init(0, NULL);

	// Debug
  base::EnableTerminationOnHeapCorruption();

  // Initialize logging.
  base::FilePath exe;
  PathService::Get(base::FILE_EXE, &exe);
  base::FilePath log_filename = exe.ReplaceExtension(FILE_PATH_LITERAL("log"));
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file = log_filename.value().c_str();
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::InitLogging(settings);
  logging::SetLogItems(true, true, true, true);

	// ICU
  base::i18n::InitializeICU();

	// Path Provider
  ui::RegisterPathProvider();
  gfx::RegisterPathProvider();

	// MainLoop
  {
		ui::ScopedOleInitializer ole_init;
		base::MessageLoop main_message_loop(base::MessageLoop::TYPE_UI);

		ui::ResourceBundle::InitSharedInstanceWithLocale("", NULL);

		views::examples::ShowExamplesWindow(views::examples::QUIT_ON_CLOSE);

		main_message_loop.Run();

		ui::ResourceBundle::CleanupSharedInstance();
		ui::Clipboard::DestroyClipboardForCurrentThread(); //Note!!!
  }

  return 0;
}