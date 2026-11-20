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

#pragma once

#include "tic80_types.h"

// The member's account without the studio: the pairing state machine, the
// token, and the injectable inputs the filesystem's shelf mount reads. The
// module takes the filesystem and the network as arguments and includes no
// studio.h.

typedef struct tic_account tic_account;
struct tic_fs;
struct tic_net;

// What a login reports: the URL and the code to show, the member's name, or
// one of the final answers. text may carry the server's message for an error.
typedef enum
{
    account_pairing_code,
    account_pairing_approved,
    account_pairing_denied,
    account_pairing_expired,
    account_pairing_error,
} account_pairing_state;

typedef void (*account_pairing_callback)(account_pairing_state state, const char* text, void* data);

// Creates the account over the filesystem and the network and attaches it to
// the filesystem's remote inputs. The token file is read here, with no
// request.
tic_account* tic_account_create(struct tic_fs* fs, struct tic_net* net);
void tic_account_close(tic_account* account);

bool tic_account_signedin(tic_account* account);
const char* tic_account_user(tic_account* account);
const char* tic_account_token(tic_account* account);

// Drives the pairing's poll schedule: the host calls it every tick with the
// elapsed milliseconds. Signed in or idle, it does nothing.
void tic_account_update(tic_account* account, u32 deltaMs);

// Starts a pairing. An already signed-in account answers approved with the
// member's name. False means a pairing is already in progress.
bool tic_account_login(tic_account* account, account_pairing_callback callback, void* data);

// Stops a pairing in progress; no further answer is delivered.
void tic_account_cancel_login(tic_account* account);

// Reports whether a pairing is running — the console watches it for ESC.
bool tic_account_login_active(tic_account* account);

// Clears the local token at once and asks the server to revoke it; the
// request is best-effort.
void tic_account_logout(tic_account* account);

// A shelf request answered 401: the session is over, so the token goes.
void tic_account_unauthorized(tic_account* account);
