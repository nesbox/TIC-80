package com.nesbox.tic;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ContentResolver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.provider.MediaStore;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class TICFileReceiver extends Activity {
    private final static String TAG = "TICFileReceiver";
    private final static String UserDirName = "TIC-80";
    private static File UserDirectory;

    @Override
    protected void onStart()
    {
        super.onStart();
        Log.v(TAG, "onStart");

        /*
           The function SDL_AndroidGetExternalStoragePath() uses
           Context::getExternalFilesDir() to get the base directory of TIC-80.
           When we change it, we also should fix below.
         */
        final File baseDir = getApplicationContext().getExternalFilesDir(null);
        UserDirectory = new File(baseDir, UserDirName);

        final Intent intent = getIntent();
        if (intent == null) return;
        final String action = intent.getAction();
        final String type = intent.getType();

        if (action.equals(Intent.ACTION_SEND) && type != null) {
            Log.v(TAG, "received intent of type " + type);
            Uri fileUri = intent.getParcelableExtra(Intent.EXTRA_STREAM);
            if (fileUri != null) {
                DialogInterface.OnClickListener listener = new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        TICFileReceiver.this.finish();
                    }
                };
                final String fileName = SaveFile(fileUri);
                AlertDialog.Builder builder = new AlertDialog.Builder(this)
                        .setTitle("TIC-80")
                        .setPositiveButton("OK", listener);
                if (fileName != null)
                    builder.setMessage("File " + fileName + " is saved to user home directory.");
                else
                    builder.setMessage("Failed to save file.");
                builder.create().show();
            }
        }
    }

    private String SaveFile(Uri uri) {
        final Context context = getApplicationContext();
        final String scheme = uri.getScheme();

        String path = null;
        if ("file".equals(scheme)) {
            path = uri.getPath();
        } else if("content".equals(scheme)) {
            ContentResolver contentResolver = context.getContentResolver();
            Cursor cursor = contentResolver.query(uri, new String[] { MediaStore.MediaColumns.DATA }, null, null, null);
            if (cursor != null) {
                cursor.moveToFirst();
                path = cursor.getString(0);
                cursor.close();
            }
        }
        if (path == null) return null;
        final File srcFile = new File(path);

        final String baseFileName = srcFile.getName();
        File dstFile = new File(UserDirectory, baseFileName);
        int noConflictIndex = 1;
        while (dstFile.exists())
            dstFile = new File(UserDirectory, baseFileName + "_" + noConflictIndex++);
        if (Copy(uri, dstFile))
            return dstFile.getName();
        else
            return null;
    }

    private boolean Copy(Uri src, File dst) {
        final int CopyBufferLength = 1024;
        ContentResolver resolver = getApplicationContext().getContentResolver();
        try (InputStream in = resolver.openInputStream(src)) {
            try (OutputStream out = new FileOutputStream(dst)) {
                // Transfer bytes from in to out
                byte[] buf = new byte[CopyBufferLength];
                int len;
                while ((len = in.read(buf)) > 0) {
                    out.write(buf, 0, len);
                }
            }
        } catch (IOException e) {
            Log.e(TAG, "Copying file failed. \n" + e.getMessage());
            return false;
        }
        Log.v(TAG, "Saved file to " + dst.getPath());
        return true;
    }
}
