// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/image_text_button.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/base/resource/resource_bundle.h"

namespace views {

  namespace {
    // The spacing between the icon edge and text.
    const int kSpacing = 14;
  }

////////////////////////////////////////////////////////////////////////////////
// ImageTextButton, public:

ImageTextButton::ImageTextButton(ButtonListener* listener,const std::wstring & text)
  : ImageButton(listener), text_(text),
    font_(ResourceBundle::GetSharedInstance().GetFont(
    ResourceBundle::BaseFont)),
    color_(SK_ColorBLACK),
    color_disabled_(SkColorSetRGB(161, 161, 146)),
    show_prefix_(false),
    is_default_(false) {
  // By default, we request that the gfx::Canvas passed to our View::Paint()
  // implementation is flipped horizontally so that the button's bitmaps are
  // mirrored when the UI directionality is right-to-left.
  EnableCanvasFlippingForRTLUI(true);
}

ImageTextButton::~ImageTextButton() {
}

gfx::Size ImageTextButton::GetPreferredSize() {
  gfx::Size image_button = ImageButton::GetPreferredSize();
  return gfx::Size(std::max(font_.GetStringWidth(text_) + kSpacing*2,image_button.width()),
                   image_button.height());
}

void ImageTextButton::OnPaint(gfx::Canvas* canvas) {
  // Call the base class first to paint any background/borders.
  View::OnPaint(canvas);

  gfx::ImageSkia img = GetImageToPaint();
  if (!img.isNull()) {
    gfx::Point position = ComputeImagePaintPosition(img);

    if (!background_image_.isNull())
      canvas->DrawImageInt(background_image_, position.x(), position.y());

    if (img.size().width() < font_.GetStringWidth(text_) + kSpacing*2) {
      SkPaint paint;
      paint.setFilterBitmap(true);
      canvas->DrawImageInt(img, 0, 0, img.width(), img.height(),
        0, 0,  font_.GetStringWidth(text_) + kSpacing*2,
        img.height(), true, paint);
    } else {
      canvas->DrawImageInt(img, position.x(), position.y());
    }
  }

  if(text_.size()){
    size_t max_width = std::max(img.size().width(), font_.GetStringWidth(text_) + kSpacing*2);
    gfx::Size image_button = ImageButton::GetPreferredSize();
    SkColor color= this->enabled()==true?color_:color_disabled_;
    canvas->DrawStringInt(text_, font_, color,
      (max_width-font_.GetStringWidth(text_))/2,
      (image_button.height()-font_.GetHeight())/2,
      font_.GetStringWidth(text_), font_.GetHeight(),
      show_prefix_ ? gfx::Canvas::SHOW_PREFIX : 0);
  }
}

void ImageTextButton::SetIsDefault(bool is_default) {
  if (is_default == is_default_)
    return;
  is_default_ = is_default;
  ui::Accelerator accel(ui::VKEY_RETURN, ui::EF_NONE);
  is_default_ ? AddAccelerator(accel) : RemoveAccelerator(accel);
}

void ImageTextButton::SetFont(const gfx::Font& font){
  font_=font;
}

void ImageTextButton::SetFontColor(const SkColor& color){
  color_=color;
}

void ImageTextButton::SetDisabledFontColor(const SkColor& disabled_color){
  color_disabled_=disabled_color;
}

void ImageTextButton::SetText(const string16& text ){
  text_ = text;
}

}  // namespace views
