// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_SE_BUBBLE_BUBBLE_DELEGATE_H_
#define UI_VIEWS_SE_BUBBLE_BUBBLE_DELEGATE_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
  class SlideAnimation;
}  // namespace ui

namespace views {

class BubbleFrameView;
// SeBubbleDelegateView creates frame and client views for bubble Widgets.
// SeBubbleDelegateView itself is the client's contents view.
//
///////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT SeBubbleDelegateView : 
  public WidgetDelegateView,
  public gfx::AnimationDelegate,
  public views::WidgetObserver {
public:

  SeBubbleDelegateView();
  virtual ~SeBubbleDelegateView();

  // Create and initialize the bubble Widget(s) with proper bounds.
  static Widget* CreateBubble(SeBubbleDelegateView* bubble_delegate);

  // WidgetDelegate overrides:
  virtual View* GetInitiallyFocusedView() OVERRIDE;
  virtual View* GetContentsView() OVERRIDE;
  virtual NonClientFrameView* CreateNonClientFrameView(
    views::Widget* widget) OVERRIDE;

  SkColor color() const { return color_; }
  void set_color(SkColor color) { color_ = color; }

  int margin() const { return margin_; }
  void set_margin(int margin) { margin_ = margin; }

  gfx::NativeView parent_window() const { return parent_window_; }
  void set_parent_window(gfx::NativeView window) { parent_window_ = window; }

  bool use_focusless() const { return use_focusless_; }
  void set_use_focusless(bool use_focusless) {
    use_focusless_ = use_focusless;
  }
  bool close_on_deactivate() const { return close_on_deactivate_; }
  void set_close_on_deactivate(bool close_on_deactivate) {
    close_on_deactivate_ = close_on_deactivate;
  }
  // Show the bubble's widget (and |border_widget_| on Windows).
  void Show();

  // Sets the bubble alignment relative to the anchor.
  void SetAlignment(BubbleBorder::BubbleAlignment alignment);

  void BorderWidgetClosing();

  virtual bool is_transparent(){return true;}
  virtual BubbleBorder::Shadow GetBorderShadow(){return BubbleBorder::SHADOW;};
protected:
  // Get bubble bounds from the anchor point and client view's preferred size.
  virtual gfx::Rect GetBubbleBounds();

  // Perform view initialization on the contents for bubble sizing.
  virtual void Init();

  // Resizes and potentially moves the Bubble to best accommodate the
  // contents preferred size.
  void SizeToContents();

  BubbleFrameView* GetBubbleFrameView() const;

  // ui::AnimationDelegate overrides:
  virtual void AnimationEnded(const gfx::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;

  virtual void OnWidgetActivationChanged(Widget* widget, bool active) OVERRIDE;
  void StartFade(bool fade_in, int during_ms);
  void ResetFade();
  void ChangeAnimateBounds();

  protected:
  gfx::Rect GetBubbleClientBounds();

  // The arrow's location on the bubble.
  BubbleBorder::Arrow arrow_location_;

  // The background color of the bubble.
  SkColor color_;

  // The margin between the content and the inside of the border, in pixels.
  int margin_;

  // Fade animation for bubble.
  scoped_ptr<gfx::SlideAnimation> fade_animation_;

  // Original opacity of the bubble.
  int original_opacity_;

  gfx::Rect animate_bounds_;
  gfx::Rect animate_border_bounds_;

protected:
  // The widget hosting the border for this bubble (non-Aura Windows only).
  Widget* border_widget_;

  // Create a popup window for focusless bubbles on Linux/ChromeOS.
  // These bubbles are not interactive and should not gain focus.
  bool use_focusless_;

  bool close_on_deactivate_;
  // Parent native window of the bubble.
  gfx::NativeView parent_window_;

  DISALLOW_COPY_AND_ASSIGN(SeBubbleDelegateView);
};

}  // namespace views

#endif  // UI_VIEWS_SE_BUBBLE_BUBBLE_DELEGATE_H_
