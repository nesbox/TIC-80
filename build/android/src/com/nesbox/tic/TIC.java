package com.nesbox.tic;

import com.samsung.sprc.fileselector.FileSelector;
import com.samsung.sprc.fileselector.FileOperation;
import com.samsung.sprc.fileselector.OnHandleFileListener;

import org.libsdl.app.SDLActivity;

public class TIC extends SDLActivity
{
	@Override
	protected String[] getLibraries() {
		return new String[] {
			"SDL2",
			"sdlgpu",
			"lua",
			"wren",
			"z",
			"gif",
			"main"
		};
	}

	protected final String[] fileSelectorResult = new String[]{""};

	public String saveFile(final String name)
	{
		fileSelectorResult[0] = "";

		runOnUiThread(new Runnable() 
		{
			@Override
			public void run() 
			{
				new FileSelector(TIC.this, FileOperation.SAVE, mSaveFileListener, name).show();
			}
		});

		synchronized (fileSelectorResult) 
		{
			try 
			{
				fileSelectorResult.wait();
			} 
			catch (InterruptedException ex) 
			{
				ex.printStackTrace();
			}
		}

		return fileSelectorResult[0];
	}

	OnHandleFileListener mSaveFileListener = new OnHandleFileListener() 
	{
		@Override
		public void handleFile(final String filePath) 
		{
			fileSelectorResult[0] = filePath;

			synchronized (fileSelectorResult) 
			{
				fileSelectorResult.notify();
			}
		}

		@Override
		public void handleCancel()
		{
			fileSelectorResult[0] = "";

			synchronized (fileSelectorResult) 
			{
				fileSelectorResult.notify();
			}
		}
	};

	public String loadFile()
	{
		fileSelectorResult[0] = "";

		runOnUiThread(new Runnable() 
		{
			@Override
			public void run() 
			{
				new FileSelector(TIC.this, FileOperation.LOAD, mLoadFileListener, "").show();
			}
		});

		synchronized (fileSelectorResult) 
		{
			try 
			{
				fileSelectorResult.wait();
			} 
			catch (InterruptedException ex) 
			{
				ex.printStackTrace();
			}
		}

		return fileSelectorResult[0];
	}

	OnHandleFileListener mLoadFileListener = new OnHandleFileListener() 
	{
		@Override
		public void handleFile(final String filePath) 
		{
			fileSelectorResult[0] = filePath;

			synchronized (fileSelectorResult) 
			{
				fileSelectorResult.notify();
			}
		}

		@Override
		public void handleCancel()
		{
			fileSelectorResult[0] = "";

			synchronized (fileSelectorResult) 
			{
				fileSelectorResult.notify();
			}
		}
	};
}