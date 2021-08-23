
#include "SimpleSocket.h"       // Include header for active socket object definition

#include <string.h>

#define MAX_PACKET 4096

int main(int argc, char **argv)
{
    CSimpleSocket socket( CSimpleSocket::SocketTypeUdp );

    if ( argc < 3 )
    {
        fprintf(stderr, "usage: %s <host> <text> [-m [<ttl>] | <bind-address> <bind-port>]\n", argv[0] );
        fprintf(stderr, "  -m : activate multicast on socket\n");
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

    if ( 3 < argc && !strcmp(argv[3], "-m") )
    {
        uint8 multicastTTL = ( 4 < argc ) ? atoi(argv[4]) : 1;
        bool ret = socket.SetMulticast(true, 3);
        fprintf(stderr, "Setting Multicast with TTL 3 %s\n", ret ? "was successful" : "failed");
    }

    fprintf(stderr, "\nLocal is %s. Local: %s:%u   "
          , ( socket.IsServerSide() ? "Server" : "Client" )
          , socket.GetLocalAddr(), (unsigned)socket.GetLocalPort());
    fprintf(stderr, "Peer: %s:%u\n", socket.GetPeerAddr(), (unsigned)socket.GetPeerPort());

    size_t sendSize = strlen( argv[2] );
    socket.Send( argv[2], (int32)sendSize );

    return 1;
}
