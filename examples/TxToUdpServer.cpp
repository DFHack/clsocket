
#include "SimpleSocket.h"

#include <string.h>

#define MAX_PACKET 4096

int main(int argc, char **argv)
{
    const char *pcHostname = 0;
    const char *pcSendText = 0;
    const char *pcBindAddr = 0;
    const uint16 portno = 6789;
    int bindPort = -1;
    int mcastTTL = -1;
    int argidx = 1;

    if ( argidx < argc )
        pcHostname = argv[argidx++];

    if ( argidx < argc )
        pcSendText = argv[argidx++];

    if ( argidx + 1 < argc && !strcmp(argv[argidx], "-m") )
    {
        argidx++;
        mcastTTL = atoi(argv[argidx++]);
    }

    if ( argidx + 1 < argc )
    {
        pcBindAddr = argv[argidx++];
        bindPort = atoi(argv[argidx++]);
    }

    if ( !pcHostname || !pcSendText )
    {
        fprintf(stderr, "usage: %s <host> <text> [-m <ttl>] [<bind-address> <bind-port>]\n", argv[0] );
        fprintf(stderr, "  -m : activate multicast on socket\n");
        return 10;
    }

    //--------------------------------------------------------------------------
    // Initialize our socket object
    //--------------------------------------------------------------------------
    CSimpleSocket socket( CSimpleSocket::SocketTypeUdp );
    socket.Initialize();

    if ( pcBindAddr && bindPort >= 0 )
    {
        bool ret = socket.Bind( pcBindAddr, uint16(bindPort) );
        fprintf(stderr, "bind to %s:%u %s\n", pcBindAddr, bindPort, (ret ? "successful":"failed") );
    }
    else
        fprintf(stderr, "binding not parametrized\n");

    bool bConnOK = socket.Open( pcHostname, portno );
    if ( !bConnOK )
    {
        fprintf(stderr, "error connecting to '%s'!\n", pcHostname );
        return 10;
    }

    if ( mcastTTL >= 0 && mcastTTL < 256 )
    {
        bool ret = socket.SetMulticast(true, uint8(mcastTTL));
        fprintf(stderr, "Setting Multicast with TTL %d %s\n", mcastTTL, ret ? "was successful" : "failed");
    }
    else
        fprintf(stderr, "multicast not parametrized\n");

    fprintf(stderr, "\nLocal is %s. Local: %s:%u   "
          , ( socket.IsServerSide() ? "Server" : "Client" )
          , socket.GetLocalAddr(), (unsigned)socket.GetLocalPort());
    fprintf(stderr, "Peer: %s:%u\n", socket.GetPeerAddr(), (unsigned)socket.GetPeerPort());

    size_t sendSize = strlen( pcSendText );
    socket.Send( pcSendText, (int32)sendSize );

    return 1;
}
