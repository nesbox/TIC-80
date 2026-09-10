// Dummy keyboard implementation.

// I originally wrote this keyboard module for minimal-miniscript2, a demo host
// app for MiniScript 2 which basically tried to show the bare minimum of what
// you'd need. There, this made the program build on WASI by removing the
// termios.h dependency. It does much the same here.

// This dummy module (inserted in place of cpp/core/keyboard.c) remembers your
// rawmode state (not that it does anything), and immediately ends input on a
// ReadKey press. It's only used by IOHelper, which uses it to exit raw mode
// when taking user input from the terminal.

namespace MiniScript {
namespace Keyboard {

// Unified key codes returned by ReadKeyTranslated() for the common editing
// keys, so a script sees the same value on every platform regardless of the
// native escape sequence or scan code the terminal produced. These match the
// codes used by Mini Micro. Ordinary printable characters pass through as
// their normal byte value; KEY_NONE (0) means an unrecognized multi-byte
// sequence was consumed and there is no key to report.
enum UnifiedKey {
	KEY_NONE          = 0,    // unrecognized escape/extended sequence (swallowed)
	KEY_HOME          = 1,
	KEY_END           = 5,
	KEY_BACKSPACE     = 8,
	KEY_RETURN        = 10,
	KEY_LEFT          = 17,
	KEY_RIGHT         = 18,
	KEY_UP            = 19,
	KEY_DOWN          = 20,
	KEY_PAGEUP        = 21,
	KEY_PAGEDOWN      = 22,
	KEY_FORWARD_DELETE = 127
};

static bool _rawmode = false;

// Switch the controlling terminal into raw (cbreak) mode: canonical line
// editing and local echo are disabled so individual keystrokes can be read
// without waiting for Enter and without being echoed. Signal generation
// (Ctrl-C, Ctrl-Z) and output processing are deliberately left enabled, so
// the program still behaves normally in every other respect.
//
// Safe to call repeatedly; the original mode is captured only on the first
// successful call. Returns true if raw mode is active when the call returns
// (false if stdin is not an interactive terminal, or on error).
bool EnterRawMode() {
	_rawmode = true;
	return true;
}

// Restore the terminal to the mode it had before the first EnterRawMode().
// Safe to call even if raw mode was never entered (then it does nothing).
void ExitRawMode() {
	_rawmode = false;
}

// True if raw mode is currently active.
bool InRawMode() {
	return _rawmode;
}

// Assert cooked mode: restore the user's original terminal settings so ordinary
// line input (echo, line editing) works. Typed-ahead input is preserved so it
// carries into a following read. In the last-writer-wins model the terminal
// then stays cooked until the next key operation re-enters raw mode. Idempotent;
// a no-op if raw mode was never entered. Used by the `input` intrinsic and
// before spawning child processes via `exec`.
void EnterCookedMode() {
	_rawmode = false;
}

// Equivalent of .NET Console.KeyAvailable: return true if at least one byte of
// input is waiting to be read, without blocking. Returns false if stdin is not
// an interactive terminal or on error. (Meaningful per-keystroke only while in
// raw mode; in canonical mode input is not readable until a full line.)
bool KeyAvailable() {
	return false;
}

// Equivalent of .NET Console.ReadKey(intercept:true): block until one byte of
// input is available, then return it as an unsigned value in 0..255. Returns
// -1 on end-of-input or error. In raw mode the byte is not echoed.
//
// This returns a single byte. Multi-byte input (e.g. the ESC '[' 'A' sequence
// for an arrow key, or a multi-byte UTF-8 character) arrives one byte per
// call; decoding such sequences is left to a higher layer.
int ReadKey() {
	return -1;
}

// Like ReadKey(), but assembles multi-byte editing-key sequences (arrow keys,
// Home/End, Page Up/Down, forward-delete) and returns a single platform-
// independent UnifiedKey code for them. Backspace is normalized to 8 and
// Return to 10 on every platform. Bare Escape returns 27 (decided via a short
// timeout to tell it apart from the start of a sequence). Unrecognized escape
// or extended sequences are consumed and return KEY_NONE (0). Ordinary
// characters pass through as their byte value, so case is preserved.
//
// The trade-off: in this translated mode you cannot distinguish an arrow key
// from the control character that shares its code (e.g. Left vs Ctrl-Q). Code
// needing that distinction should use ReadKey() and decode sequences itself.
int ReadKeyTranslated() {
	return KEY_NONE;
}

} // namespace Keyboard
} // namespace MiniScript
