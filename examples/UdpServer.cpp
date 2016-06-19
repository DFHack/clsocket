
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
    passiveSocket.Bind( "127.0.0.1", 6789 );
    CSimpleSocket &socket = passiveSocket;

    socket.SetReceiveTimeoutMillis(500);

    while (true)
    {
        //----------------------------------------------------------------------
        // Receive request from the client.
        //----------------------------------------------------------------------
        int32 rx = socket.Receive(MAX_PACKET, buffer);
        if (rx > 0)
        {
            fprintf(stderr, "\nreceived %d bytes:\n", rx );
            for ( int k = 0; k < rx; ++k )
                fprintf(stderr, "%c", buffer[k]);
            fprintf(stderr, "\n");
            if ( 0 == memcmp(buffer, "quit", 4) )
                break;
        }
        else if ( socket.IsNonblocking() && CSimpleSocket::SocketEwouldblock == socket.GetSocketError() )
            fprintf(stderr, ".");
        else if ( !socket.IsNonblocking() && CSimpleSocket::SocketTimedout == socket.GetSocketError() )
            fprintf(stderr, ".");
        else
            fprintf(stderr, "Error: %s\n", socket.DescribeError() );
    }

    return 1;
}
