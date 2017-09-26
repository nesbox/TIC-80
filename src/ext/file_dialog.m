// MIT License

// Copyright (c) 2017 Vadim Grigoruk @nesbox // grigoruk@gmail.com

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "file_dialog.h"

#include <AppKit/AppKit.h>

bool file_dialog_load_path(char* buffer)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSWindow *keyWindow = [[NSApplication sharedApplication] keyWindow];    
	NSOpenPanel *dialog = [NSOpenPanel openPanel];
	[dialog setAllowsMultipleSelection:NO];

	bool success = false;
	if ( [dialog runModal] == NSModalResponseOK )
	{
		NSURL *url = [dialog URL];
		const char *utf8Path = [[url path] UTF8String];

		strcpy( buffer, utf8Path);

		success = true;
	}

	[pool release];
	[keyWindow makeKeyAndOrderFront:nil];

	return success;
}

bool file_dialog_save_path(const char* name, char* buffer)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSWindow *keyWindow = [[NSApplication sharedApplication] keyWindow]; 
	NSSavePanel *dialog = [NSSavePanel savePanel];
	[dialog setExtensionHidden:NO];

	NSString *nameString = [NSString stringWithUTF8String: name];
	[dialog setNameFieldStringValue:nameString];

	bool success = false;
	if ( [dialog runModal] == NSModalResponseOK )
	{
		NSURL *url = [dialog URL];
		const char *utf8Path = [[url path] UTF8String];

		strcpy( buffer, utf8Path);

		success = true;
	}

	[pool release];
	[keyWindow makeKeyAndOrderFront:nil];

	return success;
}
