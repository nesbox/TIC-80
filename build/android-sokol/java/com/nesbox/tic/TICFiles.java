package com.nesbox.tic;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;

import java.io.IOException;
import java.io.OutputStream;

/**
 * A file leaving the app: Downloads is the one place every file manager
 * shows, and MediaStore puts it there without a permission.
 */
public class TICFiles {
    private static final String FOLDER = "TIC-80";

    public static boolean saveToDownloads(Context context, String name, byte[] data) {
        // MediaStore.Downloads is API 29; before it the app folder is all
        // there is, and the caller says so.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q)
            return false;

        final ContentResolver resolver = context.getContentResolver();
        final ContentValues values = new ContentValues();
        values.put(MediaStore.MediaColumns.DISPLAY_NAME, name);
        values.put(MediaStore.MediaColumns.MIME_TYPE, "application/octet-stream");
        values.put(MediaStore.MediaColumns.RELATIVE_PATH,
                Environment.DIRECTORY_DOWNLOADS + "/" + FOLDER);
        values.put(MediaStore.MediaColumns.IS_PENDING, 1);

        final Uri uri = resolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values);

        if (uri == null)
            return false;

        try (OutputStream out = resolver.openOutputStream(uri)) {
            if (out == null)
                throw new IOException("no stream");
            out.write(data);
        } catch (IOException e) {
            resolver.delete(uri, null, null);
            return false;
        }

        // The file is complete: let the file managers see it.
        values.clear();
        values.put(MediaStore.MediaColumns.IS_PENDING, 0);
        resolver.update(uri, values, null, null);

        return true;
    }
}
