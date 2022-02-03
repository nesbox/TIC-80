package com.nesbox.tic;

import org.libsdl.app.SDLActivity;

public class TIC extends SDLActivity
{
	@Override
	protected String[] getLibraries() {
		return new String[] { "tic80" };
	}
}
