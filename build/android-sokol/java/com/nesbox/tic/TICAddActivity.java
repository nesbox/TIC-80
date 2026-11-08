package com.nesbox.tic;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.widget.Toast;

import java.io.File;

/**
 * The system picker for the `add` command: copies the chosen file into the
 * carts folder, where the studio sees it.
 */
public class TICAddActivity extends Activity {
    private static final int PICK_FILE = 1;

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
        final File dir = TICFiles.folder(this);

        if (dir == null) {
            toast("No external storage");
            return;
        }

        final String name = TICFiles.displayName(this, uri);
        final File file = new File(dir, name);

        if (file.exists()) {
            toast(name + " already exists");
            return;
        }

        toast(TICFiles.copy(this, uri, file) ? name + " added" : "Failed to add " + name);
    }

    private void toast(String text) {
        Toast.makeText(this, text, Toast.LENGTH_LONG).show();
    }
}
