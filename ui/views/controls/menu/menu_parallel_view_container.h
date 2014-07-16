// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_PARALLEL_VIEW_CONTAINER_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_PARALLEL_VIEW_CONTAINER_H_
#pragma once

#include "ui/views/view.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"

namespace views {

class SubmenuParallelView;
class MenuScrollViewContainer;

// MenuParallelViewContainer contains the SubmenuParallelView (through a MenuScrollView)
// and two scroll buttons. The scroll buttons are only visible and enabled if
// the preferred height of the SubmenuParallelView is bigger than our bounds.
class MenuParallelViewContainer : public MenuScrollViewContainer {
 public:
  explicit MenuParallelViewContainer(SubmenuParallelView* content_view);

  // View overrides.
  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

 protected:
  // View override.
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;

 private:
  // The content view.
  SubmenuParallelView* content_view_;

  DISALLOW_COPY_AND_ASSIGN(MenuParallelViewContainer);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_SCROLL_VIEW_CONTAINER_H_
