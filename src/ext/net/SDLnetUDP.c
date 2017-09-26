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

#ifdef __WIN32__
#define srandom srand
#define random  rand
#endif

struct UDP_channel {
    int numbound;
    IPaddress address[SDLNET_MAX_UDPADDRESSES];
};

struct _UDPsocket {
    int ready;
    SOCKET channel;
    IPaddress address;

    struct UDP_channel binding[SDLNET_MAX_UDPCHANNELS];

    /* For debugging purposes */
    int packetloss;
};

/* Allocate/free a single UDP packet 'size' bytes long.
   The new packet is returned, or NULL if the function ran out of memory.
 */
extern UDPpacket *SDLNet_AllocPacket(int size)
{
    UDPpacket *packet;
    int error;


    error = 1;
    packet = (UDPpacket *)SDL_malloc(sizeof(*packet));
    if ( packet != NULL ) {
        packet->maxlen = size;
        packet->data = (Uint8 *)SDL_malloc(size);
        if ( packet->data != NULL ) {
            error = 0;
        }
    }
    if ( error ) {
        SDLNet_SetError("Out of memory");
        SDLNet_FreePacket(packet);
        packet = NULL;
    }
    return(packet);
}
int SDLNet_ResizePacket(UDPpacket *packet, int newsize)
{
    Uint8 *newdata;

    newdata = (Uint8 *)SDL_malloc(newsize);
    if ( newdata != NULL ) {
        SDL_free(packet->data);
        packet->data = newdata;
        packet->maxlen = newsize;
    }
    return(packet->maxlen);
}
extern void SDLNet_FreePacket(UDPpacket *packet)
{
    if ( packet ) {
        SDL_free(packet->data);
        SDL_free(packet);
    }
}

/* Allocate/Free a UDP packet vector (array of packets) of 'howmany' packets,
   each 'size' bytes long.
   A pointer to the packet array is returned, or NULL if the function ran out
   of memory.
 */
UDPpacket **SDLNet_AllocPacketV(int howmany, int size)
{
    UDPpacket **packetV;

    packetV = (UDPpacket **)SDL_malloc((howmany+1)*sizeof(*packetV));
    if ( packetV != NULL ) {
        int i;
        for ( i=0; i<howmany; ++i ) {
            packetV[i] = SDLNet_AllocPacket(size);
            if ( packetV[i] == NULL ) {
                break;
            }
        }
        packetV[i] = NULL;

        if ( i != howmany ) {
            SDLNet_SetError("Out of memory");
            SDLNet_FreePacketV(packetV);
            packetV = NULL;
        }
    }
    return(packetV);
}
void SDLNet_FreePacketV(UDPpacket **packetV)
{
    if ( packetV ) {
        int i;
        for ( i=0; packetV[i]; ++i ) {
            SDLNet_FreePacket(packetV[i]);
        }
        SDL_free(packetV);
    }
}

/* Since the UNIX/Win32/BeOS code is so different from MacOS,
   we'll just have two completely different sections here.
*/

/* Open a UDP network socket
   If 'port' is non-zero, the UDP socket is bound to a fixed local port.
*/
UDPsocket SDLNet_UDP_Open(Uint16 port)
{
    UDPsocket sock;
    struct sockaddr_in sock_addr;
    socklen_t sock_len;

    /* Allocate a UDP socket structure */
    sock = (UDPsocket)SDL_malloc(sizeof(*sock));
    if ( sock == NULL ) {
        SDLNet_SetError("Out of memory");
        goto error_return;
    }
    SDL_memset(sock, 0, sizeof(*sock));
    SDL_memset(&sock_addr, 0, sizeof(sock_addr));

    /* Open the socket */
    sock->channel = socket(AF_INET, SOCK_DGRAM, 0);
    if ( sock->channel == INVALID_SOCKET )
    {
        SDLNet_SetError("Couldn't create socket");
        goto error_return;
    }

    /* Bind locally, if appropriate */
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = INADDR_ANY;
    sock_addr.sin_port = SDLNet_Read16(&port);

    /* Bind the socket for listening */
    if ( bind(sock->channel, (struct sockaddr *)&sock_addr,
            sizeof(sock_addr)) == SOCKET_ERROR ) {
        SDLNet_SetError("Couldn't bind to local port");
        goto error_return;
    }

    /* Get the bound address and port */
    sock_len = sizeof(sock_addr);
    if ( getsockname(sock->channel, (struct sockaddr *)&sock_addr, &sock_len) < 0 ) {
        SDLNet_SetError("Couldn't get socket address");
        goto error_return;
    }

    /* Fill in the channel host address */
    sock->address.host = sock_addr.sin_addr.s_addr;
    sock->address.port = sock_addr.sin_port;

#ifdef SO_BROADCAST
    /* Allow LAN broadcasts with the socket */
    { int yes = 1;
        setsockopt(sock->channel, SOL_SOCKET, SO_BROADCAST, (char*)&yes, sizeof(yes));
    }
#endif
#ifdef IP_ADD_MEMBERSHIP
    /* Receive LAN multicast packets on 224.0.0.1
       This automatically works on Mac OS X, Linux and BSD, but needs
       this code on Windows.
    */
    /* A good description of multicast can be found here:
        http://www.docs.hp.com/en/B2355-90136/ch05s05.html
    */
    /* FIXME: Add support for joining arbitrary groups to the API */
    {
        struct ip_mreq  g;

        g.imr_multiaddr.s_addr = inet_addr("224.0.0.1");
        g.imr_interface.s_addr = INADDR_ANY;
        setsockopt(sock->channel, IPPROTO_IP, IP_ADD_MEMBERSHIP,
               (char*)&g, sizeof(g));
    }
#endif

    /* The socket is ready */

    return(sock);

error_return:
    SDLNet_UDP_Close(sock);

    return(NULL);
}

void SDLNet_UDP_SetPacketLoss(UDPsocket sock, int percent)
{
    /* FIXME: We may want this behavior to be reproducible
          but there isn't a portable reentrant random
          number generator with good randomness.
    */
    srandom(time(NULL));

    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }
    sock->packetloss = percent;
}

/* Verify that the channel is in the valid range */
static int ValidChannel(int channel)
{
    if ( (channel < 0) || (channel >= SDLNET_MAX_UDPCHANNELS) ) {
        SDLNet_SetError("Invalid channel");
        return(0);
    }
    return(1);
}

/* Bind the address 'address' to the requested channel on the UDP socket.
   If the channel is -1, then the first unbound channel that has not yet
   been bound to the maximum number of addresses will be bound with
   the given address as it's primary address.
   If the channel is already bound, this new address will be added to the
   list of valid source addresses for packets arriving on the channel.
   If the channel is not already bound, then the address becomes the primary
   address, to which all outbound packets on the channel are sent.
   This function returns the channel which was bound, or -1 on error.
*/
int SDLNet_UDP_Bind(UDPsocket sock, int channel, const IPaddress *address)
{
    struct UDP_channel *binding;

    if ( sock == NULL ) {
        SDLNet_SetError("Passed a NULL socket");
        return(-1);
    }

    if ( channel == -1 ) {
        for ( channel=0; channel < SDLNET_MAX_UDPCHANNELS; ++channel ) {
            binding = &sock->binding[channel];
            if ( binding->numbound < SDLNET_MAX_UDPADDRESSES ) {
                break;
            }
        }
    } else {
        if ( ! ValidChannel(channel) ) {
            return(-1);
        }
        binding = &sock->binding[channel];
    }
    if ( binding->numbound == SDLNET_MAX_UDPADDRESSES ) {
        SDLNet_SetError("No room for new addresses");
        return(-1);
    }
    binding->address[binding->numbound++] = *address;
    return(channel);
}

/* Unbind all addresses from the given channel */
void SDLNet_UDP_Unbind(UDPsocket sock, int channel)
{
    if ( (channel >= 0) && (channel < SDLNET_MAX_UDPCHANNELS) ) {
        sock->binding[channel].numbound = 0;
    }
}

/* Get the primary IP address of the remote system associated with the
   socket and channel.
   If the channel is not bound, this function returns NULL.
 */
IPaddress *SDLNet_UDP_GetPeerAddress(UDPsocket sock, int channel)
{
    IPaddress *address;

    address = NULL;
    switch (channel) {
        case -1:
            /* Return the actual address of the socket */
            address = &sock->address;
            break;
        default:
            /* Return the address of the bound channel */
            if ( ValidChannel(channel) &&
                (sock->binding[channel].numbound > 0) ) {
                address = &sock->binding[channel].address[0];
            }
            break;
    }
    return(address);
}

/* Send a vector of packets to the the channels specified within the packet.
   If the channel specified in the packet is -1, the packet will be sent to
   the address in the 'src' member of the packet.
   Each packet will be updated with the status of the packet after it has
   been sent, -1 if the packet send failed.
   This function returns the number of packets sent.
*/
int SDLNet_UDP_SendV(UDPsocket sock, UDPpacket **packets, int npackets)
{
    int numsent, i, j;
    struct UDP_channel *binding;
    int status;
    int sock_len;
    struct sockaddr_in sock_addr;

    if ( sock == NULL ) {
        SDLNet_SetError("Passed a NULL socket");
        return(0);
    }

    /* Set up the variables to send packets */
    sock_len = sizeof(sock_addr);

    numsent = 0;
    for ( i=0; i<npackets; ++i )
    {
        /* Simulate packet loss, if desired */
        if (sock->packetloss) {
            if ((random()%100) <= sock->packetloss) {
                packets[i]->status = packets[i]->len;
                ++numsent;
                continue;
            }
        }

        /* if channel is < 0, then use channel specified in sock */

        if ( packets[i]->channel < 0 )
        {
            sock_addr.sin_addr.s_addr = packets[i]->address.host;
            sock_addr.sin_port = packets[i]->address.port;
            sock_addr.sin_family = AF_INET;
            status = sendto(sock->channel,
                    packets[i]->data, packets[i]->len, 0,
                    (struct sockaddr *)&sock_addr,sock_len);
            if ( status >= 0 )
            {
                packets[i]->status = status;
                ++numsent;
            }
        }
        else
        {
            /* Send to each of the bound addresses on the channel */
#ifdef DEBUG_NET
            printf("SDLNet_UDP_SendV sending packet to channel = %d\n", packets[i]->channel );
#endif

            binding = &sock->binding[packets[i]->channel];

            for ( j=binding->numbound-1; j>=0; --j )
            {
                sock_addr.sin_addr.s_addr = binding->address[j].host;
                sock_addr.sin_port = binding->address[j].port;
                sock_addr.sin_family = AF_INET;
                status = sendto(sock->channel,
                        packets[i]->data, packets[i]->len, 0,
                        (struct sockaddr *)&sock_addr,sock_len);
                if ( status >= 0 )
                {
                    packets[i]->status = status;
                    ++numsent;
                }
            }
        }
    }

    return(numsent);
}

int SDLNet_UDP_Send(UDPsocket sock, int channel, UDPpacket *packet)
{
    /* This is silly, but... */
    packet->channel = channel;
    return(SDLNet_UDP_SendV(sock, &packet, 1));
}

/* Returns true if a socket is has data available for reading right now */
static int SocketReady(SOCKET sock)
{
    int retval = 0;
    struct timeval tv;
    fd_set mask;

    /* Check the file descriptors for available data */
    do {
        SDLNet_SetLastError(0);

        /* Set up the mask of file descriptors */
        FD_ZERO(&mask);
        FD_SET(sock, &mask);

        /* Set up the timeout */
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        /* Look! */
        retval = select(sock+1, &mask, NULL, NULL, &tv);
    } while ( SDLNet_GetLastError() == EINTR );

    return(retval == 1);
}

/* Receive a vector of pending packets from the UDP socket.
   The returned packets contain the source address and the channel they arrived
   on.  If they did not arrive on a bound channel, the the channel will be set
   to -1.
   This function returns the number of packets read from the network, or -1
   on error.  This function does not block, so can return 0 packets pending.
*/
extern int SDLNet_UDP_RecvV(UDPsocket sock, UDPpacket **packets)
{
    int numrecv, i, j;
    struct UDP_channel *binding;
    socklen_t sock_len;
    struct sockaddr_in sock_addr;

    if ( sock == NULL ) {
        return(0);
    }

    numrecv = 0;
    while ( packets[numrecv] && SocketReady(sock->channel) )
    {
        UDPpacket *packet;

        packet = packets[numrecv];

        sock_len = sizeof(sock_addr);
        packet->status = recvfrom(sock->channel,
                packet->data, packet->maxlen, 0,
                (struct sockaddr *)&sock_addr,
                        &sock_len);
        if ( packet->status >= 0 ) {
            packet->len = packet->status;
            packet->address.host = sock_addr.sin_addr.s_addr;
            packet->address.port = sock_addr.sin_port;
            packet->channel = -1;

            for (i=(SDLNET_MAX_UDPCHANNELS-1); i>=0; --i )
            {
                binding = &sock->binding[i];

                for ( j=binding->numbound-1; j>=0; --j )
                {
                    if ( (packet->address.host == binding->address[j].host) &&
                         (packet->address.port == binding->address[j].port) )
                    {
                        packet->channel = i;
                        goto foundit; /* break twice */
                    }
                }
            }
foundit:
            ++numrecv;
        }

        else
        {
            packet->len = 0;
        }
    }

    sock->ready = 0;

    return(numrecv);
}

/* Receive a single packet from the UDP socket.
   The returned packet contains the source address and the channel it arrived
   on.  If it did not arrive on a bound channel, the the channel will be set
   to -1.
   This function returns the number of packets read from the network, or -1
   on error.  This function does not block, so can return 0 packets pending.
*/
int SDLNet_UDP_Recv(UDPsocket sock, UDPpacket *packet)
{
    UDPpacket *packets[2];

    /* Receive a packet array of 1 */
    packets[0] = packet;
    packets[1] = NULL;
    return(SDLNet_UDP_RecvV(sock, packets));
}

/* Close a UDP network socket */
extern void SDLNet_UDP_Close(UDPsocket sock)
{
    if ( sock != NULL ) {
        if ( sock->channel != INVALID_SOCKET ) {
            closesocket(sock->channel);
        }
        SDL_free(sock);
    }
}

