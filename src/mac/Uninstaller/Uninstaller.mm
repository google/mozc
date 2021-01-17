// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#import "Uninstaller.h"

#include <Security/Security.h>

#import "DialogsController.h"

#include <string>

#include "base/mac_util.h"
#include "base/url.h"
#include "base/version.h"

namespace {

NSURL *kUninstallSurveyUrl = nil;
NSString *kUninstallerScriptPath = nil;

bool GetPrevilegeRights(AuthorizationRef *auth) {
  OSStatus status;
  AuthorizationFlags authFlags = kAuthorizationFlagDefaults;

  status = AuthorizationCreate(
      nullptr, kAuthorizationEmptyEnvironment, authFlags, auth);
  if (status != errAuthorizationSuccess) {
    return status;
  }

  AuthorizationItem *newItem = new AuthorizationItem();
  if (!newItem) {
    return false;
  }

  AuthorizationItem item = {kAuthorizationRightExecute, 0, nullptr, 0};
  AuthorizationRights rights = {1, &item};

  authFlags = kAuthorizationFlagDefaults |
      kAuthorizationFlagInteractionAllowed |
      kAuthorizationFlagPreAuthorize |
      kAuthorizationFlagExtendRights;
  status = AuthorizationCopyRights(*auth, &rights, nullptr, authFlags, nullptr);

  return status == errAuthorizationSuccess;
}

bool OpenUninstallSurvey() {
  return [[NSWorkspace sharedWorkspace] openURL:kUninstallSurveyUrl];
}

bool DeleteFiles(const AuthorizationRef &auth) {
  const char *kRmPath = "/bin/rm";
  const char *rmArgs[] = {"-rf", nullptr, nullptr};
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  const char *kRemovePaths[] = {
    "/Library/Input Methods/GoogleJapaneseInput.app",
    "/Library/LaunchAgents/com.google.inputmethod.Japanese.Converter.plist",
    "/Library/LaunchAgents/com.google.inputmethod.Japanese.Renderer.plist",
    "/Applications/GoogleJapaneseInput.localized",
    nullptr};
#else  // GOOGLE_JAPANESE_INPUT_BUILD
  const char *kRemovePaths[] = {
    "/Library/Input Methods/Mozc.app",
    "/Library/LaunchAgents/org.mozc.inputmethod.Japanese.Converter.plist",
    "/Library/LaunchAgents/org.mozc.inputmethod.Japanese.Renderer.plist",
    "/Applications/Mozc",
    nullptr};
#endif // GOOGLE_JAPANESE_INPUT_BUILD
  for (int i = 0; kRemovePaths[i] != nullptr; ++i) {
    rmArgs[1] = kRemovePaths[i];
    OSStatus status = AuthorizationExecuteWithPrivileges(
        auth, kRmPath, kAuthorizationFlagDefaults,
        const_cast<char * const *>(rmArgs), nullptr);
    if (status != errAuthorizationSuccess) {
      NSLog(@"Failed to remove path: %s", kRemovePaths[i]);
      return false;
    }
  }
  return true;
}

bool UnregisterKeystoneTicket(const AuthorizationRef &auth) {
  const char *kKsadminPath = "/Library/Google/GoogleSoftwareUpdate/"
      "GoogleSoftwareUpdate.bundle/Contents/MacOS/ksadmin";
  const char *kKsadminArgs[] =
      {"--delete", "--productid", "com.google.JapaneseIME", nullptr};
  OSStatus status = AuthorizationExecuteWithPrivileges(
      auth, kKsadminPath, kAuthorizationFlagDefaults,
      const_cast<char * const *>(kKsadminArgs), nullptr);
  return status == errAuthorizationSuccess;
}

bool RunReboot(const AuthorizationRef &auth) {
  // TODO(mukai): Use OS-specific API instead of calling reboot command.
  const char *rebootPath = "/sbin/reboot";
  char * args[] = {nullptr};
  OSStatus status = AuthorizationExecuteWithPrivileges(
      auth, rebootPath, kAuthorizationFlagDefaults, args, nullptr);
  return status == errAuthorizationSuccess;
}
}  // namespace

@implementation Uninstaller

+ (void)doUninstall:(DialogsController *)dialogs {
  mozc::MacUtil::RemovePrelauncherLoginItem();

  AuthorizationRef auth;

  if (!GetPrevilegeRights(&auth)) {
    [dialogs reportAuthError];
    AuthorizationFree(auth, kAuthorizationFlagDefaults);
    return;
  }
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  if (OpenUninstallSurvey() &&
      DeleteFiles(auth) && UnregisterKeystoneTicket(auth)) {
#else  // GOOGLE_JAPANESE_INPUT_BUILD
  if (DeleteFiles(auth)) {
#endif // GOOGLE_JAPANESE_INPUT_BUILD
    if ([dialogs reportUninstallSuccess]) {
      RunReboot(auth);
    }
  } else {
    [dialogs reportUninstallError];
  }

  AuthorizationFree(auth, kAuthorizationFlagDefaults);
}

+ (void)initializeUninstaller {
  string url;
  mozc::URL::GetUninstallationSurveyURL(mozc::Version::GetMozcVersion(), &url);
  NSString *uninstallUrl = [[NSString alloc] initWithBytes:url.data()
                                                    length:url.size()
                                                  encoding:NSUTF8StringEncoding];
  kUninstallSurveyUrl = [NSURL URLWithString:uninstallUrl];

  kUninstallerScriptPath =
      [[NSBundle mainBundle] pathForResource:@"uninstaller"
                                      ofType:@"py"
                                 inDirectory:nil];
}

@end
