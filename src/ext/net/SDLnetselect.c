/*
  SDL_net:  An example cross-platform network library for use with SDL
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/* $Id$ */

#include "SDLnetsys.h"
#include "SDL_net.h"

/* The select() API for network sockets */

struct SDLNet_Socket {
    int ready;
    SOCKET channel;
};

struct _SDLNet_SocketSet {
    int numsockets;
    int maxsockets;
    struct SDLNet_Socket **sockets;
};

/* Allocate a socket set for use with SDLNet_CheckSockets()
   This returns a socket set for up to 'maxsockets' sockets, or NULL if
   the function ran out of memory.
 */
SDLNet_SocketSet SDLNet_AllocSocketSet(int maxsockets)
{
    struct _SDLNet_SocketSet *set;
    int i;

    set = (struct _SDLNet_SocketSet *)SDL_malloc(sizeof(*set));
    if ( set != NULL ) {
        set->numsockets = 0;
        set->maxsockets = maxsockets;
        set->sockets = (struct SDLNet_Socket **)SDL_malloc
                    (maxsockets*sizeof(*set->sockets));
        if ( set->sockets != NULL ) {
            for ( i=0; i<maxsockets; ++i ) {
                set->sockets[i] = NULL;
            }
        } else {
            SDL_free(set);
            set = NULL;
        }
    }
    return(set);
}

/* Add a socket to a set of sockets to be checked for available data */
int SDLNet_AddSocket(SDLNet_SocketSet set, SDLNet_GenericSocket sock)
{
    if ( sock != NULL ) {
        if ( set->numsockets == set->maxsockets ) {
            SDLNet_SetError("socketset is full");
            return(-1);
        }
        set->sockets[set->numsockets++] = (struct SDLNet_Socket *)sock;
    }
    return(set->numsockets);
}

/* Remove a socket from a set of sockets to be checked for available data */
int SDLNet_DelSocket(SDLNet_SocketSet set, SDLNet_GenericSocket sock)
{
    int i;

    if ( sock != NULL ) {
        for ( i=0; i<set->numsockets; ++i ) {
            if ( set->sockets[i] == (struct SDLNet_Socket *)sock ) {
                break;
            }
        }
        if ( i == set->numsockets ) {
            SDLNet_SetError("socket not found in socketset");
            return(-1);
        }
        --set->numsockets;
        for ( ; i<set->numsockets; ++i ) {
            set->sockets[i] = set->sockets[i+1];
        }
    }
    return(set->numsockets);
}

/* This function checks to see if data is available for reading on the
   given set of sockets.  If 'timeout' is 0, it performs a quick poll,
   otherwise the function returns when either data is available for
   reading, or the timeout in milliseconds has elapsed, which ever occurs
   first.  This function returns the number of sockets ready for reading,
   or -1 if there was an error with the select() system call.
*/
int SDLNet_CheckSockets(SDLNet_SocketSet set, Uint32 timeout)
{
    int i;
    SOCKET maxfd;
    int retval;
    struct timeval tv;
    fd_set mask;

    /* Find the largest file descriptor */
    maxfd = 0;
    for ( i=set->numsockets-1; i>=0; --i ) {
        if ( set->sockets[i]->channel > maxfd ) {
            maxfd = set->sockets[i]->channel;
        }
    }

    /* Check the file descriptors for available data */
    do {
        SDLNet_SetLastError(0);

        /* Set up the mask of file descriptors */
        FD_ZERO(&mask);
        for ( i=set->numsockets-1; i>=0; --i ) {
            FD_SET(set->sockets[i]->channel, &mask);
        }

        /* Set up the timeout */
        tv.tv_sec = timeout/1000;
        tv.tv_usec = (timeout%1000)*1000;

        /* Look! */
        retval = select(maxfd+1, &mask, NULL, NULL, &tv);
    } while ( SDLNet_GetLastError() == EINTR );

    /* Mark all file descriptors ready that have data available */
    if ( retval > 0 ) {
        for ( i=set->numsockets-1; i>=0; --i ) {
            if ( FD_ISSET(set->sockets[i]->channel, &mask) ) {
                set->sockets[i]->ready = 1;
            }
        }
    }
    return(retval);
}

/* Free a set of sockets allocated by SDL_NetAllocSocketSet() */
extern void SDLNet_FreeSocketSet(SDLNet_SocketSet set)
{
    if ( set ) {
        SDL_free(set->sockets);
        SDL_free(set);
    }
}

