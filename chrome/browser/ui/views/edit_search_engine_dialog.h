// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// EditSearchEngineDialog provides text fields for editing a keyword: the title,
// url and actual keyword. It is used by the KeywordEditorView of the Options
// dialog, and also on its own to confirm the addition of a keyword added by
// the ExternalJSObject via the RenderView.

#ifndef CHROME_BROWSER_UI_VIEWS_EDIT_SEARCH_ENGINE_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_EDIT_SEARCH_ENGINE_DIALOG_H_

#include "base/macros.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
class ImageView;
}

class EditSearchEngineController;
class EditSearchEngineControllerDelegate;
class Profile;
class TemplateURL;

class EditSearchEngineDialog : public views::TextfieldController,
                               public views::DialogDelegateView {
 public:
  // The |template_url| and/or |delegate| may be NULL.
  EditSearchEngineDialog(TemplateURL* template_url,
                         EditSearchEngineControllerDelegate* delegate,
                         Profile* profile);
  ~EditSearchEngineDialog() override;

  // Shows the dialog to the user.
  static void Show(gfx::NativeWindow parent,
                   TemplateURL* template_url,
                   EditSearchEngineControllerDelegate* delegate,
                   Profile* profile);

  // views::DialogDelegate:
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  bool Cancel() override;
  bool Accept() override;

  // views::TextfieldController:
  // Updates whether the user can accept the dialog as well as updating image
  // views showing whether value is valid.
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

 private:
  void Init();

  // Create a Label containing the text with the specified message id.
  views::Label* CreateLabel(int message_id);

  // Creates a text field with the specified text.
  views::Textfield* CreateTextfield(const base::string16& text);

  // Invokes UpdateImageView for each of the images views.
  void UpdateImageViews();

  // Updates the tooltip and image of the image view based on is_valid. If
  // is_valid is false the tooltip of the image view is set to the message with
  // id invalid_message_id, otherwise the tooltip is set to the empty text.
  void UpdateImageView(views::ImageView* image_view,
                       bool is_valid,
                       int invalid_message_id);

  // Text fields.
  views::Textfield* title_tf_;
  views::Textfield* keyword_tf_;
  views::Textfield* url_tf_;

  // Shows error images.
  views::ImageView* title_iv_;
  views::ImageView* keyword_iv_;
  views::ImageView* url_iv_;

  std::unique_ptr<EditSearchEngineController> controller_;

  DISALLOW_COPY_AND_ASSIGN(EditSearchEngineDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EDIT_SEARCH_ENGINE_DIALOG_H_
