package com.nesbox.tic;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * The carts folder and the files going in and out of it: the system picker
 * hands a content URI, Downloads is where a file leaves the app.
 */
public class TICFiles {
    private static final String FOLDER = "TIC-80";

    public static File folder(Context context) {
        final File external = context.getExternalFilesDir(null);

        if (external == null)
            return null;

        final File dir = new File(external, FOLDER);
        dir.mkdirs();
        return dir;
    }

    public static String displayName(Context context, Uri uri) {
        try (Cursor cursor = context.getContentResolver().query(uri,
                new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                final int column = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);

                // A provider may answer with no name at all.
                if (column >= 0) {
                    final String name = cursor.getString(column);
                    if (name != null)
                        return name.replace('/', '_');
                }
            }
        } catch (RuntimeException e) {
            // No read grant or a broken provider: the URI still has a name.
        }

        // A file:// URI has no such column; its last segment is the name.
        final String name = uri.getLastPathSegment();
        return name != null ? name.replace('/', '_') : "file";
    }

    // "name (1).ext", the name a desktop gives a conflicting file.
    public static File unique(File dir, String name) {
        File file = new File(dir, name);

        if (!file.exists())
            return file;

        final int dot = name.lastIndexOf('.');
        final String base = dot > 0 ? name.substring(0, dot) : name;
        final String ext = dot > 0 ? name.substring(dot) : "";

        for (int i = 1; ; i++) {
            file = new File(dir, base + " (" + i + ")" + ext);
            if (!file.exists())
                return file;
        }
    }

    public static boolean copy(Context context, Uri uri, File file) {
        try (InputStream in = context.getContentResolver().openInputStream(uri);
             OutputStream out = new FileOutputStream(file)) {
            if (in == null)
                throw new IOException("no stream");

            final byte[] buffer = new byte[8192];
            for (int read; (read = in.read(buffer)) > 0; )
                out.write(buffer, 0, read);

            return true;
        } catch (IOException | RuntimeException e) {
            file.delete();
            return false;
        }
    }

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

        Uri uri = null;

        try {
            uri = resolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values);

            if (uri == null)
                return false;

            try (OutputStream out = resolver.openOutputStream(uri)) {
                if (out == null)
                    throw new IOException("no stream");
                out.write(data);
            }

            // The file is complete: let the file managers see it.
            values.clear();
            values.put(MediaStore.MediaColumns.IS_PENDING, 0);

            if (resolver.update(uri, values, null, null) <= 0)
                throw new IOException("not published");

            return true;
        } catch (IOException | RuntimeException e) {
            // A pending row would hide the broken file from every manager.
            if (uri != null)
                resolver.delete(uri, null, null);

            return false;
        }
    }
}
