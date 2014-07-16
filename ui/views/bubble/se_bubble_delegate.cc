// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/se_bubble_delegate.h"

#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/widget/widget.h"
#include "ui/gfx/animation/slide_animation.h"

// The duration of the fade animation in milliseconds.
static const int kHideFadeDurationMS = 200;

// The defaut margin between the content and the inside border, in pixels.
static const int kDefaultMargin = 6;

namespace views {

namespace {

  // Create a widget to host the bubble.
  Widget* CreateBubbleWidget(SeBubbleDelegateView* bubble) {
    Widget* bubble_widget = new Widget();
    Widget::InitParams bubble_params(Widget::InitParams::TYPE_BUBBLE);
    bubble_params.delegate = bubble;
    bubble_params.opacity = bubble->is_transparent() ? Widget::InitParams::TRANSLUCENT_WINDOW : Widget::InitParams::INFER_OPACITY;
    if (bubble->parent_window())
      bubble_params.parent = bubble->parent_window();
    if (bubble->use_focusless())
      bubble_params.can_activate = false;
    bubble_params.type = Widget::InitParams::TYPE_WINDOW_FRAMELESS;
    bubble_widget->Init(bubble_params);
    return bubble_widget;
  }

  class BubbleBorderDelegate : public WidgetDelegate,
    public views::WidgetObserver {
  public:
    BubbleBorderDelegate(SeBubbleDelegateView* bubble, Widget* widget)
      : bubble_(bubble),
      widget_(widget) {
    }

    virtual ~BubbleBorderDelegate() {
    }

    // WidgetDelegate overrides:
    virtual bool CanActivate() const OVERRIDE { return false; }
    virtual void DeleteDelegate() OVERRIDE { delete this; }
    virtual Widget* GetWidget() OVERRIDE { return widget_; }
    virtual const Widget* GetWidget() const OVERRIDE { return widget_; }
    virtual bool ShouldShowWindowTitle() const{return false;}
    // Returns true if the window should show a close button in the title bar.
    virtual bool ShouldShowCloseButton() const{return false;}
    virtual NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) OVERRIDE {
        BubbleFrameView* pView = new BubbleFrameView(
          gfx::Insets(bubble_->margin(), bubble_->margin(), bubble_->margin(), bubble_->margin()));
        BubbleBorder* pBorder = new BubbleBorder(BubbleBorder::NONE, bubble_->GetBorderShadow(), bubble_->color());
        pView->SetBubbleBorder(pBorder);
        pView->set_background(new BubbleBackground(pBorder));
        return pView;
    }

  private:
    SeBubbleDelegateView* bubble_;
    Widget* widget_;

    DISALLOW_COPY_AND_ASSIGN(BubbleBorderDelegate);
  };

  // Create a widget to host the bubble's border.
  Widget* CreateBorderWidget(SeBubbleDelegateView* bubble) {
    Widget* border_widget = new Widget();
    Widget::InitParams border_params(Widget::InitParams::TYPE_BUBBLE);
    border_params.delegate = new BubbleBorderDelegate(bubble, border_widget);
    border_params.opacity = Widget::InitParams::TRANSLUCENT_WINDOW;
    border_params.parent = bubble->GetWidget()->GetNativeView();
    border_params.can_activate = false;
    border_widget->Init(border_params);
    return border_widget;
  }

}  // namespace

SeBubbleDelegateView::SeBubbleDelegateView()
  : color_(color_utils::GetSysSkColor(COLOR_WINDOW)),
  margin_(kDefaultMargin),
  original_opacity_(255),
  border_widget_(NULL),
  use_focusless_(false),
  parent_window_(NULL) {
    set_background(views::Background::CreateSolidBackground(color_));
    AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

SeBubbleDelegateView::~SeBubbleDelegateView() {
}

// static
Widget* SeBubbleDelegateView::CreateBubble(SeBubbleDelegateView* bubble_delegate) {
  bubble_delegate->Init();
  Widget* bubble_widget = CreateBubbleWidget(bubble_delegate);

  // First set the contents view to initialize view bounds for widget sizing.
  bubble_widget->SetContentsView(bubble_delegate->GetContentsView());
  bubble_delegate->border_widget_ = CreateBorderWidget(bubble_delegate);

  bubble_delegate->SizeToContents();
  bubble_widget->AddObserver(bubble_delegate);
  return bubble_widget;
}

View* SeBubbleDelegateView::GetInitiallyFocusedView() {
  return this;
}

View* SeBubbleDelegateView::GetContentsView() {
  return this;
}

NonClientFrameView* SeBubbleDelegateView::CreateNonClientFrameView(
  Widget* widget) {
    return new BubbleFrameView(gfx::Insets(margin(), margin(), margin(), margin()));
}

void SeBubbleDelegateView::Show() {
  if (close_on_deactivate()){
    if (GetWidget())
      GetWidget()->Show();
    if (border_widget_)
      border_widget_->Show();
  }else{
    if (GetWidget())
    ShowWindow(GetWidget()->GetNativeView(), SW_SHOWNOACTIVATE);
  if (border_widget_)
    ShowWindow(border_widget_->GetNativeView(), SW_SHOWNOACTIVATE);
  }
}

void SeBubbleDelegateView::SetAlignment(BubbleBorder::BubbleAlignment alignment) {
  GetBubbleFrameView()->bubble_border()->set_alignment(alignment);
  SizeToContents();
}

void SeBubbleDelegateView::Init() {}

void SeBubbleDelegateView::SizeToContents() {
  if (is_transparent()){
    GetWidget()->SetBounds(GetBubbleClientBounds());
  }
  else{
    border_widget_->SetBounds(GetBubbleBounds());
    GetWidget()->SetBounds(GetBubbleClientBounds());
    GetBubbleFrameView()->bubble_border()->set_client_bounds(
      GetBubbleFrameView()->GetBoundsForClientView());
  }
}

BubbleFrameView* SeBubbleDelegateView::GetBubbleFrameView() const {
  const Widget* widget = border_widget_ ? border_widget_ : GetWidget();
  const NonClientView* view = widget ? widget->non_client_view() : NULL;
  return view ? static_cast<BubbleFrameView*>(view->frame_view()) : NULL;
}

gfx::Rect SeBubbleDelegateView::GetBubbleBounds() {
  return gfx::Rect();
}

gfx::Rect SeBubbleDelegateView::GetBubbleClientBounds(){
  if (is_transparent()){
    return GetBubbleBounds();
  }

  gfx::Rect client_bounds(GetBubbleFrameView()->GetBoundsForClientView());
  client_bounds.Offset(border_widget_->GetClientAreaBoundsInScreen().origin().x(), 
    border_widget_->GetClientAreaBoundsInScreen().origin().y());
  return client_bounds;
}

void SeBubbleDelegateView::StartFade(bool fade_in, int during_ms) {
  fade_animation_.reset(new gfx::SlideAnimation(this));
  fade_animation_->SetSlideDuration(during_ms == -1 ? kHideFadeDurationMS : during_ms);
  fade_animation_->Reset(fade_in ? 0.0 : 1.0);
  animate_bounds_ = GetBubbleClientBounds();
  animate_border_bounds_ = GetBubbleBounds();
  if (fade_in) {
    original_opacity_ = 0;
    if (border_widget_){
      border_widget_->SetOpacity(original_opacity_);
      SetWindowPos(border_widget_->GetNativeView(), NULL, animate_border_bounds_.x(), animate_border_bounds_.y(), 
        animate_border_bounds_.width(), 0, SWP_NOACTIVATE|SWP_NOZORDER);
    }
    GetWidget()->SetOpacity(original_opacity_);
    SetWindowPos(GetWidget()->GetNativeView(), NULL, animate_bounds_.x(), animate_bounds_.y(), 
      animate_bounds_.width(), 0, SWP_NOACTIVATE|SWP_NOZORDER);
    
    fade_animation_->Show();
    Show();
  } else {
    original_opacity_ = 255;
    fade_animation_->Hide();
  }
}

void SeBubbleDelegateView::ChangeAnimateBounds(){
  if (fade_animation_.get() && fade_animation_->is_animating()){
    animate_bounds_ = GetBubbleClientBounds();
    animate_border_bounds_ = GetBubbleBounds();
  }
}

void SeBubbleDelegateView::ResetFade() {
  fade_animation_.reset();
  if (border_widget_)
    border_widget_->SetOpacity(original_opacity_);
  GetWidget()->SetOpacity(original_opacity_);
}

void SeBubbleDelegateView::AnimationEnded(const gfx::Animation* animation) {
  if (animation != fade_animation_.get())
    return;
  fade_animation_->Reset();
}

void SeBubbleDelegateView::AnimationProgressed(const gfx::Animation* animation) {
  if (animation != fade_animation_.get())
    return;
  DCHECK(fade_animation_->is_animating());
  unsigned char opacity = fade_animation_->GetCurrentValue() * 255;

  if (is_transparent()){
    GetWidget()->SetOpacity(opacity);
  }else{
    const HWND hwnd = GetWidget()->GetNativeView();
    const DWORD style = GetWindowLong(hwnd, GWL_EXSTYLE);
    if ((opacity == 255) == !!(style & WS_EX_LAYERED))
      SetWindowLong(hwnd, GWL_EXSTYLE, style ^ WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd, 0, opacity, LWA_ALPHA);

    GetWidget()->SetOpacity(opacity);
    border_widget_->SetOpacity(opacity);
  }

  //¸üÐÂÎ»ÖÃ
  SizeToContents();
}

void SeBubbleDelegateView::OnWidgetActivationChanged( Widget* widget, bool active ){
  if (close_on_deactivate() && widget == GetWidget() && !active)
    GetWidget()->Close();
}

}  // namespace views
