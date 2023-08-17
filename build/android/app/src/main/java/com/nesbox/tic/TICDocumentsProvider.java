package com.nesbox.tic;

import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.graphics.Point;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract.Document;
import android.provider.DocumentsContract.Root;
import android.provider.DocumentsProvider;
import android.util.Log;
import android.webkit.MimeTypeMap;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;

/**
 * A document provider for the Storage Access Framework which exposes the files 
 * in the home (root) directory of TIC-80.
 */

public class TICDocumentsProvider extends DocumentsProvider {
    private static final String TAG = "TICDocumentProvider";
    private static final String ALL_MIME_TYPES = "*/*";
    private static final String ROOT = "root";
    private static File mBaseDir;
    
    private static final String[] DEFAULT_ROOT_PROJECTION = new String[]{
            Root.COLUMN_ROOT_ID,
            Root.COLUMN_MIME_TYPES,
            Root.COLUMN_FLAGS,
            Root.COLUMN_ICON,
            Root.COLUMN_TITLE,
            Root.COLUMN_SUMMARY,
            Root.COLUMN_DOCUMENT_ID,
            Root.COLUMN_AVAILABLE_BYTES
    };
    
    private static final String[] DEFAULT_DOCUMENT_PROJECTION = new String[]{
            Document.COLUMN_DOCUMENT_ID,
            Document.COLUMN_MIME_TYPE,
            Document.COLUMN_DISPLAY_NAME,
            Document.COLUMN_LAST_MODIFIED,
            Document.COLUMN_FLAGS,
            Document.COLUMN_SIZE
    };
    
    @Override
    public boolean onCreate()
    {
        Log.v(TAG, "onCreate");

        /* 
           The function SDL_AndroidGetExternalStoragePath() uses
           Context::getExternalFilesDir() to get the base directory of TIC-80.
           When we change it, we also should fix below.
         */
        mBaseDir = getContext().getExternalFilesDir(null);

        return true;
    }
    
    @Override
    public Cursor queryRoots(String[] projection) {
        Log.v(TAG, "queryRoots");
        
        final MatrixCursor result =
            new MatrixCursor(resolveRootProjection(projection));
        final String appName = getContext().getString(R.string.app_name);

        final MatrixCursor.RowBuilder row = result.newRow();
        row.add(Root.COLUMN_ROOT_ID, ROOT);
        row.add(Root.COLUMN_DOCUMENT_ID, getDocId(mBaseDir));
        row.add(Root.COLUMN_SUMMARY, null);
        row.add(Root.COLUMN_FLAGS,
                Root.FLAG_SUPPORTS_CREATE | Root.FLAG_SUPPORTS_IS_CHILD);
        row.add(Root.COLUMN_TITLE, appName);
        row.add(Root.COLUMN_MIME_TYPES, ALL_MIME_TYPES);
        row.add(Root.COLUMN_AVAILABLE_BYTES, mBaseDir.getFreeSpace());
        row.add(Root.COLUMN_ICON, R.mipmap.ic_launcher);
        
        return result;
    }
    
    @Override
    public Cursor queryDocument(String docId, String[] projection)
        throws FileNotFoundException {
        Log.v(TAG, "queryDocument");
        
        final MatrixCursor result =
            new MatrixCursor(resolveDocumentProjection(projection));
        includeFile(result, docId);
        
        return result;
    }
    
    @Override
    public Cursor queryChildDocuments(String parentDocId,
                                      String[] projection,
                                      String sortOrder)
        throws FileNotFoundException {
        Log.v(TAG, "queryChildDocuments");
        
        final MatrixCursor result =
            new MatrixCursor(resolveDocumentProjection(projection));
        final File parent = getFile(parentDocId);
        final File[] files = parent.listFiles();
        if (files != null)
            for (File file : files)
                includeFile(result, file);
        
        return result;
    }
    
    @Override
    public ParcelFileDescriptor openDocument(final String docId,
                                             String mode,
                                             CancellationSignal signal)
        throws FileNotFoundException {
        Log.v(TAG, "openDocument");
        
        final File file = getFile(docId);
        final int accessMode = ParcelFileDescriptor.parseMode(mode);
        return ParcelFileDescriptor.open(file, accessMode);
    }
    
    @Override
    public AssetFileDescriptor openDocumentThumbnail(String docId,
                                                     Point sizeHint,
                                                     CancellationSignal signal)
        throws FileNotFoundException {
        Log.v(TAG, "openDocumentThumbnail");
        
        final File file = getFile(docId);
        final ParcelFileDescriptor pfd =
            ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY);
        return new AssetFileDescriptor(pfd, 0, file.length());
    }
    
    @Override
    public String createDocument(String parentDocId,
                                 String mimeType,
                                 String displayName)
        throws FileNotFoundException {
        Log.v(TAG, "createDocument");
        
        File newFile = new File(parentDocId, displayName);
        int noConflictIndex = 0;
        while (newFile.exists())
            newFile = new File(parentDocId,
                               displayName + "_" + noConflictIndex++);
        final String newFilePath = newFile.getPath();
        try {
            boolean succeeded;
            if (Document.MIME_TYPE_DIR.equals(mimeType))
                succeeded = newFile.mkdir();
            else
                succeeded = newFile.createNewFile();

            if (!succeeded) {
                final String message = "Failed to create file at " + newFilePath;
                throw new FileNotFoundException(message);
            }
        } catch (IOException e) {
            final String message = "Failed to create file at " + newFilePath;
            throw new FileNotFoundException(message);
        }
        return newFilePath;
    }
    
    @Override
    public void deleteDocument(String docId)
        throws FileNotFoundException {
        Log.v(TAG, "deleteDocument");
        
        File file = getFile(docId);
        if (!file.delete()) {
            throw new FileNotFoundException("Failed to delete file at " + file.getPath());
        }
    }

    @Override
    public String getDocumentType(String docId)
        throws FileNotFoundException {
        Log.v(TAG, "getDocumentType");
        
        File file = getFile(docId);
        return getMimeType(file);
    }
    
    @Override
    public boolean isChildDocument(String parentDocId, String docId) {
        Log.v(TAG, "isChildDocument");

        final File parentFile;
        final File file;
        try {
            parentFile = getFile(parentDocId);
            file = getFile(docId);
        } catch (FileNotFoundException e) {
            Log.e(TAG, "FileNotFound in isChildDocument: " + e.getMessage());
            e.printStackTrace();
            return false;
        }
        final File realParentFile = file.getParentFile();
        return (realParentFile == null || parentFile.equals(realParentFile));
    }

    private String getDocId(File file) {
        return file.getAbsolutePath();
    }

    private File getFile(String docId)
        throws FileNotFoundException {
        final File file = new File(docId);
        if (!file.exists())
            throw new FileNotFoundException("Failed to find file at " + file.getAbsolutePath());
        return file;
    }

    private static String getMimeType(File file) {
        if (file.isDirectory()) {
            return Document.MIME_TYPE_DIR;
        } else {
            final String name = file.getName();
            final int lastDot = name.lastIndexOf('.');
            if (lastDot >= 0) {
                final String extension = name.substring(lastDot + 1).toLowerCase();
                final String mime = MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension);
                if (mime != null)
                    return mime;
            }
            return "application/octet-stream";
        }
    }

    private static String[] resolveRootProjection(String[] projection) {
        if (projection == null)
            return DEFAULT_ROOT_PROJECTION;
        else
            return projection;
    }

    private static String[] resolveDocumentProjection(String[] projection) {
        if (projection == null)
            return DEFAULT_DOCUMENT_PROJECTION;
        else
            return projection;
    }

    private void includeFile(MatrixCursor result, String docId)
            throws FileNotFoundException {
        final File file = getFile(docId);
        includeFile(result, docId, file);
    }

    private void includeFile(MatrixCursor result, File file) {
        final String docId = getDocId(file);
        includeFile(result, docId, file);
    }

    private void includeFile(MatrixCursor result, String docId, File file) {
        int flags = 0;
        
        if (file.isDirectory()) {
            if (file.canWrite())
                flags |= Document.FLAG_DIR_SUPPORTS_CREATE;
        } else if (file.canWrite()) {
            flags |= Document.FLAG_SUPPORTS_WRITE;
            flags |= Document.FLAG_SUPPORTS_DELETE;
        }
        
        final File parentFile = file.getParentFile();
        if (parentFile != null && parentFile.canWrite()) flags |= Document.FLAG_SUPPORTS_DELETE;

        final String displayName = file.getName();
        final String mimeType = getMimeType(file);
        
        if (mimeType.startsWith("image/"))
            flags |= Document.FLAG_SUPPORTS_THUMBNAIL;

        final MatrixCursor.RowBuilder row = result.newRow();
        row.add(Document.COLUMN_DOCUMENT_ID, docId);
        row.add(Document.COLUMN_DISPLAY_NAME, displayName);
        row.add(Document.COLUMN_SIZE, file.length());
        row.add(Document.COLUMN_MIME_TYPE, mimeType);
        row.add(Document.COLUMN_LAST_MODIFIED, file.lastModified());
        row.add(Document.COLUMN_FLAGS, flags);
        row.add(Document.COLUMN_ICON, R.mipmap.ic_launcher);
    }
}
