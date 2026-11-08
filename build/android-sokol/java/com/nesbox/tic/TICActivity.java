package com.nesbox.tic;

import android.app.NativeActivity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.ImageView;
import android.widget.PopupWindow;

/**
 * The NativeActivity and the DeX titlebar.
 *
 * In a DeX freeform window the framework lays the titlebar out but never draws
 * it: NativeActivity takes the window surface, so the native frame covers the
 * whole window. The titlebar's pixels are rasterized here and shown in a
 * click-through popup over the strip, so the real caption view keeps handling
 * the drag and the buttons.
 */
public class TICActivity extends NativeActivity {
    private PopupWindow caption;
    private View captionView;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);

        getWindow().getDecorView().post(new Runnable() {
            @Override
            public void run() {
                showCaption();
            }
        });
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
}
