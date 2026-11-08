package com.nesbox.tic;

import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * The system picker for the `add` command: copies the chosen file into the
 * carts folder, where the studio sees it.
 */
public class TICAddActivity extends Activity {
    private static final int PICK_FILE = 1;
    private static final String USER_DIR_NAME = "TIC-80";

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);

        if (state == null) {
            final Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            startActivityForResult(intent, PICK_FILE);
        }
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);

        if (request == PICK_FILE && result == RESULT_OK && data != null && data.getData() != null)
            copyFile(data.getData());

        finish();
    }

    private void copyFile(Uri uri) {
        final File external = getExternalFilesDir(null);

        if (external == null) {
            toast("No external storage");
            return;
        }

        final File dir = new File(external, USER_DIR_NAME);
        dir.mkdirs();

        final String name = displayName(uri).replace('/', '_');
        final File file = new File(dir, name);

        if (file.exists()) {
            toast(name + " already exists");
            return;
        }

        try (InputStream in = getContentResolver().openInputStream(uri);
             OutputStream out = new FileOutputStream(file)) {
            if (in == null)
                throw new IOException("no stream");

            final byte[] buffer = new byte[8192];
            for (int read; (read = in.read(buffer)) > 0; )
                out.write(buffer, 0, read);

            toast(name + " added");
        } catch (IOException e) {
            file.delete();
            toast("Failed to add " + name);
        }
    }

    private String displayName(Uri uri) {
        try (Cursor cursor = getContentResolver().query(uri,
                new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                final int column = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (column >= 0)
                    return cursor.getString(column);
            }
        }

        return "file";
    }

    private void toast(String text) {
        Toast.makeText(this, text, Toast.LENGTH_LONG).show();
    }
}
