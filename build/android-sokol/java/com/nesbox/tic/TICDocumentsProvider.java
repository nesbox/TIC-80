package com.nesbox.tic;

import android.content.pm.ApplicationInfo;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.graphics.Point;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract.Document;
import android.provider.DocumentsContract.Root;
import android.provider.DocumentsProvider;
import android.webkit.MimeTypeMap;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Locale;

/**
 * The carts folder as a Storage Access Framework root: the system Files app
 * can copy, move and share carts from here.
 */
public class TICDocumentsProvider extends DocumentsProvider {
    private static final String ROOT = "root";
    private static final String ALL_MIME_TYPES = "*/*";
    private static final String USER_DIR_NAME = "TIC-80";

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

    private File mBaseDir;

    @Override
    public boolean onCreate() {
        // The studio's folder, made here so the root is there before it runs.
        final File external = getContext().getExternalFilesDir(null);

        if (external != null) {
            mBaseDir = new File(external, USER_DIR_NAME);
            mBaseDir.mkdirs();
        }

        return mBaseDir != null && mBaseDir.isDirectory();
    }

    @Override
    public Cursor queryRoots(String[] projection) {
        final MatrixCursor result = new MatrixCursor(resolveRootProjection(projection));

        if (mBaseDir == null)
            return result;

        final ApplicationInfo info = getContext().getApplicationInfo();

        final MatrixCursor.RowBuilder row = result.newRow();
        row.add(Root.COLUMN_ROOT_ID, ROOT);
        row.add(Root.COLUMN_DOCUMENT_ID, mBaseDir.getAbsolutePath());
        row.add(Root.COLUMN_SUMMARY, null);
        row.add(Root.COLUMN_FLAGS, Root.FLAG_SUPPORTS_CREATE | Root.FLAG_SUPPORTS_IS_CHILD);
        row.add(Root.COLUMN_TITLE, info.loadLabel(getContext().getPackageManager()).toString());
        row.add(Root.COLUMN_MIME_TYPES, ALL_MIME_TYPES);
        row.add(Root.COLUMN_AVAILABLE_BYTES, mBaseDir.getFreeSpace());
        row.add(Root.COLUMN_ICON, info.icon);

        return result;
    }

    @Override
    public Cursor queryDocument(String docId, String[] projection)
            throws FileNotFoundException {
        final MatrixCursor result = new MatrixCursor(resolveDocumentProjection(projection));
        includeFile(result, getFile(docId));
        return result;
    }

    @Override
    public Cursor queryChildDocuments(String parentDocId, String[] projection, String sortOrder)
            throws FileNotFoundException {
        final MatrixCursor result = new MatrixCursor(resolveDocumentProjection(projection));
        final File[] files = getFile(parentDocId).listFiles();
        if (files != null)
            for (File file : files)
                includeFile(result, file);
        return result;
    }

    @Override
    public ParcelFileDescriptor openDocument(String docId, String mode, CancellationSignal signal)
            throws FileNotFoundException {
        return ParcelFileDescriptor.open(getFile(docId), ParcelFileDescriptor.parseMode(mode));
    }

    @Override
    public AssetFileDescriptor openDocumentThumbnail(String docId, Point sizeHint,
                                                    CancellationSignal signal)
            throws FileNotFoundException {
        final File file = getFile(docId);
        final ParcelFileDescriptor pfd =
                ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY);
        return new AssetFileDescriptor(pfd, 0, file.length());
    }

    @Override
    public String createDocument(String parentDocId, String mimeType, String displayName)
            throws FileNotFoundException {
        final File parent = getFile(parentDocId);
        final File file = uniqueFile(parent, displayName);

        if (!isInside(file))
            throw new FileNotFoundException("Outside the app folder: " + displayName);

        try {
            final boolean made = Document.MIME_TYPE_DIR.equals(mimeType)
                    ? file.mkdir()
                    : file.createNewFile();
            if (!made)
                throw new FileNotFoundException("Failed to create " + file.getPath());
        } catch (IOException e) {
            throw new FileNotFoundException("Failed to create " + file.getPath());
        }

        return file.getAbsolutePath();
    }

    @Override
    public void deleteDocument(String docId) throws FileNotFoundException {
        final File file = getFile(docId);

        if (isBase(file) || !deleteFile(file))
            throw new FileNotFoundException("Failed to delete " + file.getPath());
    }

    @Override
    public String getDocumentType(String docId) throws FileNotFoundException {
        return getMimeType(getFile(docId));
    }

    // A SAF tree grant names any document under the tree, not just a direct
    // child, and the Files app reads this before copying a folder into itself.
    @Override
    public boolean isChildDocument(String parentDocId, String docId) {
        final File parent = new File(parentDocId);
        final File child = new File(docId);

        if (!isInside(parent) || !isInside(child))
            return false;

        try {
            return child.getCanonicalPath().startsWith(parent.getCanonicalPath() + File.separator);
        } catch (IOException e) {
            return false;
        }
    }

    private boolean isBase(File file) {
        try {
            return file.getCanonicalPath().equals(mBaseDir.getCanonicalPath());
        } catch (IOException e) {
            return false;
        }
    }

    // A doc id is a path, and a granted URI may name any document: only the
    // carts folder and what is under it are served.
    private boolean isInside(File file) {
        if (mBaseDir == null)
            return false;

        try {
            final String base = mBaseDir.getCanonicalPath();
            final String path = file.getCanonicalPath();
            return path.equals(base) || path.startsWith(base + File.separator);
        } catch (IOException e) {
            return false;
        }
    }

    private File getFile(String docId) throws FileNotFoundException {
        final File file = new File(docId);

        if (!isInside(file) || !file.exists())
            throw new FileNotFoundException("Missing " + docId);

        return file;
    }

    private boolean deleteFile(File file) {
        if (file.isDirectory()) {
            final File[] children = file.listFiles();
            if (children != null)
                for (File child : children)
                    // A symlink inside the folder could point anywhere.
                    if (!isInside(child) || !deleteFile(child))
                        return false;
        }

        return file.delete();
    }

    // "name (1).ext", the name a desktop gives a conflicting file.
    private static File uniqueFile(File parent, String name) {
        File file = new File(parent, name);

        if (!file.exists())
            return file;

        final int dot = name.lastIndexOf('.');
        final String base = dot > 0 ? name.substring(0, dot) : name;
        final String ext = dot > 0 ? name.substring(dot) : "";

        for (int i = 1; ; i++) {
            file = new File(parent, base + " (" + i + ")" + ext);
            if (!file.exists())
                return file;
        }
    }

    private void includeFile(MatrixCursor result, File file) {
        int flags = 0;

        if (file.isDirectory()) {
            if (file.canWrite())
                flags |= Document.FLAG_DIR_SUPPORTS_CREATE;
        } else if (file.canWrite()) {
            flags |= Document.FLAG_SUPPORTS_WRITE;
            flags |= Document.FLAG_SUPPORTS_DELETE;
        }

        final File parent = file.getParentFile();
        // The root's parent is writable, but the root itself cannot be deleted.
        if (parent != null && parent.canWrite() && !isBase(file))
            flags |= Document.FLAG_SUPPORTS_DELETE;

        final String mimeType = getMimeType(file);
        if (mimeType.startsWith("image/"))
            flags |= Document.FLAG_SUPPORTS_THUMBNAIL;

        final MatrixCursor.RowBuilder row = result.newRow();
        row.add(Document.COLUMN_DOCUMENT_ID, file.getAbsolutePath());
        row.add(Document.COLUMN_DISPLAY_NAME, file.getName());
        row.add(Document.COLUMN_SIZE, file.length());
        row.add(Document.COLUMN_MIME_TYPE, mimeType);
        row.add(Document.COLUMN_LAST_MODIFIED, file.lastModified());
        row.add(Document.COLUMN_FLAGS, flags);
    }

    private static String getMimeType(File file) {
        if (file.isDirectory())
            return Document.MIME_TYPE_DIR;

        final String name = file.getName();
        final int dot = name.lastIndexOf('.');
        if (dot >= 0) {
            final String extension = name.substring(dot + 1).toLowerCase(Locale.US);
            final String mime = MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension);
            if (mime != null)
                return mime;
        }

        return "application/octet-stream";
    }

    private static String[] resolveRootProjection(String[] projection) {
        return projection != null ? projection : DEFAULT_ROOT_PROJECTION;
    }

    private static String[] resolveDocumentProjection(String[] projection) {
        return projection != null ? projection : DEFAULT_DOCUMENT_PROJECTION;
    }
}
