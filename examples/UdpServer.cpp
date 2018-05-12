
#include "PassiveSocket.h"       // Include header for active socket object definition

#define MAX_PACKET 4096

int main(int argc, char **argv)
{
    char buffer[ MAX_PACKET ];
    CPassiveSocket passiveSocket( CSimpleSocket::SocketTypeUdp );

    const char * bindAddr = 0;
    unsigned bindPort = 6789;
    if ( argc <= 1 )
        fprintf(stderr, "usage: %s [<bind-address> [<bind-port>]]\n", argv[0] );
    if ( 1 < argc )
        bindAddr = argv[1];
    if ( 2 < argc )
        bindPort = atoi(argv[2]) & 65535;

    //--------------------------------------------------------------------------
    // Initialize our socket object 
    //--------------------------------------------------------------------------
    passiveSocket.Initialize();
    fprintf(stderr, "binding to %s:%u\n", bindAddr, bindPort);
    passiveSocket.Bind( bindAddr, uint16(bindPort) );   // not "127.0.0.1" to allow testing with remotes
    CSimpleSocket &socket = passiveSocket;

    fprintf(stderr, "\nLocal is %s. Local: %s:%u   "
          , ( socket.IsServerSide() ? "Server" : "Client" )
          , socket.GetLocalAddr(), (unsigned)socket.GetLocalPort());
    fprintf(stderr, "(invalid) Peer: %s:%u\n", socket.GetPeerAddr(), (unsigned)socket.GetPeerPort());

    socket.SetReceiveTimeoutMillis(500);

    while (true)
    {
        //----------------------------------------------------------------------
        // Receive request from the client.
        //----------------------------------------------------------------------

        //socket.WaitUntilReadable(100);

        int32 peekRx = socket.GetNumReceivableBytes();
        int32 rx = socket.Receive(MAX_PACKET, buffer);
        if ( peekRx != rx && !( peekRx == 0 && rx < 0 ) )
            fprintf( stderr, "GetNumReceivableBytes() = %d != %d = Receive() !\n", peekRx, rx );
        if (rx > 0)
        {
            fprintf(stderr, "\nLocal is %s. Local: %s:%u   "
                  , ( socket.IsServerSide() ? "Server" : "Client" )
                  , socket.GetLocalAddr(), (unsigned)socket.GetLocalPort());
            fprintf(stderr, "Peer: %s:%u\n", socket.GetPeerAddr(), (unsigned)socket.GetPeerPort());

            fprintf(stderr, "\nreceived %d bytes:\n", rx );
            for ( int k = 0; k < rx; ++k )
                fprintf(stderr, "%c", buffer[k]);
            fprintf(stderr, "\n");
            if ( 0 == memcmp(buffer, "quit", 4) )
                break;
        }
        else if ( CSimpleSocket::SocketEwouldblock == socket.GetSocketError() )
            fprintf(stderr, ".");
        else if ( !socket.IsNonblocking() && CSimpleSocket::SocketTimedout == socket.GetSocketError() )
            fprintf(stderr, ".");
        else
        {
            fprintf(stderr, "Error: %s\n", socket.DescribeError() );
            fprintf(stderr, "Error: %s, code %d\n"
                    , ( socket.IsNonblocking() ? "NonBlocking socket" : "Blocking socket" )
                    , (int) socket.GetSocketError()
                    );
        }
    }

    return 1;
}
