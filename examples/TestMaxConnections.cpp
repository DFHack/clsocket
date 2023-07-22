
#include "PassiveSocket.h"

#include <thread>
#include <functional>
#include <cstdio>
#include <cstring>

#define MAX_CLIENTS 8192
#define MAX_PACKET  4096
//#define TEST_PACKET "Test Packet"
#define TEST_PACKET "Test"


struct thread_data
{
    const int       max_clients;
    const char    * pszServerAddr;
    short int       nPort;
    int             nNumBytesToReceive;
    int             nTotalPayloadSize;
    CPassiveSocket  server_socket;
    int             nClients;
    CSimpleSocket * client_sockets[MAX_CLIENTS];
    bool            terminate;
    bool            ready;

    thread_data(int max_clients_)
        : max_clients(max_clients_)
    {
        pszServerAddr = "127.0.0.1";
        nPort = 6789;
        nNumBytesToReceive = 1;
        nTotalPayloadSize = (int)strlen(TEST_PACKET);
        nClients = 0;
        terminate = false;
        ready = false;
        for (int k=0; k < MAX_CLIENTS; ++k)
            client_sockets[k] = 0;
        if (!server_socket.Initialize())
        {
            fprintf(stderr, "Error: initializing socket!\n");
            return;
        }
        if (!server_socket.SetNonblocking())
        {
            fprintf(stderr, "Error: server/listen socket could set Nonblocking!\n");
            return;
        }
        if (!server_socket.Listen(pszServerAddr, nPort))
        {
            fprintf(stderr, "Error: listen failed!\n");
            return;
        }
        ready = true;
    }

    ~thread_data()
    {
        printf("\nserver: shutting down server!\n");
        server_socket.Close();
        printf("server: cleanup/delete connections ..\n");
        for (int k = 0; k < nClients; ++k)
            delete client_sockets[k];
    }

    bool serve();
};


bool thread_data::serve()
{
    if (!ready)
        return false;
    while (!terminate)
    {
        if (nClients >= max_clients)
        {
            fprintf(stderr, "\nserver: have served %d clients. shut down\n", nClients);
            return false;
        }
        if (!server_socket.Select(0, 100000))  // 100ms
        {
            // fprintf(stderr, "server: wait select\n");
            continue;
        }

        // printf("server: accept next connection %d ..\n", nClients);
        CSimpleSocket *pClient = server_socket.Accept();
        if (!pClient)
        {
            if (server_socket.GetSocketError() == CPassiveSocket::SocketEwouldblock)
            {
                fprintf(stderr, "server: accept() would block\n");
                continue;
            }

            fprintf(stderr, "server: Error at accept() for %d: code %d: %s\n",
                    nClients, (int)server_socket.GetSocketError(), server_socket.DescribeError() );
            break;
        }
        printf("server: accepted %d ..\n", nClients);

        client_sockets[nClients++] = pClient;
        int nBytesReceived = 0;
        while (!terminate && nBytesReceived < nTotalPayloadSize)
        {
            if (nBytesReceived += pClient->Receive(nNumBytesToReceive))
            {
                pClient->Send((const uint8 *)pClient->GetData(), pClient->GetBytesReceived());
            }
        }
        // keep the new connection open!
    }

    return true;
}


bool connect_new_client(CActiveSocket &client, int k, const thread_data &thData)
{
    bool conn_ok = client.Open("127.0.0.1", 6789);
    if (!conn_ok)
    {
        fprintf(stderr, "Error: %d could not connect to server!\n", k);
        return false;
    }

    int32 r = client.Send((uint8 *)TEST_PACKET, strlen(TEST_PACKET));
    if (!r)
    {
        fprintf(stderr, "Error: connection %d closed from peer while sending packet!\n", k);
        return false;
    }

    int numBytes = -1;
    int bytesReceived = 0;
    char result[1024];

    client.WaitUntilReadable(100);  // wait up to 100 ms

    // Receive() until socket gets closed or we already received the full packet
    while ( numBytes != 0 && bytesReceived < thData.nTotalPayloadSize )
    {
        numBytes = client.Receive(MAX_PACKET);

        if (numBytes > 0)
        {
            bytesReceived += numBytes;
            // memcpy(result, client.GetData(), numBytes);
            // printf("\nreceived %d bytes: '%s'\n", numBytes, result);
        }
        else if ( CSimpleSocket::SocketEwouldblock == client.GetSocketError() )
            fprintf(stderr, ".");
        else if ( !client.IsNonblocking() && CSimpleSocket::SocketTimedout == client.GetSocketError() )
            fprintf(stderr, "w");
        else
        {
            printf("\n%d: received %d bytes\n", k, numBytes);

            fprintf(stderr, "Error: %s\n", client.DescribeError() );
            fprintf(stderr, "Error: %s, code %d\n"
                    , ( client.IsNonblocking() ? "NonBlocking socket" : "Blocking socket" )
                    , (int) client.GetSocketError()
                    );
        }

        if ( bytesReceived >= thData.nTotalPayloadSize )
            return true;  // no need to wait
        // else
        //    printf("read %d of %d\n", bytesReceived, thData.nTotalPayloadSize);

        client.WaitUntilReadable(100);  // wait up to 100 ms
    }

    if (bytesReceived >= thData.nTotalPayloadSize)
        ;  // printf("%d: received %d bytes response.\n", k, bytesReceived);
    else
    {
        fprintf(stderr, "Error at %d: received %d bytes - expected %d!\n", k, bytesReceived, thData.nTotalPayloadSize);
        return false;
    }
    if (!numBytes)
    {
        fprintf(stderr, "Error: connection %d closed from peer\n", k);
        return false;
    }
    return true;
}



int main(int argc, char **argv)
{
    int max_clients = MAX_CLIENTS;
    int init_sockets_at_startup = 1;

    if ( 1 < argc )
    {
        if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-help") || !strcmp(argv[1], "-h") || !strcmp(argv[1], "/h"))
        {
            printf("usage: %s [<max_num_connections> [<init_sockets_at_startup>] ]\n", argv[0]);
            printf("  max_num_connections: number of connections to test. default = %d, max = %d\n", max_clients, max_clients);
            printf("  init_sockets_at_startup: 0 or 1. default = %d\n", init_sockets_at_startup);
            return 0;
        }
        max_clients = atoi(argv[1]);
    }

    if ( 2 < argc )
        init_sockets_at_startup = atoi(argv[2]);

    if (max_clients >= MAX_CLIENTS)
        max_clients = MAX_CLIENTS;

    thread_data thData{max_clients};
    if (!thData.ready)
        return 1;
    std::thread server_thread(&thread_data::serve, &thData);

    CActiveSocket * clients = new CActiveSocket[MAX_CLIENTS];
    printf("try to setup/communicate %d conncections ..\n", max_clients);

    bool have_err = false;
    if (init_sockets_at_startup)
    {
        for (int k = 0; k < max_clients; ++k)
        {
            CActiveSocket &client = clients[k];
            printf("init socket %d ..\n", k);
            if (!client.Initialize())
            {
                fprintf(stderr, "Error: socket %d could get initialized!\n", k);
                have_err = true;
                break;
            }
            if (!client.SetNonblocking())
            {
                fprintf(stderr, "Error: socket %d could set Nonblocking!\n", k);
                have_err = true;
                break;
            }
        }
    }

    for (int k = 0; !have_err && k < max_clients; ++k)
    {
        CActiveSocket &client = clients[k];
        printf("\nclient %d ..\n", k);
        if (!init_sockets_at_startup)
        {
            if (!client.Initialize())
            {
                fprintf(stderr, "Error: socket %d could get initialized!\n", k);
                break;
            }
            if (!client.SetNonblocking())
            {
                fprintf(stderr, "Error: socket %d could set Nonblocking!\n", k);
                break;
            }
        }
        have_err = !connect_new_client(client, k, thData);
    }

    thData.terminate = true;
    printf("trigger server to terminate ..\n");

    printf("cleanup clients connections ..\n");
    for (int k=0; k < MAX_CLIENTS; ++k)
    {
        CActiveSocket &client = clients[k];
        if (client.IsSocketValid())
            client.Close();
    }
    delete []clients;

    server_thread.join();
}
