// Copyright 2010-2018, Google Inc.
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

package org.mozc.android.inputmethod.japanese.testing;

import android.content.ComponentName;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.FeatureInfo;
import android.content.pm.InstrumentationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageInstaller;
import android.content.pm.PackageManager;
import android.content.pm.PermissionGroupInfo;
import android.content.pm.PermissionInfo;
import android.content.pm.ProviderInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.content.res.Resources;
import android.content.res.XmlResourceParser;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.UserHandle;

import java.util.List;

/**
 * @see MockContext
 */
public class MockPackageManager extends PackageManager {

  @Deprecated
  @Override
  public void addPackageToPreferred(String arg0) {
  }

  @Override
  public boolean addPermission(PermissionInfo arg0) {
    return false;
  }

  @Override
  public boolean addPermissionAsync(PermissionInfo arg0) {
    return false;
  }

  @Deprecated
  @Override
  public void addPreferredActivity(IntentFilter arg0, int arg1,
      ComponentName[] arg2, ComponentName arg3) {

  }

  @Override
  public String[] canonicalToCurrentPackageNames(String[] arg0) {
    return null;
  }

  @Override
  public int checkPermission(String arg0, String arg1) {
    return 0;
  }

  @Override
  public int checkSignatures(String arg0, String arg1) {
    return 0;
  }

  @Override
  public int checkSignatures(int arg0, int arg1) {
    return 0;
  }

  @Override
  public void clearPackagePreferredActivities(String arg0) {
  }

  @Override
  public String[] currentToCanonicalPackageNames(String[] arg0) {
    return null;
  }

  @Override
  public void extendVerificationTimeout(int arg0, int arg1, long arg2) {
  }

  @Override
  public Drawable getActivityIcon(ComponentName arg0)
      throws NameNotFoundException {
    return null;
  }

  @Override
  public Drawable getActivityIcon(Intent arg0) throws NameNotFoundException {
    return null;
  }

  @Override
  public ActivityInfo getActivityInfo(ComponentName arg0, int arg1)
      throws NameNotFoundException {
    return null;
  }

  @Override
  public Drawable getActivityLogo(ComponentName arg0)
      throws NameNotFoundException {
    return null;
  }

  @Override
  public Drawable getActivityLogo(Intent arg0) throws NameNotFoundException {
    return null;
  }

  @Override
  public List<PermissionGroupInfo> getAllPermissionGroups(int arg0) {
    return null;
  }

  @Override
  public int getApplicationEnabledSetting(String arg0) {
    return 0;
  }

  @Override
  public Drawable getApplicationIcon(ApplicationInfo arg0) {
    return null;
  }

  @Override
  public Drawable getApplicationIcon(String arg0) throws NameNotFoundException {
    return null;
  }

  @Override
  public ApplicationInfo getApplicationInfo(String arg0, int arg1)
      throws NameNotFoundException {
    return null;
  }

  @Override
  public CharSequence getApplicationLabel(ApplicationInfo arg0) {
    return null;
  }

  @Override
  public Drawable getApplicationLogo(ApplicationInfo arg0) {
    return null;
  }

  @Override
  public Drawable getApplicationLogo(String arg0) throws NameNotFoundException {
    return null;
  }

  @Override
  public int getComponentEnabledSetting(ComponentName arg0) {
    return 0;
  }

  @Override
  public Drawable getDefaultActivityIcon() {
    return null;
  }

  @Override
  public Drawable getDrawable(String arg0, int arg1, ApplicationInfo arg2) {
    return null;
  }

  @Override
  public List<ApplicationInfo> getInstalledApplications(int arg0) {
    return null;
  }

  @Override
  public List<PackageInfo> getInstalledPackages(int arg0) {
    return null;
  }

  @Override
  public String getInstallerPackageName(String arg0) {
    return null;
  }

  @Override
  public InstrumentationInfo getInstrumentationInfo(ComponentName arg0, int arg1)
      throws NameNotFoundException {
    return null;
  }

  @Override
  public Intent getLaunchIntentForPackage(String arg0) {
    return null;
  }

  @Override
  public String getNameForUid(int arg0) {
    return null;
  }

  @Override
  public int[] getPackageGids(String arg0) throws NameNotFoundException {
    return null;
  }

  @Override
  public PackageInfo getPackageInfo(String arg0, int arg1)
      throws NameNotFoundException {
    return null;
  }

  @Override
  public String[] getPackagesForUid(int arg0) {
    return null;
  }

  @Override
  public List<PackageInfo> getPackagesHoldingPermissions(String[] arg0,
      int arg1) {
    return null;
  }

  @Override
  public PermissionGroupInfo getPermissionGroupInfo(String arg0, int arg1)
      throws NameNotFoundException {
    return null;
  }

  @Override
  public PermissionInfo getPermissionInfo(String arg0, int arg1)
      throws NameNotFoundException {
    return null;
  }

  @Override
  public int getPreferredActivities(List<IntentFilter> arg0,
      List<ComponentName> arg1, String arg2) {
    return 0;
  }

  @Override
  public List<PackageInfo> getPreferredPackages(int arg0) {
    return null;
  }

  @Override
  public ProviderInfo getProviderInfo(ComponentName arg0, int arg1)
      throws NameNotFoundException {
    return null;
  }

  @Override
  public ActivityInfo getReceiverInfo(ComponentName arg0, int arg1)
      throws NameNotFoundException {
    return null;
  }

  @Override
  public Resources getResourcesForActivity(ComponentName arg0)
      throws NameNotFoundException {
    return null;
  }

  @Override
  public Resources getResourcesForApplication(ApplicationInfo arg0)
      throws NameNotFoundException {
    return null;
  }

  @Override
  public Resources getResourcesForApplication(String arg0)
      throws NameNotFoundException {
    return null;
  }

  @Override
  public ServiceInfo getServiceInfo(ComponentName arg0, int arg1)
      throws NameNotFoundException {
    return null;
  }

  @Override
  public FeatureInfo[] getSystemAvailableFeatures() {
    return null;
  }

  @Override
  public String[] getSystemSharedLibraryNames() {
    return null;
  }

  @Override
  public CharSequence getText(String arg0, int arg1, ApplicationInfo arg2) {
    return null;
  }

  @Override
  public XmlResourceParser getXml(String arg0, int arg1, ApplicationInfo arg2) {
    return null;
  }

  @Override
  public boolean hasSystemFeature(String arg0) {
    return false;
  }

  @Override
  public boolean isSafeMode() {
    return false;
  }

  @Override
  public List<ResolveInfo> queryBroadcastReceivers(Intent arg0, int arg1) {
    return null;
  }

  @Override
  public List<ProviderInfo> queryContentProviders(String arg0, int arg1, int arg2) {
    return null;
  }

  @Override
  public List<InstrumentationInfo> queryInstrumentation(String arg0, int arg1) {
    return null;
  }

  @Override
  public List<ResolveInfo> queryIntentActivities(Intent arg0, int arg1) {
    return null;
  }

  @Override
  public List<ResolveInfo> queryIntentActivityOptions(ComponentName arg0,
      Intent[] arg1, Intent arg2, int arg3) {
    return null;
  }

  @Override
  public List<ResolveInfo> queryIntentContentProviders(Intent arg0, int arg1) {
    return null;
  }

  @Override
  public List<ResolveInfo> queryIntentServices(Intent arg0, int arg1) {
    return null;
  }

  @Override
  public List<PermissionInfo> queryPermissionsByGroup(String arg0, int arg1)
      throws NameNotFoundException {
    return null;
  }

  @Deprecated
  @Override
  public void removePackageFromPreferred(String arg0) {
  }

  @Override
  public void removePermission(String arg0) {
  }

  @Override
  public ResolveInfo resolveActivity(Intent arg0, int arg1) {
    return null;
  }

  @Override
  public ProviderInfo resolveContentProvider(String arg0, int arg1) {
    return null;
  }

  @Override
  public ResolveInfo resolveService(Intent arg0, int arg1) {
    return null;
  }

  @Override
  public void setApplicationEnabledSetting(String arg0, int arg1, int arg2) {
  }

  @Override
  public void setComponentEnabledSetting(ComponentName arg0, int arg1, int arg2) {
  }

  @Override
  public void setInstallerPackageName(String arg0, String arg1) {
  }

  @Override
  public void verifyPendingInstall(int arg0, int arg1) {
  }

  @Override
  public Drawable getApplicationBanner(ApplicationInfo info) {
    return null;
  }

  @Override
  public Drawable getApplicationBanner(String packageName) throws NameNotFoundException {
    return null;
  }

  @Override
  public Drawable getActivityBanner(ComponentName activityName) throws NameNotFoundException {
    return null;
  }

  @Override
  public Drawable getActivityBanner(Intent intent) throws NameNotFoundException {
    return null;
  }

  @Override
  public Intent getLeanbackLaunchIntentForPackage(String packageName) {
    return null;
  }

  @Override
  public PackageInstaller getPackageInstaller() {
    return null;
  }

  @Override
  public Drawable getUserBadgedDrawableForDensity(Drawable arg0, UserHandle arg1, Rect arg2,
      int arg3) {
    return null;
  }

  @Override
  public Drawable getUserBadgedIcon(Drawable arg0, UserHandle arg1) {
    return null;
  }

  @Override
  public CharSequence getUserBadgedLabel(CharSequence arg0, UserHandle arg1) {
    return null;
  }

  // Overriding hidden public abstract method.
  @SuppressWarnings("unused")
  public Drawable getUserBadgeForDensity(UserHandle user, int density) {
    return null;
  }
}
