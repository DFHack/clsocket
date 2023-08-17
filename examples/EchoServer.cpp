
#include "PassiveSocket.h"       // Include header for active socket object definition

#define MAX_PACKET 4096

int main(int argc, char **argv)
{
    CPassiveSocket socket;
    CActiveSocket *pClient = NULL;

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
    socket.Initialize();
    fprintf(stderr, "binding to %s:%u\n", bindAddr, bindPort);
    socket.Listen( bindAddr, uint16(bindPort) );    // not "127.0.0.1" to allow testing with remotes

    while (true)
    {
        if ((pClient = socket.Accept()) != NULL)
        {
            fprintf(stderr, "\nLocal is %s. Local: %s:%u   "
                  , ( pClient->IsServerSide() ? "Server" : "Client" )
                  , pClient->GetLocalAddr(), (unsigned)pClient->GetLocalPort());
            fprintf(stderr, "Peer: %s:%u\n", pClient->GetPeerAddr(), (unsigned)pClient->GetPeerPort());

            //----------------------------------------------------------------------
            // Receive request from the client.
            //----------------------------------------------------------------------

            pClient->WaitUntilReadable(100);
            int32 peekRx = pClient->GetNumReceivableBytes();

            if (pClient->Receive(MAX_PACKET))
            {
                fprintf(stderr, "\n%s. Local: %s:%u   Peer: %s:%u\n"
                      , ( pClient->IsServerSide() ? "Local is Server" : "Local is Client" )
                      , pClient->GetLocalAddr(), (unsigned)pClient->GetLocalPort()
                      , pClient->GetPeerAddr(), (unsigned)pClient->GetPeerPort()
                      );

                //------------------------------------------------------------------
                // Send response to client and close connection to the client.
                //------------------------------------------------------------------
                int32 rx = pClient->GetBytesReceived();

                if ( peekRx != rx && !( peekRx == 0 && rx < 0 ) )
                    fprintf( stderr, "GetNumReceivableBytes() = %d != %d = Receive() !\n", peekRx, rx );

                uint8 *txt = pClient->GetData();
                pClient->Send( txt, rx );
                fprintf(stderr, "received and sent %d bytes:\n", rx );
                for ( int k = 0; k < rx; ++k )
                    fprintf(stderr, "%c", txt[k]);
                fprintf(stderr, "\n");
                pClient->Close();
            }

            delete pClient;
        }
    }

    //-----------------------------------------------------------------------------
    // Receive request from the client.
    //-----------------------------------------------------------------------------
    socket.Close();

    return 1;
}
