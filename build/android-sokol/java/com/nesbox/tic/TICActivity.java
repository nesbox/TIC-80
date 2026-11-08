package com.nesbox.tic;

import android.app.NativeActivity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.view.Gravity;
import android.view.View;
import android.widget.ImageView;
import android.widget.PopupWindow;

import java.io.File;
import java.util.Locale;

/**
 * The NativeActivity plus the one cart a launch can bring: a .tic opened from
 * a file manager is loaded once, at startup.
 *
 * In a DeX freeform window the framework lays the titlebar out but never draws
 * it: NativeActivity takes the window surface, so the native frame covers the
 * whole window. The titlebar's pixels are rasterized here and shown in a
 * click-through popup over the strip, so the real caption view keeps handling
 * the drag and the buttons.
 */
public class TICActivity extends NativeActivity {
    private static final String LAUNCH_CART = "launchCart";

    // Taken by sokol_main, before the studio exists.
    private static String launchCart;

    // The same path with the instance's lifetime, to survive process death.
    private String savedCart;

    private PopupWindow caption;
    private View captionView;

    @Override
    protected void onCreate(Bundle state) {
        // The system redelivers the VIEW intent when the task is restored:
        // the cart was imported already, and a second copy would be made.
        if (state != null) {
            savedCart = state.getString(LAUNCH_CART);
            launchCart = savedCart;
        } else
            savedCart = launchCart = importCart(getIntent());

        super.onCreate(state);

        getWindow().getDecorView().post(new Runnable() {
            @Override
            public void run() {
                showCaption();
            }
        });
    }

    @Override
    protected void onSaveInstanceState(Bundle state) {
        super.onSaveInstanceState(state);
        state.putString(LAUNCH_CART, savedCart);
    }

    @Override
    protected void onDestroy() {
        hideCaption();
        super.onDestroy();
    }

    @Override
    public void onWindowFocusChanged(boolean focus) {
        super.onWindowFocusChanged(focus);

        // The caption's shade follows the window's focus.
        if (caption != null)
            getWindow().getDecorView().post(new Runnable() {
                @Override
                public void run() {
                    showCaption();
                }
            });
    }

    private void showCaption() {
        final View decor = getWindow().getDecorView();

        // android:id/caption is a hidden framework id.
        final int id = getResources().getIdentifier("caption", "id", "android");
        final View view = id == 0 ? null : decor.findViewById(id);

        if (view == null)
            return;

        if (view != captionView) {
            captionView = view;
            view.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
                @Override
                public void onLayoutChange(View v, int l, int t, int r, int b, int ol, int ot, int or, int ob) {
                    if (caption == null || v.getWidth() != caption.getWidth() || v.getHeight() != caption.getHeight())
                        v.post(new Runnable() {
                            @Override
                            public void run() {
                                showCaption();
                            }
                        });
                }
            });
        }

        // Not laid out yet; the listener above runs once it is.
        if (view.getWidth() == 0 || view.getHeight() == 0)
            return;

        if (isFinishing() || !decor.isAttachedToWindow())
            return;

        final Bitmap bitmap = Bitmap.createBitmap(view.getWidth(), view.getHeight(), Bitmap.Config.ARGB_8888);
        view.draw(new Canvas(bitmap));

        final ImageView image = new ImageView(this);
        image.setImageBitmap(bitmap);

        hideCaption();

        // A panel window above the app's one, in the caption's place; it takes
        // no touches, so the drag and the buttons reach the real view.
        final PopupWindow popup = new PopupWindow(image, view.getWidth(), view.getHeight());
        popup.setTouchable(false);
        popup.setFocusable(false);
        popup.setClippingEnabled(false);
        // No frame and no shadow: the bitmap sits exactly on the strip.
        popup.setBackgroundDrawable(null);
        popup.setElevation(0);
        popup.showAtLocation(decor, Gravity.LEFT | Gravity.TOP, view.getLeft(), view.getTop());
        caption = popup;
    }

    private void hideCaption() {
        if (caption != null) {
            caption.dismiss();
            caption = null;
        }
    }

    // The launch cart, once.
    static String takeLaunchCart() {
        final String path = launchCart;
        launchCart = null;
        return path;
    }

    private String importCart(Intent intent) {
        if (intent == null || !Intent.ACTION_VIEW.equals(intent.getAction()))
            return null;

        final Uri uri = intent.getData();

        if (uri == null)
            return null;

        try {
            // The app's own root serves real paths: open the file in place.
            if ((getPackageName() + ".documents").equals(uri.getAuthority()))
                return cartInFolder(DocumentsContract.getDocumentId(uri));

            final String name = TICFiles.displayName(this, uri);

            if (!name.toLowerCase(Locale.US).endsWith(".tic"))
                return null;

            final File dir = TICFiles.folder(this);

            if (dir == null)
                return null;

            final File file = TICFiles.unique(dir, name);

            return TICFiles.copy(this, uri, file) ? file.getAbsolutePath() : null;
        } catch (RuntimeException e) {
            // An unreadable or malformed URI is not a reason to crash.
            return null;
        }
    }

    // A path this app may load: inside the carts folder, and a cart.
    private String cartInFolder(String path) {
        final File dir = TICFiles.folder(this);

        if (dir == null || path == null || !path.toLowerCase(Locale.US).endsWith(".tic"))
            return null;

        try {
            final String folder = dir.getCanonicalPath();
            final String file = new File(path).getCanonicalPath();

            return file.startsWith(folder + File.separator) ? path : null;
        } catch (Exception e) {
            return null;
        }
    }
}
