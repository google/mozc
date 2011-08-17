// Copyright 2010-2011, Google Inc.
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

// The custom actions created for installing the IME.

#ifndef MOZC_WIN32_CUSTOM_ACTION_CUSTOM_ACTION_H_
#define MOZC_WIN32_CUSTOM_ACTION_CUSTOM_ACTION_H_

#include <windows.h>
#include <msi.h>

// Makes ieuser.exe update the cache of the elevation policies.
UINT __stdcall RefreshPolicy(MSIHANDLE msi_handle);

// Install Mozc as an IME.
UINT __stdcall InstallIME(MSIHANDLE msi_handle);

// A rollback custom action for RegisterServer.
UINT __stdcall InstallIMERollback(MSIHANDLE msi_handle);

// Unregister Mozc from IMEs.
UINT __stdcall UninstallIME(MSIHANDLE msi_handle);

// A rollback custom action for UninstallIME.
UINT __stdcall UninstallIMERollback(MSIHANDLE msi_handle);

// Opens the uninstall survey page with the default browser.
UINT __stdcall OpenUninstallSurveyPage(MSIHANDLE msi_handle);

// Shuts down mozc_server.exe and mozc_renderer.exe to remove their files.
UINT __stdcall ShutdownServer(MSIHANDLE msi_handle);

// Restores the IME environment for the current user.  See the comment in
// uninstall_helper.h for details.
UINT __stdcall RestoreUserIMEEnvironment(MSIHANDLE msi_handle);

// Ensures that Mozc is disabled for the service account.  See the comment in
// uninstall_helper.h for details.
UINT __stdcall EnsureIMEIsDisabledForServiceAccount(MSIHANDLE msi_handle);

// Hides the cancel button on a progress dialog shown by the installer.
UINT __stdcall HideCancelButton(MSIHANDLE msi_handle);

// Check the installation condition and write the result code to the registry
// for Omaha if any error occurs.
UINT __stdcall InitialInstallation(MSIHANDLE msi_handle);

// Write the success code to the registry for Omaha.
UINT __stdcall InitialInstallationCommit(MSIHANDLE msi_handle);

// Saves omaha's ap value for WriteApValue, WriteApValueRollback, and
// RestoreServiceState.
// Since they are executed as deferred customs actions and most properties
// cannot be accessible from a deferred custom action, it is necessary to store
// these data explictly to CustomCationData.
UINT __stdcall SaveCustomActionData(MSIHANDLE msi_handle);

// Restore the settings of the cache service as it was.
// This custom action may start the cache service if needed.
UINT __stdcall RestoreServiceState(MSIHANDLE msi_handle);

// Ensure the cache service stopped. You have to stop the cache service before
// you replace the GoogleIMEJaCacheService.exe.
UINT __stdcall StopCacheService(MSIHANDLE msi_handle);

// Writes omaha's ap value for changing the user's channel.
UINT __stdcall WriteApValue(MSIHANDLE msi_handle);

// A rollback custom action for WriteApValue.
UINT __stdcall WriteApValueRollback(MSIHANDLE msi_handle);

#ifndef NO_LOGGING
// Disables Windows Error Reporting.
UINT __stdcall DisableErrorReporting(MSIHANDLE msi_handle);

// Test function for rollback test.
UINT __stdcall Failure(MSIHANDLE msi_handle);
#endif

#endif  // MOZC_WIN32_CUSTOM_ACTION_CUSTOM_ACTION_H_
