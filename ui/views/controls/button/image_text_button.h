// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_BUTTON_IMAGE_TEXT_BUTTON_H_
#define VIEWS_CONTROLS_BUTTON_IMAGE_TEXT_BUTTON_H_

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/gfx/font.h"
#include "ui/gfx/skbitmap_operations.h"

namespace views {

// An image button.

// Note that this type of button is not focusable by default and will not be
// part of the focus chain.  Call SetFocusable(true) to make it part of the
// focus chain.

class VIEWS_EXPORT ImageTextButton : public ImageButton {
 public:
  explicit ImageTextButton(ButtonListener* listener, const std::wstring& title);
  virtual ~ImageTextButton();

  // Overridden from View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  const gfx::Font& font() { return font_;}
  void SetFont(const gfx::Font& font);
  void SetFontColor(const SkColor& color);
  void SetDisabledFontColor(const SkColor& disabled_color);

  void SetText(const string16& text);

  // set & char as underline
  void SetShowPrefix(bool show_prefix) { show_prefix_ = show_prefix; }

  // Get or set the option to handle the return key; false by default.
  bool is_default() const { return is_default_; }
  void SetIsDefault(bool is_default);

 private:
  std::wstring text_;
  bool show_prefix_;
  // The font used to paint the text.
  gfx::Font font_;
  // Text color.
  SkColor color_;
  // SkColor color_enabled_;
  SkColor color_disabled_;
  // Flag indicating default handling of the return key via an accelerator.
  // Whether or not the button appears or behaves as the default button in its
  // current context;
  bool is_default_;

  DISALLOW_COPY_AND_ASSIGN(ImageTextButton);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_BUTTON_IMAGE_BUTTON_H_
