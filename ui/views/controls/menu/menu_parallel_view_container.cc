// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_parallel_view_container.h"

#if defined(OS_WIN)
#include <windows.h>
#include <uxtheme.h>
#include <Vssym32.h>
#endif

#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/controls/menu/submenu_parallel_view.h"

using ui::NativeTheme;

// Height of the scroll arrow.
// This goes up to 4 with large fonts, but this is close enough for now.
static const int scroll_arrow_height = 3;

namespace views {

MenuParallelViewContainer::MenuParallelViewContainer(SubmenuParallelView* content_view)
    : MenuScrollViewContainer(content_view), content_view_(content_view) {
  AddChildView(content_view_);

  set_border(Border::CreateSolidSidedBorder(
                 SubmenuParallelView::kSubmenuBorderSize,
                 SubmenuParallelView::kSubmenuBorderSize,
                 SubmenuParallelView::kSubmenuBorderSize,
                 SubmenuParallelView::kSubmenuBorderSize,
                 SkColorSetRGB(218,218,218)));
}

void MenuParallelViewContainer::OnPaintBackground(gfx::Canvas* canvas) {
  if (background()) {
    View::OnPaintBackground(canvas);
    return;
  }

#if defined(OS_WIN)
  HDC dc = canvas->BeginPlatformPaint();
#endif
  gfx::Rect bounds(0, 0, width(), height());
  NativeTheme::ExtraParams extra;
  NativeTheme::instance()->Paint(canvas->sk_canvas(),
      NativeTheme::kMenuPopupBackground, NativeTheme::kNormal, bounds, extra);
#if defined(OS_WIN)
  canvas->EndPlatformPaint();
#endif
}

void MenuParallelViewContainer::Layout() {
  gfx::Insets insets = GetInsets();
  int x = insets.left();
  int y = insets.top();
  int width = View::width() - insets.width();
  int content_height = height() - insets.height();
  content_view_->SetBounds(x, y, width, content_height);
  content_view_->Layout();
}

gfx::Size MenuParallelViewContainer::GetPreferredSize() {
  gfx::Size prefsize = content_view_->GetPreferredSize();
  gfx::Insets insets = GetInsets();
  prefsize.Enlarge(insets.width(), insets.height());
  return prefsize;
}

void MenuParallelViewContainer::GetAccessibleState(
    ui::AccessibleViewState* state) {
  // Get the name from the submenu view.
  content_view_->GetAccessibleState(state);

  // Now change the role.
  state->role = ui::AccessibilityTypes::ROLE_MENUBAR;
  // Some AT (like NVDA) will not process focus events on menu item children
  // unless a parent claims to be focused.
  state->state = ui::AccessibilityTypes::STATE_FOCUSED;
}

void MenuParallelViewContainer::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  Layout();
}

}  // namespace views
