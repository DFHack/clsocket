
#include "SimpleSocket.h"       // Include header for active socket object definition

#include <string.h>

#define MAX_PACKET 4096

int main(int argc, char **argv)
{
    CSimpleSocket socket;

    if ( argc < 3 )
    {
        fprintf(stderr, "usage: %s <host> <text> [<bind-address> <bind-port>]\n", argv[0] );
        return 10;
    }

    //--------------------------------------------------------------------------
    // Initialize our socket object
    //--------------------------------------------------------------------------
    socket.Initialize();

    if ( 4 < argc )
    {
        unsigned bindPort = atoi(argv[4]);
        bool ret = socket.Bind( argv[3], uint16(bindPort) );
        fprintf(stderr, "bind to %s:%u %s\n", argv[3], bindPort, (ret ? "successful":"failed") );
    }

    bool bConnOK = socket.Open( argv[1], 6789 );
    if ( !bConnOK )
    {
        fprintf(stderr, "error connecting to '%s'!\n", argv[1] );
        return 10;
    }

    fprintf(stderr, "\nLocal is %s. Local: %s:%u   "
          , ( socket.IsServerSide() ? "Server" : "Client" )
          , socket.GetLocalAddr(), (unsigned)socket.GetLocalPort());
    fprintf(stderr, "Peer: %s:%u\n", socket.GetPeerAddr(), (unsigned)socket.GetPeerPort());

    uint32 nSendBufSize = socket.GetSendWindowSize();
    fprintf(stderr, "default Send Buffer Size %u\n", nSendBufSize );
    nSendBufSize = socket.SetSendWindowSize( 80000 );
    fprintf(stderr, "new Send Buffer Size %u\n", nSendBufSize );

    size_t sendSize = strlen( argv[2] );
    socket.Transmit( (const uint8 *)argv[2], (int32)sendSize );

    socket.SetNonblocking();
    //socket.SetBlocking();

    int32 rxSize = 0;
    char rxBuffer[16];
    while (rxSize < (int32)sendSize)
    {
        if ( !socket.WaitUntilReadable(500) )
        {
            fprintf(stderr, ".");
            continue;
        }

        int32 peekRx = socket.GetNumReceivableBytes();
        int32 rx = socket.Receive( 15, rxBuffer );
        if ( peekRx != rx && !( peekRx == 0 && rx < 0 ) )
            fprintf( stderr, "GetNumReceivableBytes() = %d != %d = Receive() !\n", peekRx, rx );

        if ( rx > 0 )
        {
            rxBuffer[rx] = '\0';
            fprintf(stderr, "\nreceived %d bytes: '%s'\n", rx, rxBuffer );
            rxSize += rx;
        }
        else if ( !socket.IsSocketValid() )
            break;
    }

    return 1;
}
