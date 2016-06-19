
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

    //--------------------------------------------------------------------------
    // Initialize our socket object 
    //--------------------------------------------------------------------------
    socket.Initialize();
    socket.Listen("127.0.0.1", 6789);

    while (true)
    {
        if ((pClient = socket.Accept()) != NULL)
        {
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
