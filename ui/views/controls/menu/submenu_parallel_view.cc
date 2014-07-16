// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/submenu_parallel_view.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ui_strings.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_host.h"
#include "ui/views/controls/menu/menu_parallel_view_container.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

namespace {

// Height of the drop indicator. This should be an even number.
const int kDropIndicatorHeight = 2;

// Color of the drop indicator.
const SkColor kDropIndicatorColor = SK_ColorBLACK;

// 单列最大最小宽度=
const int kMaxSingleColumnWidth = 320;
const int kMinSingleColumnWidth = 100;
const int kColumnBetweenWidth = 10;

// show more id
const int kShowMoreID = 0x19999999;

}  // namespace

namespace views {

// static
const int SubmenuParallelView::kSubmenuBorderSize = 1;

// static
const char SubmenuParallelView::kViewClassName[] = "views/SubmenuParallelView";

SubmenuParallelView::SubmenuParallelView(MenuItemView* parent)
    : SubmenuView(parent),
      parent_menu_item_(parent),
      host_(NULL),
      drop_item_(NULL),
      drop_position_(MenuDelegate::DROP_NONE),
      parallel_view_container_(NULL),
      max_accelerator_width_(0),
      minimum_preferred_width_(0),
      max_width_(0),
      max_height_(0),
      column_width_(0),
      resize_open_menu_(false),
      show_more_(false) {
  DCHECK(parent);
  // We'll delete ourselves, otherwise the ScrollView would delete us on close.
  set_owned_by_client();
}

SubmenuParallelView::~SubmenuParallelView() {
  // The menu may not have been closed yet (it will be hidden, but not
  // necessarily closed).
  Close();

  delete parallel_view_container_;
}

int SubmenuParallelView::GetMenuItemCount() {
  int count = 0;
  for (int i = 0; i < child_count(); ++i) {
    if (child_at(i)->id() == MenuItemView::kMenuItemViewID)
      count++;
  }
  return count;
}

MenuItemView* SubmenuParallelView::GetMenuItemAt(int index) {
  for (int i = 0, count = 0; i < child_count(); ++i) {
    if (child_at(i)->id() == MenuItemView::kMenuItemViewID &&
        count++ == index) {
      return static_cast<MenuItemView*>(child_at(i));
    }
  }
  NOTREACHED();
  return NULL;
}

void SubmenuParallelView::ChildPreferredSizeChanged(View* child) {
  if (!resize_open_menu_)
    return;

  MenuItemView *item = GetMenuItem();
  MenuController* controller = item->GetMenuController();

  if (controller) {
    bool dir;
    gfx::Rect bounds = controller->CalculateMenuBounds(item, false, &dir);
    Reposition(bounds);
  }
}

void SubmenuParallelView::Layout() {
  // We're in a ScrollView, and need to set our width/height ourselves.
  if (!parent())
    return;

  size_t cur_column = 1;
  gfx::Insets insets = GetInsets();
  int x = insets.left();
  int y = insets.top();
  for (int i = 0; i < child_count(); ++i) {
    View* child = child_at(i);
    if (child->visible()) {
      gfx::Size child_pref_size = child->GetPreferredSize();
      child->SetBounds(x, y, column_width_, child_pref_size.height());
      y += child_pref_size.height();

      DCHECK(cur_column <= columns_size_.size());
      if(cur_column <= columns_size_.size() &&
        y >= columns_size_.at(cur_column - 1).height()) {
        //开始分列=
        x += column_width_;
        x += kColumnBetweenWidth;
        y = insets.top();
        cur_column++;

        // 2013.8.22 产品需求，暂时不需要显示更多。
        // 显示不全，先隐藏掉显示在外的，再加上“显示更多...”=
       if(x >= width() && show_more_) {
          for(int j = i - 1; j < child_count(); ++j) {
            View* child = child_at(j);
            child->SetVisible(false);
          }

          // separator
          parent_menu_item_->AppendSeparator();
          View* child = child_at(child_count() - 1);
          child->SetBounds(x, y, column_width_, child->GetPreferredSize().height());

          // add more
          parent_menu_item_->AppendMenuItem(kShowMoreID,
            UTF16ToWide(l10n_util::GetStringUTF16(IDS_BOOKMARK_MENU_SHOW_MORE)),
            MenuItemView::NORMAL);
         break;
       }
      }
    }
  }
}

gfx::Size SubmenuParallelView::GetPreferredSize() {
  if (!has_children())
    return gfx::Size();

  columns_size_.clear();
  column_width_ = 0;
  max_accelerator_width_ = 0;
  int accelerator_width = 0;
  int max_width = 0;
  int height = 0;
  int total_width = 0;
  int total_height = 0;
  for (int i = 0; i < child_count(); ++i) {
    View* child = child_at(i);
    gfx::Size child_pref_size = child->visible() ? child->GetPreferredSize()
                                                 : gfx::Size();
    max_width = std::max(max_width, child_pref_size.width());
    if(height + child_pref_size.height() > max_height_) {
      //开始分列=
      max_width = std::min(max_width + accelerator_width +
      MenuConfig::instance(NULL).label_to_minor_text_padding, kMaxSingleColumnWidth);
      column_width_ = std::max(max_width, column_width_);
      total_height = std::max(total_height, height);
      columns_size_.push_back(gfx::Size(max_width, height));
      max_width = child_pref_size.width();
      height = 0;
      total_width = 0;
    }
    height += child_pref_size.height();
    if (child->id() == MenuItemView::kMenuItemViewID) {
      MenuItemView* menu = static_cast<MenuItemView*>(child);
      int menu_acc_width = menu->GetDimensions().minor_text_width;
      max_accelerator_width_ =
          std::max(max_accelerator_width_, menu_acc_width);
      accelerator_width = 
        std::max(accelerator_width, menu_acc_width);
    }
  }
  if(height != 0) {
    max_width = std::min(max_width + accelerator_width +
    MenuConfig::instance(NULL).label_to_minor_text_padding, kMaxSingleColumnWidth);
    column_width_ = std::max(max_width, column_width_);
    total_height = std::max(total_height, height);
    columns_size_.push_back(gfx::Size(max_width, height));
  }
  int columns = static_cast<int>(columns_size_.size());
  total_width = column_width_ * columns + kColumnBetweenWidth * (columns - 1);
  if (max_accelerator_width_ > 0) {
    max_accelerator_width_ +=
        MenuConfig::instance(NULL).label_to_minor_text_padding;
  }
  gfx::Insets insets = GetInsets();
  gfx::Size size(
    std::max(total_width + insets.width(),
    minimum_preferred_width_ - 2 * kSubmenuBorderSize),
    total_height + insets.height());

  if((kMinSingleColumnWidth  * columns +
      kColumnBetweenWidth * (columns - 1) +
      insets.width()) > max_width_) {
    // 采用最小宽度还不够，将显示“显示更多...” [6/8/2012 guoxiaolong-browser]
    int width = kMinSingleColumnWidth;
    columns = 1;
    while(1) {
      if(width + kColumnBetweenWidth + kMinSingleColumnWidth > max_width_)
        break;
      width = width + kColumnBetweenWidth + kMinSingleColumnWidth;
      columns++;
    }
    column_width_ = (max_width_ - insets.width() -
      kColumnBetweenWidth * (columns - 1)) / columns;
    size.set_width(column_width_ * columns + kColumnBetweenWidth * (columns - 1) +
                   insets.width());
    show_more_ = true;
  } else {
    // 能全部显示 [6/8/2012 guoxiaolong-browser]
    if(size.width() > max_width_) {
      column_width_ = (max_width_ - insets.width() -
                      kColumnBetweenWidth * (columns - 1)) / columns;
      size.set_width(column_width_ * columns + kColumnBetweenWidth * (columns - 1) +
                     insets.width());
    }
    show_more_ = false;
  }

  return size;
}

void SubmenuParallelView::GetAccessibleState(ui::AccessibleViewState* state) {
  // Inherit most of the state from the parent menu item, except the role.
  if (GetMenuItem())
    GetMenuItem()->GetAccessibleState(state);
  state->role = ui::AccessibilityTypes::ROLE_MENUPOPUP;
}

void SubmenuParallelView::PaintChildren(gfx::Canvas* canvas) {
  View::PaintChildren(canvas);

  if (drop_item_ && drop_position_ != MenuDelegate::DROP_ON)
    PaintDropIndicator(canvas, drop_item_, drop_position_);
}

void SubmenuParallelView::OnPaint(gfx::Canvas* canvas) {
  if(columns_size_.size() <= 1)
    return;

  const ui::NativeTheme* theme = ui::NativeTheme::instance();
  int start_x = column_width_;
  while (start_x < width()) {
    gfx::Rect separator_bounds(start_x + 4, 0, 1, height());
    canvas->FillRect(separator_bounds, SkColorSetRGB(218,218,218));
    start_x += (kColumnBetweenWidth + column_width_);
  }
}

bool SubmenuParallelView::GetDropFormats(
      int* formats,
      std::set<OSExchangeData::CustomFormat>* custom_formats) {
  DCHECK(GetMenuItem()->GetMenuController());
  return GetMenuItem()->GetMenuController()->GetDropFormats(this, formats,
                                                            custom_formats);
}

bool SubmenuParallelView::AreDropTypesRequired() {
  DCHECK(GetMenuItem()->GetMenuController());
  return GetMenuItem()->GetMenuController()->AreDropTypesRequired(this);
}

bool SubmenuParallelView::CanDrop(const OSExchangeData& data) {
  DCHECK(GetMenuItem()->GetMenuController());
  return GetMenuItem()->GetMenuController()->CanDrop(this, data);
}

void SubmenuParallelView::OnDragEntered(const ui::DropTargetEvent& event) {
  DCHECK(GetMenuItem()->GetMenuController());
  GetMenuItem()->GetMenuController()->OnDragEntered(this, event);
}

int SubmenuParallelView::OnDragUpdated(const ui::DropTargetEvent& event) {
  DCHECK(GetMenuItem()->GetMenuController());
  return GetMenuItem()->GetMenuController()->OnDragUpdated(this, event);
}

void SubmenuParallelView::OnDragExited() {
  DCHECK(GetMenuItem()->GetMenuController());
  GetMenuItem()->GetMenuController()->OnDragExited(this);
}

int SubmenuParallelView::OnPerformDrop(const ui::DropTargetEvent& event) {
  DCHECK(GetMenuItem()->GetMenuController());
  return GetMenuItem()->GetMenuController()->OnPerformDrop(this, event);
}

bool SubmenuParallelView::OnMouseWheel(const ui::MouseWheelEvent& e) {
  return true;
}

void SubmenuParallelView::OnGestureEvent(ui::GestureEvent* event) {
}

bool SubmenuParallelView::IsShowing() {
  return host_ && host_->IsMenuHostVisible();
}

void SubmenuParallelView::ShowAt(Widget* parent,
                         const gfx::Rect& bounds,
                         bool do_capture) {
  if (host_) {
    host_->ShowMenuHost(do_capture);
  } else {
    host_ = new MenuHost(this);
    // Force construction of the scroll view container.
    GetParallelViewContainer();
    // Make sure the first row is visible.
    ScrollRectToVisible(gfx::Rect(gfx::Size(1, 1)));
    host_->InitMenuHost(parent, bounds, parallel_view_container_, do_capture);
  }

  GetParallelViewContainer()->NotifyAccessibilityEvent(
      ui::AccessibilityTypes::EVENT_MENUSTART,
      true);
  NotifyAccessibilityEvent(
      ui::AccessibilityTypes::EVENT_MENUPOPUPSTART,
      true);
}

void SubmenuParallelView::Reposition(const gfx::Rect& bounds) {
  if (host_)
    host_->SetMenuHostBounds(bounds);
}

void SubmenuParallelView::Close() {
  if (host_) {
    NotifyAccessibilityEvent(
        ui::AccessibilityTypes::EVENT_MENUPOPUPEND,
        true);
    GetParallelViewContainer()->NotifyAccessibilityEvent(
        ui::AccessibilityTypes::EVENT_MENUEND,
        true);

    host_->DestroyMenuHost();
    host_ = NULL;
  }
}

void SubmenuParallelView::Hide() {
  if (host_)
    host_->HideMenuHost();
}

void SubmenuParallelView::ReleaseCapture() {
  if (host_)
    host_->ReleaseMenuHostCapture();
}

bool SubmenuParallelView::SkipDefaultKeyEventProcessing(const ui::KeyEvent& e) {
  return views::FocusManager::IsTabTraversalKeyEvent(e);
}

MenuItemView* SubmenuParallelView::GetMenuItem() const {
  return parent_menu_item_;
}

void SubmenuParallelView::SetDropMenuItem(MenuItemView* item,
                                  MenuDelegate::DropPosition position) {
  if (drop_item_ == item && drop_position_ == position)
    return;
  SchedulePaintForDropIndicator(drop_item_, drop_position_);
  drop_item_ = item;
  drop_position_ = position;
  SchedulePaintForDropIndicator(drop_item_, drop_position_);
}

bool SubmenuParallelView::GetShowSelection(MenuItemView* item) {
  if (drop_item_ == NULL)
    return true;
  // Something is being dropped on one of this menus items. Show the
  // selection if the drop is on the passed in item and the drop position is
  // ON.
  return (drop_item_ == item && drop_position_ == MenuDelegate::DROP_ON);
}

MenuScrollViewContainer* SubmenuParallelView::GetScrollViewContainer() {
  return GetParallelViewContainer();
}

MenuParallelViewContainer* SubmenuParallelView::GetParallelViewContainer() {
  if (!parallel_view_container_) {
    parallel_view_container_ = new MenuParallelViewContainer(this);
    // Otherwise MenuHost would delete us.
    parallel_view_container_->set_owned_by_client();
  }
  return parallel_view_container_;
}

void SubmenuParallelView::MenuHostDestroyed() {
  host_ = NULL;
  GetMenuItem()->GetMenuController()->Cancel(MenuController::EXIT_DESTROYED);
}

const char* SubmenuParallelView::GetClassName() const {
  return kViewClassName;
}

void SubmenuParallelView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  SchedulePaint();
}

void SubmenuParallelView::PaintDropIndicator(gfx::Canvas* canvas,
                                     MenuItemView* item,
                                     MenuDelegate::DropPosition position) {
  if (position == MenuDelegate::DROP_NONE)
    return;

  gfx::Rect bounds = CalculateDropIndicatorBounds(item, position);
  canvas->FillRect(bounds, kDropIndicatorColor);
}

void SubmenuParallelView::SchedulePaintForDropIndicator(
    MenuItemView* item,
    MenuDelegate::DropPosition position) {
  if (item == NULL)
    return;

  if (position == MenuDelegate::DROP_ON) {
    item->SchedulePaint();
  } else if (position != MenuDelegate::DROP_NONE) {
    SchedulePaintInRect(CalculateDropIndicatorBounds(item, position));
  }
}

gfx::Rect SubmenuParallelView::CalculateDropIndicatorBounds(
    MenuItemView* item,
    MenuDelegate::DropPosition position) {
  DCHECK(position != MenuDelegate::DROP_NONE);
  gfx::Rect item_bounds = item->bounds();
  switch (position) {
    case MenuDelegate::DROP_BEFORE:
      item_bounds.Offset(0, -kDropIndicatorHeight / 2);
      item_bounds.set_height(kDropIndicatorHeight);
      return item_bounds;

    case MenuDelegate::DROP_AFTER:
      item_bounds.Offset(0, item_bounds.height() - kDropIndicatorHeight / 2);
      item_bounds.set_height(kDropIndicatorHeight);
      return item_bounds;

    default:
      // Don't render anything for on.
      return gfx::Rect();
  }
}

bool SubmenuParallelView::OnScroll(float dx, float dy) {
  return true;
}

}  // namespace views
