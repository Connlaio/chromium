// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_UI_MESSAGE_HANDLER_H_
#define CONTENT_PUBLIC_BROWSER_WEB_UI_MESSAGE_HANDLER_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_ui.h"

class GURL;
class WebUIBrowserTest;

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace content {

class WebUI;
class WebUIImpl;

// Messages sent from the DOM are forwarded via the WebUI to handler
// classes. These objects are owned by WebUI and destroyed when the
// host is destroyed.
class CONTENT_EXPORT WebUIMessageHandler {
 public:
  WebUIMessageHandler() : javascript_allowed_(false), web_ui_(nullptr) {}
  virtual ~WebUIMessageHandler() {}

 protected:
  FRIEND_TEST_ALL_PREFIXES(WebUIMessageHandlerTest, ExtractIntegerValue);
  FRIEND_TEST_ALL_PREFIXES(WebUIMessageHandlerTest, ExtractDoubleValue);
  FRIEND_TEST_ALL_PREFIXES(WebUIMessageHandlerTest, ExtractStringValue);

  // Subclasses must call this once the page is ready for JavaScript calls
  // from this handler.
  void AllowJavascript();

  bool IsJavascriptAllowed() const;

  // Helper methods:

  // Extract an integer value from a list Value.
  static bool ExtractIntegerValue(const base::ListValue* value, int* out_int);

  // Extract a floating point (double) value from a list Value.
  static bool ExtractDoubleValue(const base::ListValue* value,
                                 double* out_value);

  // Extract a string value from a list Value.
  static base::string16 ExtractStringValue(const base::ListValue* value);

  // This is where subclasses specify which messages they'd like to handle and
  // perform any additional initialization.. At this point web_ui() will return
  // the associated WebUI object.
  virtual void RegisterMessages() = 0;

  // Will be called whenever JavaScript from this handler becomes allowed from
  // the disallowed state. Subclasses should override this method to register
  // observers that push JavaScript calls to the page.
  virtual void OnJavascriptAllowed() {}

  // Will be called whenever JavaScript from this handler becomes disallowed
  // from the allowed state. This will never be called before
  // OnJavascriptAllowed has been called. Subclasses should override this method
  // to deregister or disabled observers that push JavaScript calls to the page.
  virtual void OnJavascriptDisallowed() {}

  // Call a Javascript function by sending its name and arguments down to
  // the renderer.  This is asynchronous; there's no way to get the result
  // of the call, and should be thought of more like sending a message to
  // the page.
  // All function names in WebUI must consist of only ASCII characters.
  // These functions will crash if JavaScript is not currently allowed.
  template <typename... Values>
  void CallJavascriptFunction(const std::string& function_name,
                              const Values&... values) {
    CHECK(IsJavascriptAllowed()) << "Cannot CallJavascriptFunction before "
                                    "explicitly allowing JavaScript.";
    web_ui()->CallJavascriptFunction(function_name, values...);
  }

  // Returns the attached WebUI for this handler.
  WebUI* web_ui() const { return web_ui_; }

  // Sets the attached WebUI - exposed to subclasses for testing purposes.
  void set_web_ui(WebUI* web_ui) { web_ui_ = web_ui; }

 private:
  // Provide external classes access to web_ui(), set_web_ui(), and
  // RenderViewReused.
  friend class WebUIImpl;
  friend class ::WebUIBrowserTest;

  // Called when a RenderView is reused to display a page (i.e. reload).
  void RenderViewReused();

  // True if the page is for JavaScript calls from this handler.
  bool javascript_allowed_;

  WebUI* web_ui_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_UI_MESSAGE_HANDLER_H_
