
#include <string.h>
#include "ActiveSocket.h"       // Include header for active socket object definition

int main(int argc, char **argv)
{
    CActiveSocket socket;       // Instantiate active socket object (defaults to TCP).
    char          time[50];

    memset(&time, 0, 50);

    //--------------------------------------------------------------------------
    // Initialize our socket object 
    //--------------------------------------------------------------------------
    socket.Initialize();

    //--------------------------------------------------------------------------
    // Create a connection to the time server so that data can be sent
    // and received.
    //--------------------------------------------------------------------------
    const char * timeServer = ( argc >= 2 ) ? argv[1] : "time-C.timefreq.bldrdoc.gov";
    int PortNo = ( argc >= 3 ) ? atoi(argv[2]) : 13;
    fprintf(stderr, "trying to connect to timeserver %s:%d ..\n", timeServer, PortNo);
    if (socket.Open(timeServer, (uint16)PortNo))
    {
        fprintf(stderr, "\nLocal is %s. Local: %s:%u   "
              , ( socket.IsServerSide() ? "Server" : "Client" )
              , socket.GetLocalAddr(), (unsigned)socket.GetLocalPort());
        fprintf(stderr, "Peer: %s:%u\n", socket.GetPeerAddr(), (unsigned)socket.GetPeerPort());

        //----------------------------------------------------------------------
        // Send a requtest the server requesting the current time.
        //----------------------------------------------------------------------
        if (socket.Send((const uint8 *)"\n", 1))
        {
            //----------------------------------------------------------------------
            // Receive response from the server.
            //----------------------------------------------------------------------
            socket.Receive(49);
            memcpy(&time, socket.GetData(), 49);
            printf("%s\n", time);

            //----------------------------------------------------------------------
            // Close the connection.
            //----------------------------------------------------------------------
            socket.Close();
        }
    }


    return 1;
}
