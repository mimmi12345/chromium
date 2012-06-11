// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_MODAL_CONFIRM_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_MODAL_CONFIRM_DIALOG_VIEWS_H_
#pragma once

#include "ui/gfx/native_widget_types.h"
#include "ui/views/window/dialog_delegate.h"

class TabContents;
class TabModalConfirmDialogDelegate;

namespace views {
class MessageBoxView;
}

// Displays a tab-modal dialog, i.e. a dialog that will block the current page
// but still allow the user to switch to a different page.
// To display the dialog, allocate this object on the heap. It will open the
// dialog from its constructor and then delete itself when the user dismisses
// the dialog.
class TabModalConfirmDialogViews : public views::DialogDelegate {
 public:
  TabModalConfirmDialogViews(TabModalConfirmDialogDelegate* delegate,
                             TabContents* tab_contents);

  // views::DialogDelegate:
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;

 private:
  virtual ~TabModalConfirmDialogViews();

  scoped_ptr<TabModalConfirmDialogDelegate> delegate_;

  // The message box view whose commands we handle.
  views::MessageBoxView* message_box_view_;

  DISALLOW_COPY_AND_ASSIGN(TabModalConfirmDialogViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_MODAL_CONFIRM_DIALOG_VIEWS_H_
