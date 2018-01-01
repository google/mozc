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

package org.mozc.android.inputmethod.japanese.accessibility;

import com.google.common.base.Preconditions;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.content.Context;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityManager.AccessibilityStateChangeListener;
import android.view.accessibility.AccessibilityManager.TouchExplorationStateChangeListener;

import java.util.Collections;
import java.util.List;

/**
 * Wrapper for {@link AccessibilityManager}.
 *
 * <p>This class is introduced for testing.
 * Raw {@link AccessibilityManager} cannot be overridden so mock-technique cannot be applied.
 * Overridden instance should be registered into {@link AccessibilityUtil} for testing.
 */
class AccessibilityManagerWrapper {

  private interface ManagerProxy {
    boolean addAccessibilityStateChangeListener(AccessibilityStateChangeListener listener);
    boolean addTouchExplorationStateChangeListener(
        AccessibilityManager.TouchExplorationStateChangeListener listener);
    List<ServiceInfo> getAccessibilityServiceList();
    List<AccessibilityServiceInfo> getEnabledAccessibilityServiceList(int feedbackTypeFlags);
    List<AccessibilityServiceInfo> getInstalledAccessibilityServiceList();
    void interrupt();
    boolean isEnabled();
    boolean isTouchExplorationEnabled();
    boolean removeAccessibilityStateChangeListener(
        AccessibilityManager.AccessibilityStateChangeListener listener);
    boolean removeTouchExplorationStateChangeListener(
        AccessibilityManager.TouchExplorationStateChangeListener listener);
    void sendAccessibilityEvent(AccessibilityEvent event);
  }

  private static class ManagerLessThan14 implements ManagerProxy {

    protected final AccessibilityManager manager;

    ManagerLessThan14(AccessibilityManager manager) {
      this.manager = Preconditions.checkNotNull(manager);
    }

    @Override
    public boolean addAccessibilityStateChangeListener(AccessibilityStateChangeListener listener) {
      return false;
    }

    @Override
    public boolean addTouchExplorationStateChangeListener(
        TouchExplorationStateChangeListener listener) {
      return false;
    }

    @Override
    public List<AccessibilityServiceInfo> getEnabledAccessibilityServiceList(
        int feedbackTypeFlags) {
      return Collections.emptyList();
    }

    @Override
    public List<AccessibilityServiceInfo> getInstalledAccessibilityServiceList() {
      return Collections.emptyList();
    }

    @Override
    public boolean isTouchExplorationEnabled() {
      return false;
    }

    @Override
    public boolean removeAccessibilityStateChangeListener(
        AccessibilityStateChangeListener listener) {
      return false;
    }

    @Override
    public boolean removeTouchExplorationStateChangeListener(
        TouchExplorationStateChangeListener listener) {
      return false;
    }

    @SuppressWarnings("deprecation")
    @Override
    public List<ServiceInfo> getAccessibilityServiceList() {
      return manager.getAccessibilityServiceList();
    }

    @Override
    public void interrupt() {
      manager.interrupt();
    }

    @Override
    public boolean isEnabled() {
      return manager.isEnabled();
    }

    @Override
    public void sendAccessibilityEvent(AccessibilityEvent event) {
      manager.sendAccessibilityEvent(event);
    }
  }

  private static class ManagerFrom14To18 extends ManagerLessThan14 {

    ManagerFrom14To18(AccessibilityManager manager) {
      super(manager);
    }

    @Override
    public boolean addAccessibilityStateChangeListener(AccessibilityStateChangeListener listener) {
      return manager.addAccessibilityStateChangeListener(listener);
    }

    @Override
    public List<AccessibilityServiceInfo> getEnabledAccessibilityServiceList(
        int feedbackTypeFlags) {
      return manager.getEnabledAccessibilityServiceList(feedbackTypeFlags);
    }

    @Override
    public List<AccessibilityServiceInfo> getInstalledAccessibilityServiceList() {
      return manager.getInstalledAccessibilityServiceList();
    }

    @Override
    public boolean isTouchExplorationEnabled() {
      return manager.isTouchExplorationEnabled();
    }

    @Override
    public boolean removeAccessibilityStateChangeListener(
        AccessibilityStateChangeListener listener) {
      return manager.removeAccessibilityStateChangeListener(listener);
    }
  }

  private static class ManagerFrom19 extends ManagerFrom14To18 {

    ManagerFrom19(AccessibilityManager manager) {
      super(manager);
    }

    @Override
    public boolean addTouchExplorationStateChangeListener(
        AccessibilityManager.TouchExplorationStateChangeListener listener) {
      return manager.addTouchExplorationStateChangeListener(listener);
    }

    @Override
    public boolean removeTouchExplorationStateChangeListener(
        AccessibilityManager.TouchExplorationStateChangeListener listener) {
      return manager.removeTouchExplorationStateChangeListener(listener);
    }
  }

  private final ManagerProxy managerProxy;

  AccessibilityManagerWrapper(Context context) {
    AccessibilityManager manager = AccessibilityManager.class.cast(
        Preconditions.checkNotNull(context)
            .getSystemService(Context.ACCESSIBILITY_SERVICE));
    if (Build.VERSION.SDK_INT < 14) {
      managerProxy = new ManagerLessThan14(manager);
    } else if (Build.VERSION.SDK_INT < 19) {
      managerProxy = new ManagerFrom14To18(manager);
    } else {
      managerProxy = new ManagerFrom19(manager);
    }
  }

  public boolean isEnabled() {
    return managerProxy.isEnabled();
  }

  public boolean isTouchExplorationEnabled() {
    return managerProxy.isTouchExplorationEnabled();
  }

  public void sendAccessibilityEvent(AccessibilityEvent event) {
    managerProxy.sendAccessibilityEvent(event);
  }

  public void interrupt() {
    managerProxy.interrupt();
  }

  @Deprecated
  public List<ServiceInfo> getAccessibilityServiceList() {
    return managerProxy.getAccessibilityServiceList();
  }

  public List<AccessibilityServiceInfo> getInstalledAccessibilityServiceList() {
    return managerProxy.getInstalledAccessibilityServiceList();
  }

  public List<AccessibilityServiceInfo> getEnabledAccessibilityServiceList(
      int feedbackTypeFlags) {
    return managerProxy.getEnabledAccessibilityServiceList(feedbackTypeFlags);
  }

  public boolean addAccessibilityStateChangeListener(AccessibilityStateChangeListener listener) {
    return managerProxy.addAccessibilityStateChangeListener(listener);
  }

  public boolean removeAccessibilityStateChangeListener(
      AccessibilityStateChangeListener listener) {
    return managerProxy.removeAccessibilityStateChangeListener(listener);
  }
}