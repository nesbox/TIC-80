package com.nesbox.tic;

import android.app.NativeActivity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import java.io.File;

/**
 * The NativeActivity plus the one cart a launch can bring: a .tic opened from
 * a file manager is copied into the folder and loaded once, at startup.
 */
public class TICActivity extends NativeActivity {
    // Taken by sokol_main, before the studio exists: the copy happens before
    // the native side starts.
    private static String launchCart;

    @Override
    protected void onCreate(Bundle state) {
        launchCart = importCart(getIntent());
        super.onCreate(state);
    }

    // The launch cart, once.
    static String takeLaunchCart() {
        final String cart = launchCart;
        launchCart = null;
        return cart;
    }

    private String importCart(Intent intent) {
        if (intent == null || !Intent.ACTION_VIEW.equals(intent.getAction()))
            return null;

        final Uri uri = intent.getData();
        final File dir = uri == null ? null : TICFiles.folder(this);

        if (dir == null)
            return null;

        final File file = TICFiles.unique(dir, TICFiles.displayName(this, uri));

        return TICFiles.copy(this, uri, file) ? file.getAbsolutePath() : null;
    }
}
