package com.samsung.sprc.fileselector;

import java.io.File;

/**
 * A set of tools for file operations
 */
public class FileUtils {

	/** Filter which accepts every file */
	public static final String FILTER_ALLOW_ALL = "*.*";

	/**
	 * This method checks that the file is accepted by the filter
	 * 
	 * @param file
	 *            - file that will be checked if there is a specific type
	 * @param filter
	 *            - criterion - the file type(for example ".jpg")
	 * @return true - if file meets the criterion - false otherwise.
	 */
	public static boolean accept(final File file, final String filter) {
		if (filter.compareTo(FILTER_ALLOW_ALL) == 0) {
			return true;
		}
		if (file.isDirectory()) {
			return true;
		}
		int lastIndexOfPoint = file.getName().lastIndexOf('.');
		if (lastIndexOfPoint == -1) {
			return false;
		}
		String fileType = file.getName().substring(lastIndexOfPoint).toLowerCase();
		return fileType.compareTo(filter) == 0;
	}
}
