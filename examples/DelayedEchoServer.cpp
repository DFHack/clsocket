
#include "PassiveSocket.h"       // Include header for active socket object definition

#define MAX_PACKET 4096


#ifdef WIN32
#include <windows.h>

  static void sleep( unsigned int seconds )
  {
    Sleep( seconds * 1000 );
  }
#elif defined(_LINUX) || defined (_DARWIN)
  #include <unistd.h>
#endif

int main(int argc, char **argv)
{
    CPassiveSocket socket;
    CSimpleSocket *pClient = NULL;

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
            if (pClient->Receive(MAX_PACKET))
            {
                //------------------------------------------------------------------
                // Send response to client and close connection to the client.
                //------------------------------------------------------------------
                int32 rx = pClient->GetBytesReceived();
                uint8 *txt = pClient->GetData();
                fprintf(stderr, "received %d bytes:\n", rx );
                for ( int k = 0; k < rx; ++k )
                    fprintf(stderr, "%c", txt[k]);
                fprintf(stderr, "\n");
                sleep(5);
                pClient->Send( txt, rx );
                fprintf(stderr, "sent back after 5 seconds\n" );
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
