// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSION_HOST_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSION_HOST_DELEGATE_H_

#include "base/macros.h"
#include "extensions/browser/extension_host_delegate.h"

namespace extensions {

// A minimal ExtensionHostDelegate.
class ShellExtensionHostDelegate : public ExtensionHostDelegate {
 public:
  ShellExtensionHostDelegate();
  virtual ~ShellExtensionHostDelegate();

  // ExtensionHostDelegate implementation.
  virtual void OnExtensionHostCreated(
      content::WebContents* web_contents) OVERRIDE;
  virtual void OnRenderViewCreatedForBackgroundPage(
      ExtensionHost* host) OVERRIDE;
  virtual content::JavaScriptDialogManager* GetJavaScriptDialogManager()
      OVERRIDE;
  virtual void CreateTab(content::WebContents* web_contents,
                         const std::string& extension_id,
                         WindowOpenDisposition disposition,
                         const gfx::Rect& initial_pos,
                         bool user_gesture) OVERRIDE;
  virtual void ProcessMediaAccessRequest(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback,
      const Extension* extension) OVERRIDE;
  virtual bool CheckMediaAccessPermission(content::WebContents* web_contents,
                                          const GURL& security_origin,
                                          content::MediaStreamType type,
                                          const Extension* extension) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellExtensionHostDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSION_HOST_DELEGATE_H_
