package com.nesbox.tic;

import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.util.Log;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

import org.libsdl.app.SDLActivity;

public class TIC extends SDLActivity
{
    private static String sGameToLaunch = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Check for a file intent before starting the SDLActivity's native code.
        Intent intent = getIntent();
        Uri uri = intent.getData();

        if (uri != null && Intent.ACTION_VIEW.equals(intent.getAction())) {
            sGameToLaunch = copyUriToCache(uri);
        }

        super.onCreate(savedInstanceState);
    }

    private String copyUriToCache(Uri uri) {
        String fileName = "cart.tic"; // A fallback filename.
        // Get the original filename from the URI's metadata.
        try (Cursor cursor = getContentResolver().query(uri, new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (nameIndex > -1) {
                    fileName = cursor.getString(nameIndex);
                }
            }
        }

        try (InputStream inputStream = getContentResolver().openInputStream(uri)) {
            if (inputStream == null) {
                Log.e("TIC", "Could not open input stream for URI: " + uri);
                return null;
            }

            // Create a file in the app's cache directory using the original filename.
            File tempFile = new File(getCacheDir(), fileName);

            // Copy the content from the URI to the temporary file.
            try (FileOutputStream outputStream = new FileOutputStream(tempFile)) {
                byte[] buffer = new byte[4096];
                int bytesRead;
                while ((bytesRead = inputStream.read(buffer)) != -1) {
                    outputStream.write(buffer, 0, bytesRead);
                }
            }

            String filePath = tempFile.getAbsolutePath();
            Log.i("TIC", "Copied content URI to cache file: " + filePath);
            return filePath;

        } catch (Exception e) {
            Log.e("TIC", "Error copying URI to cache", e);
            return null;
        }
    }

    @Override
    protected String[] getArguments() {
        if (sGameToLaunch != null) {
            String[] args = { sGameToLaunch };
            sGameToLaunch = null; // Clear it after use so it's not reused.
            return args;
        }
        return new String[0];
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "tic80" };
    }
}
