#include <pthread.h>
#include "PassiveSocket.h"

#ifdef WIN32
#include <windows.h>

  // usually defined with #include <unistd.h>
  static void sleep( unsigned int seconds )
  {
    Sleep( seconds * 1000 );
  }
#endif

#define MAX_PACKET  4096
#define TEST_PACKET "Test Packet"

struct thread_data
{
    const char *pszServerAddr;
    short int   nPort;
    int         nNumBytesToReceive;
    int         nTotalPayloadSize;
};


void *CreateTCPEchoServer(void *param)
{
    CPassiveSocket socket;
    CActiveSocket *pClient = NULL;
    struct thread_data *pData = (struct thread_data *)param;
    int            nBytesReceived = 0;

    socket.Initialize();
    socket.Listen(pData->pszServerAddr, pData->nPort);

    if ((pClient = socket.Accept()) != NULL)
    {
        while (nBytesReceived != pData->nTotalPayloadSize)
        {
            if (nBytesReceived += pClient->Receive(pData->nNumBytesToReceive))
            {
                pClient->Send((const uint8 *)pClient->GetData(), pClient->GetBytesReceived());
            }
        }

        sleep(1);

        delete pClient;
    }

    socket.Close();

    return NULL;
}

int main(int argc, char **argv)
{
    pthread_t          threadId;
    struct thread_data thData;
    CActiveSocket      client;
    char result[1024];

    thData.pszServerAddr = "127.0.0.1";
    thData.nPort = 6789;
    thData.nNumBytesToReceive = 1;
    thData.nTotalPayloadSize = (int)strlen(TEST_PACKET);

    pthread_create(&threadId, 0, CreateTCPEchoServer, &thData);
    sleep(1); // allow a second for the thread to create and listen

    client.Initialize();
    client.SetNonblocking();

    if (client.Open("127.0.0.1", 6789))
    {
        if (client.Send((uint8 *)TEST_PACKET, strlen(TEST_PACKET)))
        {
            int numBytes = -1;
            int bytesReceived = 0;

            client.WaitUntilReadable(100);  // wait up to 100 ms

            //while (bytesReceived != strlen(TEST_PACKET))
            while ( numBytes != 0 )   // Receive() until socket gets closed
            {
                int32 peekRx = client.GetNumReceivableBytes();

                numBytes = client.Receive(MAX_PACKET);

                if ( peekRx != numBytes && !( peekRx == 0 && numBytes < 0 ) )
                  fprintf( stderr, "\nGetNumReceivableBytes() = %d != %d = Receive() !\n", peekRx, numBytes );

                if (numBytes > 0)
                {
                    bytesReceived += numBytes;
                    memset(result, 0, 1024);
                    memcpy(result, client.GetData(), numBytes);
                    printf("\nreceived %d bytes: '%s'\n", numBytes, result);
                }
                else if ( CSimpleSocket::SocketEwouldblock == client.GetSocketError() )
                    fprintf(stderr, ".");
                else if ( !client.IsNonblocking() && CSimpleSocket::SocketTimedout == client.GetSocketError() )
                      fprintf(stderr, "w");
                else
                {
                    printf("\nreceived %d bytes\n", numBytes);

                    fprintf(stderr, "Error: %s\n", client.DescribeError() );
                    fprintf(stderr, "Error: %s, code %d\n"
                            , ( client.IsNonblocking() ? "NonBlocking socket" : "Blocking socket" )
                            , (int) client.GetSocketError()
                            );
                }

                client.WaitUntilReadable(100);  // wait up to 100 ms
            }
        }
    }
}
