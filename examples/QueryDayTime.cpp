
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
    if (socket.Open("time-C.timefreq.bldrdoc.gov", 13))
    {
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
