
#include "SimpleSocket.h"       // Include header for active socket object definition

#include <string.h>

#define MAX_PACKET 4096

int main(int argc, char **argv)
{
    CSimpleSocket socket( CSimpleSocket::SocketTypeUdp );

    if ( argc < 3 )
    {
        fprintf(stderr, "usage: %s <host> <text>\n", argv[0] );
        return 10;
    }

    //--------------------------------------------------------------------------
    // Initialize our socket object
    //--------------------------------------------------------------------------
    socket.Initialize();

    bool bConnOK = socket.Open( argv[1], 6789 );
    if ( !bConnOK )
    {
        fprintf(stderr, "error connecting to '%s'!\n", argv[1] );
        return 10;
    }

    size_t sendSize = strlen( argv[2] );
    socket.Send( argv[2], (int32)sendSize );

    return 1;
}
