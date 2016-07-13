
#include "PassiveSocket.h"       // Include header for active socket object definition

#define MAX_PACKET 4096

int main(int argc, char **argv)
{
    char buffer[ MAX_PACKET ];
    CPassiveSocket passiveSocket( CSimpleSocket::SocketTypeUdp );

    //--------------------------------------------------------------------------
    // Initialize our socket object 
    //--------------------------------------------------------------------------
    passiveSocket.Initialize();
    passiveSocket.Bind( 0, 6789 );  // NULL (not "127.0.0.1") to allow testing with remotes
    CSimpleSocket &socket = passiveSocket;

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
            fprintf(stderr, "\n%s. Local: %s:%u   Peer: %s:%u\n"
                    , ( passiveSocket.IsServerSide() ? "Local is Server" : "Local is Client" )
                    , passiveSocket.GetLocalAddr(), (unsigned)passiveSocket.GetLocalPort()
                    , passiveSocket.GetPeerAddr(), (unsigned)passiveSocket.GetPeerPort()
                    );

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
