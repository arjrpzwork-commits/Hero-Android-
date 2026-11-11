package com.webapp.herostudios;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.webkit.ConsoleMessage;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceError;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.webkit.JavascriptInterface;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.io.IOException;

public final class MainActivity extends Activity {
    private static final int FILE_PICKER_REQUEST = 4107;
    private static final String EDITOR_URL = "file:///android_asset/editor/index.html";
    private WebView webView;
    private ValueCallback<Uri[]> filePathCallback;
    private View errorView;
    private HeroNativeRuntime nativeRuntime;

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        nativeRuntime = new HeroNativeRuntime(this);
        if (!assetExists("editor/index.html")) {
            showStartupError("The bundled Hero editor is missing from this app package. Rebuild using the complete Android project ZIP.");
            return;
        }
        openEditor();
    }

    private boolean assetExists(String path) {
        try (java.io.InputStream stream = getAssets().open(path)) {
            return stream.read() >= 0;
        } catch (IOException error) {
            return false;
        }
    }

    private void openEditor() {
        errorView = null;
        webView = new WebView(this);
        setContentView(webView);
        WebSettings settings = webView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setAllowFileAccess(true);
        settings.setAllowContentAccess(true);
        settings.setMediaPlaybackRequiresUserGesture(true);
        settings.setDatabaseEnabled(false);
        settings.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);
        settings.setAllowFileAccessFromFileURLs(false);
        settings.setAllowUniversalAccessFromFileURLs(false);
        webView.addJavascriptInterface(nativeRuntime, "HeroRuntime");
        webView.setWebViewClient(new WebViewClient() {
            @Override public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                Uri uri = request.getUrl();
                String value = uri.toString();
                // Keep the editor self-contained. Permit its bundled assets and in-document URLs only.
                return !("file".equalsIgnoreCase(uri.getScheme()) && value.startsWith("file:///android_asset/"))
                        && !"about".equalsIgnoreCase(uri.getScheme())
                        && !"data".equalsIgnoreCase(uri.getScheme())
                        && !"blob".equalsIgnoreCase(uri.getScheme());
            }

            @Override public void onReceivedError(WebView view, WebResourceRequest request, WebResourceError error) {
                if (request.isForMainFrame()) {
                    String detail = error == null ? "Unknown WebView startup error" : error.getDescription().toString();
                    showStartupError("Hero could not open its bundled editor. " + detail + "\n\nThis app is designed to start locally; it does not require a website.");
                }
            }
        });
        webView.setWebChromeClient(new WebChromeClient() {
            @Override public boolean onConsoleMessage(ConsoleMessage message) {
                android.util.Log.d("HeroVidStudios", message.message() + " (" + message.sourceId() + ":" + message.lineNumber() + ")");
                return true;
            }

            @Override public boolean onShowFileChooser(WebView view, ValueCallback<Uri[]> callback,
                    FileChooserParams params) {
                if (filePathCallback != null) filePathCallback.onReceiveValue(null);
                filePathCallback = callback;
                Intent intent = params.createIntent();
                try { startActivityForResult(intent, FILE_PICKER_REQUEST); }
                catch (Exception error) {
                    filePathCallback = null;
                    callback.onReceiveValue(null);
                    showStartupError("Android could not open the media picker: " + error.getMessage());
                    return false;
                }
                return true;
            }
        });
        webView.loadUrl(EDITOR_URL);
    }

    private void showStartupError(String message) {
        if (webView != null) {
            webView.stopLoading();
            webView.destroy();
            webView = null;
        }
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER);
        layout.setPadding(36, 36, 36, 36);
        layout.setBackgroundColor(Color.rgb(12, 18, 29));
        TextView title = new TextView(this);
        title.setText("HERO VID STUDIOS");
        title.setTextColor(Color.WHITE);
        title.setTextSize(22);
        title.setGravity(Gravity.CENTER);
        TextView detail = new TextView(this);
        detail.setText(message);
        detail.setTextColor(Color.LTGRAY);
        detail.setTextSize(15);
        detail.setGravity(Gravity.CENTER);
        detail.setPadding(0, 24, 0, 24);
        Button retry = new Button(this);
        retry.setText("Retry opening editor");
        retry.setOnClickListener(v -> openEditor());
        layout.addView(title);
        layout.addView(detail);
        layout.addView(retry);
        errorView = layout;
        setContentView(errorView);
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == FILE_PICKER_REQUEST && filePathCallback != null) {
            Uri[] results = WebChromeClient.FileChooserParams.parseResult(resultCode, data);
            filePathCallback.onReceiveValue(results);
            filePathCallback = null;
        }
    }

    @Override public void onBackPressed() {
        if (webView != null && webView.canGoBack()) webView.goBack();
        else super.onBackPressed();
    }

    @Override protected void onDestroy() {
        if (filePathCallback != null) filePathCallback.onReceiveValue(null);
        if (webView != null) { webView.stopLoading(); webView.destroy(); webView = null; }
        if (nativeRuntime != null) nativeRuntime.close();
        super.onDestroy();
    }
}
