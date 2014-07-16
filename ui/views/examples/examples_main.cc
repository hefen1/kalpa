// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/message_loop/message_loop.h"
#include "examples_window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/win/scoped_ole_initializer.h"

int main(int argc, char** argv) {
  ui::ScopedOleInitializer ole_init;

  CommandLine::Init(0, NULL);
  base::AtExitManager exit_manager;

  base::MessageLoop main_message_loop(base::MessageLoop::TYPE_UI);

  base::i18n::InitializeICU();
  ResourceBundle::InitSharedInstanceWithLocale("", NULL);

  views::examples::ShowExamplesWindow(views::examples::QUIT_ON_CLOSE);

  main_message_loop.Run();

  ui::ResourceBundle::CleanupSharedInstance();

  return 0;
}