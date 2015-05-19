// Copyright 2010-2014, Google Inc.
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

package org.mozc.android.inputmethod.japanese.preference;

import org.mozc.android.inputmethod.japanese.resources.R;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.view.KeyEvent;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import java.util.regex.Pattern;

/**
 * Mini browser to show licenses.
 *
 * <p>We must show some web sites (e.g. EULA) from preference screen.
 * However in some special environments default browser is not installed so
 * even if an Intent is issued nothing will happen.
 * This mini browser accepts an Intent and shows its content (of which URL is included as
 * Intent's data) like as a browser.
 *
 */
public class MiniBrowserActivity extends Activity {

  // TODO(matsuzakit): "print" link is meaningless. Should be invisible.
  // TODO(matsuzakit): CSS needs to be improved.

  @VisibleForTesting
  static final class MiniBrowserClient extends WebViewClient {
    private final String restrictionPattern;
    private final PackageManager packageManager;
    private final Context context;

    @VisibleForTesting
    MiniBrowserClient(String restrictionPattern, PackageManager packageManager, Context context) {
      Preconditions.checkNotNull(restrictionPattern);
      Preconditions.checkNotNull(packageManager);
      Preconditions.checkNotNull(packageManager);

      this.restrictionPattern = restrictionPattern;
      this.packageManager = packageManager;
      this.context = context;
    }

    @Override
    public boolean shouldOverrideUrlLoading(WebView view, String url) {
      Preconditions.checkNotNull(view);
      Preconditions.checkNotNull(url);

      // Use temporary matcher intentionally.
      // Regex engine is rather heavy to instantiate so use it as less as possible.
      if (!Pattern.matches(restrictionPattern, url)) {
        // If the URL's doesn't match restriction pattern,
        // delegate the operation to the default browser.
        Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        if (!packageManager.queryIntentActivities(browserIntent, 0).isEmpty()) {
          context.startActivity(browserIntent);
        }
        // If no default browser is available, do nothing.
        return true;
      }
      // Prevent from invoking default browser.
      // In some special environment default browser is not installed.
      return false;
    }
  }

  private Optional<WebView> webView = Optional.absent();

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    WebView webView = new WebView(this);
    this.webView = Optional.of(webView);
    webView.setWebViewClient(
        new MiniBrowserClient(getResources().getString(R.string.pref_url_restriction_regex),
                              getPackageManager(), this));
    webView.loadUrl(getIntent().getData().toString());
    setContentView(webView);
  }

  @Override
  public boolean onKeyDown(int keyCode, KeyEvent event) {
    Preconditions.checkNotNull(event);

    // Enable back-key inside the webview.
    // If no history exists, default behavior is executed (finish the activity).
    if (keyCode == KeyEvent.KEYCODE_BACK && webView.isPresent() && webView.get().canGoBack()) {
      webView.get().goBack();
      return true;
    }
    return super.onKeyDown(keyCode, event);
  }
}
