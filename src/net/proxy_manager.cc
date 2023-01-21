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

#include "net/proxy_manager.h"

#include <string>

#ifdef __APPLE__
#include <CFNetwork/CFNetwork.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include "base/mac_util.h"
#include "base/scoped_cftyperef.h"
#endif  // __APPLE__

#include "base/logging.h"
#include "base/port.h"
#include "base/singleton.h"

namespace mozc {

#ifdef __APPLE__
namespace {
// Helper functions for proxy configuration for Mac.

// Callback for CFNetworkExecuteProxyAutoConfigurationURL.  client is a
// pointer to a CFTypeRef.  This stashes either error or proxies in that
// location.
// The implementation is inspired by
// http://developer.apple.com/samplecode/CFProxySupportTool/
static void PACResultCallback(void *client, CFArrayRef proxies,
                              CFErrorRef error) {
  DCHECK((proxies == nullptr && error != nullptr) ||
         (proxies != nullptr && error == nullptr));
  CFTypeRef *result = static_cast<CFTypeRef *>(client);

  if (result != nullptr && *result == nullptr) {
    if (error != nullptr) {
      *result = CFRetain(error);
    } else {
      *result = CFRetain(proxies);
    }
  }

  CFRunLoopStop(CFRunLoopGetCurrent());
}

// If the specified proxy is PAC, it fetches the PAC file using
// CFNetworkExecuteProxyAutoConfigurationURL() and apply it to cfurl,
// and then returns the actual proxy configuration dictionary.  Otherwise,
// it just retains the specified proxy and then returns it.
// The implementation is inspired by
// http://developer.apple.com/samplecode/CFProxySupportTool/
CFDictionaryRef RetainOrExpandPacFile(CFURLRef cfurl, CFDictionaryRef proxy) {
  CFDictionaryRef final_proxy = nullptr;
  CFStringRef proxy_type = reinterpret_cast<CFStringRef>(
      CFDictionaryGetValue(proxy, kCFProxyTypeKey));

  // PAC file case.  Currently it always call
  // CFNetworkExecuteProxyAutoConfigurationURL() so it always ask to
  // the pac URL server.  However, it seems that the function seems to
  // take care of caching in memory.  So there are no additional
  // latency problems here.
  if (proxy_type != nullptr && CFGetTypeID(proxy_type) == CFStringGetTypeID() &&
      CFEqual(proxy_type, kCFProxyTypeAutoConfigurationURL)) {
    CFURLRef script_url = reinterpret_cast<CFURLRef>(
        CFDictionaryGetValue(proxy, kCFProxyAutoConfigurationURLKey));
    if (script_url != nullptr && CFGetTypeID(script_url) == CFURLGetTypeID()) {
      CFTypeRef result = nullptr;
      CFStreamClientContext context = {0, &result, nullptr, nullptr, nullptr};
      scoped_cftyperef<CFRunLoopSourceRef> runloop_source(
          CFNetworkExecuteProxyAutoConfigurationURL(
              script_url, cfurl, PACResultCallback, &context));
      const std::string label = MacUtil::GetLabelForSuffix("ProxyResolverMac");
      scoped_cftyperef<CFStringRef> private_runloop_mode(
          CFStringCreateWithBytes(nullptr,
                                  reinterpret_cast<const UInt8 *>(label.data()),
                                  label.size(), kCFStringEncodingUTF8, false));
      CFRunLoopAddSource(CFRunLoopGetCurrent(), runloop_source.get(),
                         private_runloop_mode.get());
      CFRunLoopRunInMode(private_runloop_mode.get(), 1.0e10, false);
      CFRunLoopRemoveSource(CFRunLoopGetCurrent(), runloop_source.get(),
                            private_runloop_mode.get());

      // resolving PAC succeeds
      if (result != nullptr && CFGetTypeID(result) == CFArrayGetTypeID() &&
          CFArrayGetCount(reinterpret_cast<CFArrayRef>(result)) > 0) {
        final_proxy = reinterpret_cast<CFDictionaryRef>(
            CFArrayGetValueAtIndex(reinterpret_cast<CFArrayRef>(result), 0));
        CFRetain(final_proxy);
      } else {
        LOG(WARNING) << "Failed to resolve PAC file. "
                     << "Possibly wrong PAC file is specified.";
      }
      if (result != nullptr) {
        CFRelease(result);
      }
    }
  }

  // If configuration isn't PAC or resolving PAC fails, just returns
  // the retained proxy.
  if (final_proxy == nullptr) {
    final_proxy = reinterpret_cast<CFDictionaryRef>(CFRetain(proxy));
  }

  return final_proxy;
}
}  // namespace

// MacProxyManager is a proxy manager for Mac OSX.  It uses
// CoreService API and SystemConfiguration API to obtain the current
// network configuration.
class MacProxyManager : public ProxyManagerInterface {
 public:
  bool GetProxyData(const std::string &url, std::string *hostdata,
                    std::string *authdata) {
    DCHECK(hostdata);
    DCHECK(authdata);
#ifdef OS_IOS
    scoped_cftyperef<CFDictionaryRef> proxy_settings(
        CFNetworkCopySystemProxySettings());
#else   // OS_IOS
    scoped_cftyperef<CFDictionaryRef> proxy_settings(
        SCDynamicStoreCopyProxies(nullptr));
#endif  // OS_IOS
    if (!proxy_settings.Verify(CFDictionaryGetTypeID())) {
      LOG(ERROR) << "Failed to create proxy setting";
      return false;
    }

    scoped_cftyperef<CFURLRef> cfurl(CFURLCreateWithBytes(
        nullptr, reinterpret_cast<const UInt8 *>(url.data()), url.size(),
        kCFStringEncodingUTF8, nullptr));
    if (!cfurl.Verify(CFURLGetTypeID())) {
      LOG(ERROR) << "Failed to create URL object from the specified URL";
      return false;
    }

    scoped_cftyperef<CFArrayRef> proxies(
        CFNetworkCopyProxiesForURL(cfurl.get(), proxy_settings.get()));
    if (!proxies.Verify(CFArrayGetTypeID())) {
      LOG(ERROR) << "Failed to get the proxies from the URL / proxy settings";
      return false;
    }

    bool proxy_available = false;
    if (CFArrayGetCount(proxies.get()) > 0) {
      scoped_cftyperef<CFDictionaryRef> proxy(RetainOrExpandPacFile(
          cfurl.get(), reinterpret_cast<CFDictionaryRef>(
                           CFArrayGetValueAtIndex(proxies.get(), 0))));

      CFStringRef proxy_type = static_cast<CFStringRef>(
          CFDictionaryGetValue(proxy.get(), kCFProxyTypeKey));
      if (proxy.Verify(CFDictionaryGetTypeID()) &&
          CFEqual(proxy_type, kCFProxyTypeHTTP)) {
        scoped_cftyperef<CFTypeRef> host(
            CFDictionaryGetValue(proxy.get(), kCFProxyHostNameKey), true);
        scoped_cftyperef<CFTypeRef> port(
            CFDictionaryGetValue(proxy.get(), kCFProxyPortNumberKey), true);
        scoped_cftyperef<CFTypeRef> username(
            CFDictionaryGetValue(proxy.get(), kCFProxyUsernameKey), true);
        scoped_cftyperef<CFTypeRef> password(
            CFDictionaryGetValue(proxy.get(), kCFProxyPasswordKey), true);
        if (host.Verify(CFStringGetTypeID())) {
          scoped_cftyperef<CFStringRef> host_desc;
          if (port.Verify(CFNumberGetTypeID())) {
            host_desc.reset(CFStringCreateWithFormat(
                nullptr, nullptr, CFSTR("%@:%@"), host.get(), port.get()));
          } else {
            host_desc.reset(reinterpret_cast<CFStringRef>(host.get()));
            CFRetain(host.get());
          }
          if (host_desc.Verify(CFStringGetTypeID())) {
            const char *hostdata_ptr =
                CFStringGetCStringPtr(host_desc.get(), kCFStringEncodingUTF8);
            if (hostdata_ptr != nullptr) {
              hostdata->assign(hostdata_ptr);
              proxy_available = true;
            } else {
              proxy_available = false;
            }
          } else {
            LOG(ERROR) << "Invalid proxy spec: no host is specified";
          }
        }
        if (proxy_available && username.Verify(CFStringGetTypeID()) &&
            password.Verify(CFStringGetTypeID())) {
          scoped_cftyperef<CFStringRef> auth_desc(
              CFStringCreateWithFormat(nullptr, nullptr, CFSTR("%@:%@"),
                                       username.get(), password.get()));
          if (auth_desc.Verify(CFStringGetTypeID())) {
            const char *authdata_ptr =
                CFStringGetCStringPtr(auth_desc.get(), kCFStringEncodingUTF8);
            if (authdata_ptr != nullptr) {
              authdata->assign(authdata_ptr);
            }
          }
        }
      }
    }

    return proxy_available;
  }
};
#endif  // __APPLE__

bool DummyProxyManager::GetProxyData(const std::string &url,
                                     std::string *hostdata,
                                     std::string *authdata) {
  return false;
}

namespace {

// This is only used for dependency injection
ProxyManagerInterface *g_proxy_manager = nullptr;

ProxyManagerInterface *GetProxyManager() {
  if (g_proxy_manager == nullptr) {
#ifdef __APPLE__
    return Singleton<MacProxyManager>::get();
#else   // __APPLE__
    return Singleton<DummyProxyManager>::get();
#endif  // __APPLE__
  } else {
    return g_proxy_manager;
  }
}
}  // namespace

void ProxyManager::SetProxyManager(ProxyManagerInterface *proxy_manager) {
  g_proxy_manager = proxy_manager;
}

bool ProxyManager::GetProxyData(const std::string &url, std::string *hostdata,
                                std::string *authdata) {
  return GetProxyManager()->GetProxyData(url, hostdata, authdata);
}

ProxyManagerInterface::ProxyManagerInterface() {}

ProxyManagerInterface::~ProxyManagerInterface() {}
}  // namespace mozc
