/*---------------------------------------------------------------------------*/
/*                                                                           */
/* CSimpleSocket.cpp - CSimpleSocket Implementation                          */
/*                                                                           */
/* Author : Mark Carrier (mark@carrierlabs.com)                              */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/* Copyright (c) 2007-2009 CarrierLabs, LLC.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * 4. The name "CarrierLabs" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    mark@carrierlabs.com.
 *
 * THIS SOFTWARE IS PROVIDED BY MARK CARRIER ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL MARK CARRIER OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *----------------------------------------------------------------------------*/
#include "SimpleSocket.h"

#if defined(_LINUX) || defined(_DARWIN)
#include <sys/ioctl.h>
#endif

#define PRINT_CLOSE 0


CSimpleSocket::CSimpleSocket(CSocketType nType) :
    m_socket(INVALID_SOCKET),
    m_socketErrno(CSimpleSocket::SocketInvalidSocket),
    m_pBuffer(NULL), m_nBufferSize(0), m_nSocketDomain(AF_INET),
    m_nSocketType(SocketTypeInvalid), m_nBytesReceived(-1),
    m_nBytesSent(-1), m_nFlags(0),
    m_bIsBlocking(true),
    m_bIsServerSide(false),
    m_bPeerHasClosed(true)
{
    SetConnectTimeout(1, 0);    // default timeout for connection
    memset(&m_stRecvTimeout, 0, sizeof(struct timeval));
    memset(&m_stSendTimeout, 0, sizeof(struct timeval));
    memset(&m_stLinger, 0, sizeof(struct linger));

    switch(nType)
    {
        //----------------------------------------------------------------------
        // Declare socket type stream - TCP
        //----------------------------------------------------------------------
    case CSimpleSocket::SocketTypeTcp:
    {
        m_nSocketDomain = AF_INET;
        m_nSocketType = CSimpleSocket::SocketTypeTcp;
        break;
    }
    case CSimpleSocket::SocketTypeTcp6:
    {
        m_nSocketDomain = AF_INET6;
        m_nSocketType = CSimpleSocket::SocketTypeTcp6;
        break;
    }
    //----------------------------------------------------------------------
    // Declare socket type datagram - UDP
    //----------------------------------------------------------------------
    case CSimpleSocket::SocketTypeUdp:
    {
        m_nSocketDomain = AF_INET;
        m_nSocketType = CSimpleSocket::SocketTypeUdp;
        break;
    }
    case CSimpleSocket::SocketTypeUdp6:
    {
        m_nSocketDomain = AF_INET6;
        m_nSocketType = CSimpleSocket::SocketTypeUdp6;
        break;
    }
    //----------------------------------------------------------------------
    // Declare socket type raw Ethernet - Ethernet
    //----------------------------------------------------------------------
    case CSimpleSocket::SocketTypeRaw:
    {
#if defined(_LINUX) && !defined(_DARWIN)
        m_nSocketDomain = AF_PACKET;
        m_nSocketType = CSimpleSocket::SocketTypeRaw;
#endif
#ifdef _WIN32
        m_nSocketType = CSimpleSocket::SocketTypeInvalid;
#endif
        break;
    }
    case CSimpleSocket::SocketTypeInvalid:
    default:
        m_nSocketType = CSimpleSocket::SocketTypeInvalid;
        break;
    }
}

CSimpleSocket::CSimpleSocket(CSimpleSocket &socket)
{
    m_pBuffer = new uint8[socket.m_nBufferSize];
    m_nBufferSize = socket.m_nBufferSize;
    memcpy(m_pBuffer, socket.m_pBuffer, socket.m_nBufferSize);
}

CSimpleSocket::~CSimpleSocket()
{
    if (m_pBuffer != NULL)
    {
        delete [] m_pBuffer;
        m_pBuffer = NULL;
    }

    Close();
}

CSimpleSocket *CSimpleSocket::operator=(CSimpleSocket &socket)
{
    if (m_nBufferSize != socket.m_nBufferSize)
    {
        delete m_pBuffer;
        m_pBuffer = new uint8[socket.m_nBufferSize];
        m_nBufferSize = socket.m_nBufferSize;
        memcpy(m_pBuffer, socket.m_pBuffer, socket.m_nBufferSize);
    }

    return this;
}


//------------------------------------------------------------------------------
//
// ConnectTCP() -
//
//------------------------------------------------------------------------------
bool CSimpleSocket::ConnectTCP(const char *pAddr, uint16 nPort)
{
    bool           bRetVal = false;
    struct in_addr stIpAddress;

    // this is the Client
    m_bIsServerSide = false;

    //------------------------------------------------------------------
    // Preconnection setup that must be performed
    //------------------------------------------------------------------
    memset(&m_stServerSockaddr, 0, sizeof(m_stServerSockaddr));
    m_stServerSockaddr.sin_family = AF_INET;

    struct hostent *pHE = GETHOSTBYNAME(pAddr);
    if (pHE == NULL)
    {
#ifdef WIN32
        TranslateSocketError();
#else
        if (h_errno == HOST_NOT_FOUND)
        {
            SetSocketError(SocketInvalidAddress);
        }
#endif
        return bRetVal;
    }

    memcpy(&stIpAddress, pHE->h_addr_list[0], pHE->h_length);
    m_stServerSockaddr.sin_addr.s_addr = stIpAddress.s_addr;

    if ((int32)m_stServerSockaddr.sin_addr.s_addr == CSimpleSocket::SocketError)
    {
        TranslateSocketError();
        return bRetVal;
    }

    m_stServerSockaddr.sin_port = htons(nPort);

    //------------------------------------------------------------------
    // Connect to address "xxx.xxx.xxx.xxx"    (IPv4) address only.
    //
    //------------------------------------------------------------------
    m_timer.Initialize();
    m_timer.SetStartTime();

    if (connect(m_socket, (struct sockaddr*)&m_stServerSockaddr, sizeof(m_stServerSockaddr)) ==
            CSimpleSocket::SocketError)
    {
        //--------------------------------------------------------------
        // Get error value this might be a non-blocking socket so we
        // must first check.
        //--------------------------------------------------------------
        TranslateSocketError();

        //--------------------------------------------------------------
        // If the socket is non-blocking and the current socket error
        // is SocketEinprogress or SocketEwouldblock then poll connection
        // with select for designated timeout period.
        // Linux returns EINPROGRESS and Windows returns WSAEWOULDBLOCK.
        //--------------------------------------------------------------
        if ((IsNonblocking()) &&
                ((GetSocketError() == CSimpleSocket::SocketEwouldblock) ||
                 (GetSocketError() == CSimpleSocket::SocketEinprogress)))
        {
            bRetVal = Select( m_stConnectTimeout.tv_sec, m_stConnectTimeout.tv_usec );
        }
    }
    else
    {
        TranslateSocketError();
        bRetVal = true;
    }

    m_timer.SetEndTime();
    if ( bRetVal )
      m_bPeerHasClosed = false;

    return bRetVal;
}

//------------------------------------------------------------------------------
//
// ConnectUDP() -
//
//------------------------------------------------------------------------------
bool CSimpleSocket::ConnectUDP(const char *pAddr, uint16 nPort)
{
    bool           bRetVal = false;
    struct in_addr stIpAddress;

    // this is the Client
    m_bIsServerSide = false;

    //------------------------------------------------------------------
    // Pre-connection setup that must be performed
    //------------------------------------------------------------------
    memset(&m_stServerSockaddr, 0, sizeof(m_stServerSockaddr));
    m_stServerSockaddr.sin_family = AF_INET;

    struct hostent *pHE = GETHOSTBYNAME(pAddr);
    if (pHE == NULL)
    {
#ifdef WIN32
        TranslateSocketError();
#else
        if (h_errno == HOST_NOT_FOUND)
        {
            SetSocketError(SocketInvalidAddress);
        }
#endif
        return bRetVal;
    }

    memcpy(&stIpAddress, pHE->h_addr_list[0], pHE->h_length);
    m_stServerSockaddr.sin_addr.s_addr = stIpAddress.s_addr;

    if ((int32)m_stServerSockaddr.sin_addr.s_addr == CSimpleSocket::SocketError)
    {
        TranslateSocketError();
        return bRetVal;
    }

    m_stServerSockaddr.sin_port = htons(nPort);

    //------------------------------------------------------------------
    // Connect to address "xxx.xxx.xxx.xxx"    (IPv4) address only.
    //
    //------------------------------------------------------------------
    m_timer.Initialize();
    m_timer.SetStartTime();

    if (connect(m_socket, (struct sockaddr*)&m_stServerSockaddr, sizeof(m_stServerSockaddr)) != CSimpleSocket::SocketError)
    {
        bRetVal = true;
        m_bPeerHasClosed = false;
    }

    TranslateSocketError();

    m_timer.SetEndTime();

    return bRetVal;
}

//------------------------------------------------------------------------------
//
// ConnectRAW() -
//
//------------------------------------------------------------------------------
bool CSimpleSocket::ConnectRAW(const char *pAddr, uint16 nPort)
{
    bool           bRetVal = false;
    struct in_addr stIpAddress;

    // this is the Client
    m_bIsServerSide = false;

    //------------------------------------------------------------------
    // Pre-connection setup that must be performed
    //------------------------------------------------------------------
    memset(&m_stServerSockaddr, 0, sizeof(m_stServerSockaddr));
    m_stServerSockaddr.sin_family = AF_INET;

    struct hostent *pHE = GETHOSTBYNAME(pAddr);
    if (pHE == NULL)
    {
#ifdef WIN32
        TranslateSocketError();
#else
        if (h_errno == HOST_NOT_FOUND)
        {
            SetSocketError(SocketInvalidAddress);
        }
#endif
        return bRetVal;
    }

    memcpy(&stIpAddress, pHE->h_addr_list[0], pHE->h_length);
    m_stServerSockaddr.sin_addr.s_addr = stIpAddress.s_addr;

    if ((int32)m_stServerSockaddr.sin_addr.s_addr == CSimpleSocket::SocketError)
    {
        TranslateSocketError();
        return bRetVal;
    }

    m_stServerSockaddr.sin_port = htons(nPort);

    //------------------------------------------------------------------
    // Connect to address "xxx.xxx.xxx.xxx"    (IPv4) address only.
    //
    //------------------------------------------------------------------
    m_timer.Initialize();
    m_timer.SetStartTime();

    if (connect(m_socket, (struct sockaddr*)&m_stServerSockaddr, sizeof(m_stServerSockaddr)) != CSimpleSocket::SocketError)
    {
        bRetVal = true;
        m_bPeerHasClosed = false;
    }

    TranslateSocketError();

    m_timer.SetEndTime();

    return bRetVal;
}


//------------------------------------------------------------------------------
//
// Initialize() - Initialize socket class
//
//------------------------------------------------------------------------------
bool CSimpleSocket::Initialize()
{
    errno = CSimpleSocket::SocketSuccess;

#ifdef WIN32
    //-------------------------------------------------------------------------
    // Data structure containing general Windows Sockets Info
    //-------------------------------------------------------------------------
    // use WinSock 2.2
    // see https://msdn.microsoft.com/en-us/library/windows/desktop/ms742213(v=vs.85).aspx
    // Windows Sockets version 2.2 is supported on
    //   Windows Server 2008,
    //   Windows Vista,
    //   Windows Server 2003,
    //   Windows XP,
    //   Windows 2000,
    //   Windows NT 4.0 with Service Pack 4 (SP4) and later,
    //   Windows Me,
    //   Windows 98,
    //   Windows 95 OSR2,
    //   Windows 95 with the Windows Socket 2 Update
    memset(&m_hWSAData, 0, sizeof(m_hWSAData));
    WSAStartup(MAKEWORD(2, 2), &m_hWSAData);
#endif

    //-------------------------------------------------------------------------
    // Create the basic Socket Handle
    //-------------------------------------------------------------------------
    m_bPeerHasClosed = true;
    m_timer.Initialize();
    m_timer.SetStartTime();
    m_socket = socket(m_nSocketDomain, m_nSocketType, 0);
    m_timer.SetEndTime();

    TranslateSocketError();

    return (IsSocketValid());
}


//------------------------------------------------------------------------------
//
// Open() - Create a connection to a specified address on a specified port
//
//------------------------------------------------------------------------------
bool CSimpleSocket::Open(const char *pAddr, uint16 nPort)
{
    bool bRetVal = false;
    m_bIsServerSide = false;
    m_bPeerHasClosed = true;

    if (IsSocketValid() == false)
    {
        SetSocketError(CSimpleSocket::SocketInvalidSocket);
        return bRetVal;
    }

    if (pAddr == NULL)
    {
        SetSocketError(CSimpleSocket::SocketInvalidAddress);
        return bRetVal;
    }

    if (nPort == 0)
    {
        SetSocketError(CSimpleSocket::SocketInvalidPort);
        return bRetVal;
    }

    switch (m_nSocketType)
    {
    case CSimpleSocket::SocketTypeTcp :
    {
        bRetVal = ConnectTCP(pAddr, nPort);
        break;
    }
    case CSimpleSocket::SocketTypeUdp :
    {
        bRetVal = ConnectUDP(pAddr, nPort);
        break;
    }
    case CSimpleSocket::SocketTypeRaw :
        break;
    case CSimpleSocket::SocketTypeInvalid:
    case CSimpleSocket::SocketTypeTcp6:
    case CSimpleSocket::SocketTypeUdp6:
    default:
        break;
    }

    //--------------------------------------------------------------------------
    // If successful then create a local copy of the address and port
    //--------------------------------------------------------------------------
    if (bRetVal)
    {
        socklen_t nSockLen = sizeof(struct sockaddr);

        memset(&m_stServerSockaddr, 0, nSockLen);
        getpeername(m_socket, (struct sockaddr *)&m_stServerSockaddr, &nSockLen);

        nSockLen = sizeof(struct sockaddr);
        memset(&m_stClientSockaddr, 0, nSockLen);
        getsockname(m_socket, (struct sockaddr *)&m_stClientSockaddr, &nSockLen);

        SetSocketError(SocketSuccess);
    }

    return bRetVal;
}


//------------------------------------------------------------------------------
//
// BindInterface()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::BindInterface(const char *pInterface)
{
    bool           bRetVal = false;
    struct in_addr stInterfaceAddr;

    if (GetMulticast() == true)
    {
        if (pInterface)
        {
            stInterfaceAddr.s_addr= inet_addr(pInterface);
            if (SETSOCKOPT(m_socket, IPPROTO_IP, IP_MULTICAST_IF, &stInterfaceAddr, sizeof(stInterfaceAddr)) == SocketSuccess)
            {
                bRetVal = true;
            }
        }
    }
    else
    {
        SetSocketError(CSimpleSocket::SocketProtocolError);
    }

    return bRetVal;
}


//------------------------------------------------------------------------------
//
// SetMulticast()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::SetMulticast(bool bEnable, uint8 multicastTTL)
{
    bool bRetVal = false;

    if (GetSocketType() == CSimpleSocket::SocketTypeUdp)
    {
        m_bIsMulticast = bEnable;
        if (SETSOCKOPT(m_socket, IPPROTO_IP, IP_MULTICAST_TTL, (void *)&multicastTTL, sizeof(multicastTTL)) == SocketError)
        {
            TranslateSocketError();
            bRetVal = false;
        }
        else
        {
            bRetVal = true;
        }
    }
    else
    {
        m_socketErrno = CSimpleSocket::SocketProtocolError;
    }

    return bRetVal;
}


//------------------------------------------------------------------------------
//
// SetSocketDscp()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::SetSocketDscp(int32 nDscp)
{
    bool  bRetVal = true;
    int32 nTempVal = nDscp;

    nTempVal <<= 4;
    nTempVal /= 4;

    if (IsSocketValid())
    {
        if (SETSOCKOPT(m_socket, IPPROTO_IP, IP_TOS, &nTempVal, sizeof(nTempVal)) == SocketError)
        {
            TranslateSocketError();
            bRetVal = false;
        }
    }

    return bRetVal;
}


//------------------------------------------------------------------------------
//
// GetSocketDscp()
//
//------------------------------------------------------------------------------
int32 CSimpleSocket::GetSocketDscp(void)
{
    int32      nTempVal = 0;
    socklen_t  nLen = 0;

    if (IsSocketValid())
    {
        if (GETSOCKOPT(m_socket, IPPROTO_IP, IP_TOS, &nTempVal, &nLen) == SocketError)
        {
            TranslateSocketError();
        }

        nTempVal *= 4;
        nTempVal >>= 4;
    }

    return nTempVal;
}


/// Returns clients Internet host address as a string in standard numbers-and-dots notation.
///  @return NULL if invalid
const char * CSimpleSocket::GetClientAddr() const
{
    return inet_ntoa(m_stClientSockaddr.sin_addr);
}

/// Returns the port number on which the client is connected.
///  @return client port number.
uint16 CSimpleSocket::GetClientPort() const
{
    return m_stClientSockaddr.sin_port;
}

/// Returns server Internet host address as a string in standard numbers-and-dots notation.
///  @return NULL if invalid
const char * CSimpleSocket::GetServerAddr() const
{
    return inet_ntoa(m_stServerSockaddr.sin_addr);
}

/// Returns the port number on which the server is connected.
///  @return server port number.
uint16 CSimpleSocket::GetServerPort() const
{
    return ntohs(m_stServerSockaddr.sin_port);
}


/// Returns if this is the Server side of the connection
bool CSimpleSocket::IsServerSide() const
{
  return m_bIsServerSide;
}

/// Returns local Internet host address as a string in standard numbers-and-dots notation.
///  @return NULL if invalid
const char * CSimpleSocket::GetLocalAddr() const
{
  return (m_bIsServerSide) ? GetServerAddr() : GetClientAddr();
}

/// Returns the port number on which the local socket is connected.
///  @return client port number.
uint16 CSimpleSocket::GetLocalPort() const
{
  return (m_bIsServerSide) ? GetServerPort() : GetClientPort();
}


/// Returns Peer's Internet host address as a string in standard numbers-and-dots notation.
///  @return NULL if invalid
const char * CSimpleSocket::GetPeerAddr() const
{
  return (m_bIsServerSide) ? GetClientAddr() : GetServerAddr();
}

/// Returns the port number on which the peer is connected.
///  @return client port number.
uint16 CSimpleSocket::GetPeerPort() const
{
  return (m_bIsServerSide) ? GetClientPort() : GetServerPort();
}


//------------------------------------------------------------------------------
//
// GetWindowSize()
//
//------------------------------------------------------------------------------
uint32 CSimpleSocket::GetWindowSize(uint32 nOptionName)
{
    uint32 nTcpWinSize = 0;

    //-------------------------------------------------------------------------
    // no socket given, return system default allocate our own new socket
    //-------------------------------------------------------------------------
    if (m_socket != CSimpleSocket::SocketError)
    {
        socklen_t nLen = sizeof(nTcpWinSize);

        //---------------------------------------------------------------------
        // query for buffer size
        //---------------------------------------------------------------------
        GETSOCKOPT(m_socket, SOL_SOCKET, nOptionName, &nTcpWinSize, &nLen);
        TranslateSocketError();
    }
    else
    {
        SetSocketError(CSimpleSocket::SocketInvalidSocket);
    }

    return nTcpWinSize;
}


//------------------------------------------------------------------------------
//
// SetWindowSize()
//
//------------------------------------------------------------------------------
uint32 CSimpleSocket::SetWindowSize(uint32 nOptionName, uint32 nWindowSize)
{
    //-------------------------------------------------------------------------
    // no socket given, return system default allocate our own new socket
    //-------------------------------------------------------------------------
    if (m_socket != CSimpleSocket::SocketError)
    {
        int nRetVal = SETSOCKOPT(m_socket, SOL_SOCKET, nOptionName, &nWindowSize, sizeof(nWindowSize));
        TranslateSocketError();
        if ( !nRetVal )
            return GetWindowSize(nOptionName);
    }
    else
    {
        SetSocketError(CSimpleSocket::SocketInvalidSocket);
    }

    return nWindowSize;
}


//------------------------------------------------------------------------------
//
// DisableNagleAlgorithm()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::DisableNagleAlgoritm()
{
    bool  bRetVal = false;
    int32 nTcpNoDelay = 1;

    //----------------------------------------------------------------------
    // Set TCP NoDelay flag to true
    //----------------------------------------------------------------------
    if (SETSOCKOPT(m_socket, IPPROTO_TCP, TCP_NODELAY, &nTcpNoDelay, sizeof(int32)) == 0)
    {
        bRetVal = true;
    }

    TranslateSocketError();

    return bRetVal;
}


//------------------------------------------------------------------------------
//
// EnableNagleAlgorithm()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::EnableNagleAlgoritm()
{
    bool  bRetVal = false;
    int32 nTcpNoDelay = 0;

    //----------------------------------------------------------------------
    // Set TCP NoDelay flag to false
    //----------------------------------------------------------------------
    if (SETSOCKOPT(m_socket, IPPROTO_TCP, TCP_NODELAY, &nTcpNoDelay, sizeof(int32)) == 0)
    {
        bRetVal = true;
    }

    TranslateSocketError();

    return bRetVal;
}


/// Set object socket handle to that specified as parameter
///  @param socket value of socket descriptor
void CSimpleSocket::SetSocketHandle(SOCKET socket, bool bIsServerSide )
{
    m_socket = socket;
    m_bIsServerSide = bIsServerSide;
    m_bPeerHasClosed = false;
}


//------------------------------------------------------------------------------
//
// Send() - Send data on a valid socket
//
//------------------------------------------------------------------------------
int32 CSimpleSocket::Send(const uint8 *pBuf, size_t bytesToSend)
{
    SetSocketError(SocketSuccess);
    m_nBytesSent = 0;

    switch(m_nSocketType)
    {
    case CSimpleSocket::SocketTypeTcp:
    {
        if (IsSocketValid())
        {
            if ((bytesToSend > 0) && (pBuf != NULL))
            {
                m_timer.Initialize();
                m_timer.SetStartTime();

                //---------------------------------------------------------
                // Check error condition and attempt to resend if call
                // was interrupted by a signal.
                //---------------------------------------------------------
                do
                {
                    m_nBytesSent = SEND(m_socket, pBuf, bytesToSend, 0);
                    TranslateSocketError();
                } while (GetSocketError() == CSimpleSocket::SocketInterrupted);

                m_timer.SetEndTime();
            }
        }
        break;
    }
    case CSimpleSocket::SocketTypeUdp:
    {
        if (IsSocketValid())
        {
            if ((bytesToSend > 0) && (pBuf != NULL))
            {
                m_timer.Initialize();
                m_timer.SetStartTime();

                //---------------------------------------------------------
                // Check error condition and attempt to resend if call
                // was interrupted by a signal.
                //---------------------------------------------------------
                //                    if (GetMulticast())
                //                    {
                //                        do
                //                        {
                //                            m_nBytesSent = SENDTO(m_socket, pBuf, bytesToSend, 0, (const sockaddr *)&m_stMulticastGroup,
                //                                                  sizeof(m_stMulticastGroup));
                //                            TranslateSocketError();
                //                        } while (GetSocketError() == CSimpleSocket::SocketInterrupted);
                //                    }
                //                    else
                {
                    do
                    {
                        m_nBytesSent = SENDTO(m_socket, pBuf, bytesToSend, 0,
                                              (const sockaddr *)&m_stServerSockaddr,
                                              sizeof(m_stServerSockaddr));
                        TranslateSocketError();
                    } while (GetSocketError() == CSimpleSocket::SocketInterrupted);
                }

                m_timer.SetEndTime();
            }
        }
        break;
    }
    case CSimpleSocket::SocketTypeInvalid:
    case CSimpleSocket::SocketTypeTcp6:
    case CSimpleSocket::SocketTypeUdp6:
    case CSimpleSocket::SocketTypeRaw:
    default:
        break;
    }

    return m_nBytesSent;
}


//------------------------------------------------------------------------------
//
// Close() - Close socket and free up any memory allocated for the socket
//
//------------------------------------------------------------------------------
bool CSimpleSocket::Close(void)
{
    bool bRetVal = false;

    //--------------------------------------------------------------------------
    // delete internal buffer
    //--------------------------------------------------------------------------
    if (m_pBuffer != NULL)
    {
        delete [] m_pBuffer;
        m_pBuffer = NULL;
    }

    //--------------------------------------------------------------------------
    // if socket handle is currently valid, close and then invalidate
    //--------------------------------------------------------------------------
    if (IsSocketValid())
    {
        if (CLOSE(m_socket) != CSimpleSocket::SocketError)
        {
            m_socket = INVALID_SOCKET;
            bRetVal = true;
        }
    }

    TranslateSocketError();

    return bRetVal;
}


//------------------------------------------------------------------------------
//
// Shtudown()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::Shutdown(CShutdownMode nShutdown)
{
    CSocketError nRetVal = SocketEunknown;

    nRetVal = (CSocketError)shutdown(m_socket, CSimpleSocket::Sends);
    TranslateSocketError();

    return (nRetVal == CSimpleSocket::SocketSuccess) ? true: false;
}


bool CSimpleSocket::Select(void)
{
    return Select(0,0);
}


//------------------------------------------------------------------------------
//
// Flush()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::Flush()
{
    int32 nTcpNoDelay = 1;
    int32 nCurFlags = 0;
    uint8 tmpbuf = 0;
    bool  bRetVal = false;

    //--------------------------------------------------------------------------
    // Get the current setting of the TCP_NODELAY flag.
    //--------------------------------------------------------------------------
    if (GETSOCKOPT(m_socket, IPPROTO_TCP, TCP_NODELAY, &nCurFlags, sizeof(int32)) == 0)
    {
        //----------------------------------------------------------------------
        // Set TCP NoDelay flag
        //----------------------------------------------------------------------
        if (SETSOCKOPT(m_socket, IPPROTO_TCP, TCP_NODELAY, &nTcpNoDelay, sizeof(int32)) == 0)
        {
            //------------------------------------------------------------------
            // Send empty byte stream to flush the TCP send buffer
            //------------------------------------------------------------------
            if (Send(&tmpbuf, 0) != CSimpleSocket::SocketError)
            {
                bRetVal = true;
            }

            TranslateSocketError();
        }

        //----------------------------------------------------------------------
        // Reset the TCP_NODELAY flag to original state.
        //----------------------------------------------------------------------
        SETSOCKOPT(m_socket, IPPROTO_TCP, TCP_NODELAY, &nCurFlags, sizeof(int32));
    }

    return bRetVal;
}


//------------------------------------------------------------------------------
//
// Writev -
//
//------------------------------------------------------------------------------
int32 CSimpleSocket::Writev(const struct iovec *pVector, size_t nCount)
{
    int32 nBytes     = 0;
    int32 nBytesSent = 0;
    int32 i          = 0;

    //--------------------------------------------------------------------------
    // Send each buffer as a separate send, windows does not support this
    // function call.
    //--------------------------------------------------------------------------
    for (i = 0; i < (int32)nCount; i++)
    {
        if ((nBytes = Send((uint8 *)pVector[i].iov_base, pVector[i].iov_len)) == CSimpleSocket::SocketError)
        {
            break;
        }

        nBytesSent += nBytes;
    }

    if (i > 0)
    {
        Flush();
    }

    return nBytesSent;
}


//------------------------------------------------------------------------------
//
// Send() - Send data on a valid socket via a vector of buffers.
//
//------------------------------------------------------------------------------
int32 CSimpleSocket::Send(const struct iovec *sendVector, int32 nNumItems)
{
    SetSocketError(SocketSuccess);
    m_nBytesSent = 0;

    if ((m_nBytesSent = WRITEV(m_socket, sendVector, nNumItems)) == CSimpleSocket::SocketError)
    {
        TranslateSocketError();
    }

    return m_nBytesSent;
}


//------------------------------------------------------------------------------
//
// SetConnectTimeout()
//
//------------------------------------------------------------------------------
void CSimpleSocket::SetConnectTimeout(int32 nConnectTimeoutSec, int32 nConnectTimeoutUsec)
{
    m_stConnectTimeout.tv_sec = nConnectTimeoutSec;
    m_stConnectTimeout.tv_usec = nConnectTimeoutUsec;
}


//------------------------------------------------------------------------------
//
// SetReceiveTimeout()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::SetReceiveTimeout(int32 nRecvTimeoutSec, int32 nRecvTimeoutUsec)
{
    bool bRetVal = true;

#ifdef WIN32
    uint32 nMillis = nRecvTimeoutSec * 1000 + nRecvTimeoutUsec / 1000;
    if ( nMillis == 0 && ( nRecvTimeoutUsec > 0 || nRecvTimeoutSec > 0 ) )
        nMillis = 1;
    nRecvTimeoutSec = nMillis / 1000;
    nRecvTimeoutUsec = nMillis % 1000;
#endif

    memset(&m_stRecvTimeout, 0, sizeof(struct timeval));
    m_stRecvTimeout.tv_sec = nRecvTimeoutSec;
    m_stRecvTimeout.tv_usec = nRecvTimeoutUsec;

    //--------------------------------------------------------------------------
    // Sanity check to make sure the options are supported!
    //--------------------------------------------------------------------------
#ifdef WIN32
    // see https://msdn.microsoft.com/en-us/library/windows/desktop/ms740476(v=vs.85).aspx
    if (SETSOCKOPT(m_socket, SOL_SOCKET, SO_RCVTIMEO, &nMillis,
                   sizeof(nMillis)) == CSimpleSocket::SocketError)
#else
    if (SETSOCKOPT(m_socket, SOL_SOCKET, SO_RCVTIMEO, &m_stRecvTimeout,
                   sizeof(struct timeval)) == CSimpleSocket::SocketError)
#endif
    {
        bRetVal = false;
        TranslateSocketError();
    }

    return bRetVal;
}


//------------------------------------------------------------------------------
//
// SetSendTimeout()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::SetSendTimeout(int32 nSendTimeoutSec, int32 nSendTimeoutUsec)
{
    bool bRetVal = true;

#ifdef WIN32
    uint32 nMillis = nSendTimeoutSec * 1000 + nSendTimeoutUsec / 1000;
    if ( nMillis == 0 && ( nSendTimeoutUsec > 0 || nSendTimeoutSec > 0 ) )
        nMillis = 1;
    nSendTimeoutSec = nMillis / 1000;
    nSendTimeoutUsec = nMillis % 1000;
#endif

    memset(&m_stSendTimeout, 0, sizeof(struct timeval));
    m_stSendTimeout.tv_sec = nSendTimeoutSec;
    m_stSendTimeout.tv_usec = nSendTimeoutUsec;

    //--------------------------------------------------------------------------
    // Sanity check to make sure the options are supported!
    //--------------------------------------------------------------------------
#ifdef WIN32
    // see https://msdn.microsoft.com/en-us/library/windows/desktop/ms740476(v=vs.85).aspx
    if (SETSOCKOPT(m_socket, SOL_SOCKET, SO_SNDTIMEO, &nMillis,
                   sizeof(nMillis)) == CSimpleSocket::SocketError)
#else
    if (SETSOCKOPT(m_socket, SOL_SOCKET, SO_SNDTIMEO, &m_stSendTimeout,
                   sizeof(struct timeval)) == CSimpleSocket::SocketError)
#endif
    {
        bRetVal = false;
        TranslateSocketError();
    }

    return bRetVal;
}


//------------------------------------------------------------------------------
//
// SetOptionReuseAddr()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::SetOptionReuseAddr()
{
    bool  bRetVal = false;
    int32 nReuse  = IPTOS_LOWDELAY;

    if (SETSOCKOPT(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&nReuse, sizeof(int32)) == 0)
    {
        bRetVal = true;
    }

    TranslateSocketError();

    return bRetVal;
}


//------------------------------------------------------------------------------
//
// SetOptionLinger()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::SetOptionLinger(bool bEnable, uint16 nTimeInSeconds)
{
    bool bRetVal = false;

    m_stLinger.l_onoff = (bEnable == true) ? 1: 0;
    m_stLinger.l_linger = nTimeInSeconds;

    if (SETSOCKOPT(m_socket, SOL_SOCKET, SO_LINGER, &m_stLinger, sizeof(m_stLinger)) == 0)
    {
        bRetVal = true;
    }

    TranslateSocketError();

    return bRetVal;
}


//------------------------------------------------------------------------------
//
// Receive() - Attempts to receive a block of data on an established
//             connection.    Data is received in an internal buffer managed
//             by the class.  This buffer is only valid until the next call
//             to Receive(), a call to Close(), or until the object goes out
//             of scope.
//
//------------------------------------------------------------------------------
int32 CSimpleSocket::Receive(int32 nMaxBytes, uint8 * pBuffer )
{
    m_nBytesReceived = 0;

    //--------------------------------------------------------------------------
    // If the socket is invalid then return false.
    //--------------------------------------------------------------------------
    if (IsSocketValid() == false)
    {
        return m_nBytesReceived;
    }

    uint8 * pWorkBuffer = pBuffer;
    if ( pBuffer == NULL )
    {
        //--------------------------------------------------------------------------
        // Free existing buffer and allocate a new buffer the size of
        // nMaxBytes.
        //--------------------------------------------------------------------------
        if ((m_pBuffer != NULL) && (nMaxBytes != m_nBufferSize))
        {
            delete [] m_pBuffer;
            m_pBuffer = NULL;
        }

        //--------------------------------------------------------------------------
        // Allocate a new internal buffer to receive data.
        //--------------------------------------------------------------------------
        if (m_pBuffer == NULL)
        {
            m_nBufferSize = nMaxBytes;
            m_pBuffer = new uint8[nMaxBytes];
        }

        pWorkBuffer = m_pBuffer;
    }

    SetSocketError(SocketSuccess);

    m_timer.Initialize();
    m_timer.SetStartTime();

    switch (m_nSocketType)
    {
        //----------------------------------------------------------------------
        // If zero bytes are received, then return.  If SocketERROR is
        // received, free buffer and return CSocket::SocketError (-1) to caller.
        //----------------------------------------------------------------------
    case CSimpleSocket::SocketTypeTcp:
    {
        do
        {
            m_nBytesReceived = RECV(m_socket, (pWorkBuffer + m_nBytesReceived),
                                    nMaxBytes, m_nFlags);
            if ( 0 == m_nBytesReceived )
            {
#if PRINT_CLOSE
              fprintf(stderr, "CSimpleSocket::Receive() = 0 Bytes ==> peer closed connection!\n");
#endif
              m_bPeerHasClosed = true;
            }
            TranslateSocketError();
        } while ((GetSocketError() == CSimpleSocket::SocketInterrupted));

        break;
    }
    case CSimpleSocket::SocketTypeUdp:
    {
        uint32 srcSize;

        srcSize = sizeof(struct sockaddr_in);

        if (GetMulticast() == true)
        {
            do
            {
                m_nBytesReceived = RECVFROM(m_socket, pWorkBuffer, nMaxBytes, 0,
                                            &m_stMulticastGroup, &srcSize);
                TranslateSocketError();
            } while (GetSocketError() == CSimpleSocket::SocketInterrupted);
        }
        else
        {
            do
            {
                m_nBytesReceived = RECVFROM(m_socket, pWorkBuffer, nMaxBytes, 0,
                                            &m_stClientSockaddr, &srcSize);
                TranslateSocketError();
            } while (GetSocketError() == CSimpleSocket::SocketInterrupted);
        }

        break;
    }
    case CSimpleSocket::SocketTypeInvalid:
    case CSimpleSocket::SocketTypeTcp6:
    case CSimpleSocket::SocketTypeUdp6:
    case CSimpleSocket::SocketTypeRaw:
    default:
        break;
    }

    m_timer.SetEndTime();
    TranslateSocketError();

    //--------------------------------------------------------------------------
    // If we encounter an error translate the error code and return.  One
    // possible error code could be EAGAIN (EWOULDBLOCK) if the socket is
    // non-blocking.  This does not mean there is an error, but no data is
    // yet available on the socket.
    //--------------------------------------------------------------------------
    if (m_nBytesReceived == CSimpleSocket::SocketError)
    {
        if (m_pBuffer != NULL)
        {
            delete [] m_pBuffer;
            m_pBuffer = NULL;
        }
    }

    return m_nBytesReceived;
}


/// Attempts to return the number of Bytes waiting at next Receive()
/// @return number of bytes ready for Receive()
/// @return of -1 means that an error has occurred.
int32 CSimpleSocket::GetNumReceivableBytes()
{
    //--------------------------------------------------------------------------
    // If the socket is invalid then return false.
    //--------------------------------------------------------------------------
    if (IsSocketValid() == false)
    {
        return -1;
    }

#if WIN32
    u_long numBytesInSocket = 0;
    if ( ::ioctlsocket(m_socket, FIONREAD, &numBytesInSocket) < 0 )
        return -1;

    return (int32)numBytesInSocket;
#elif defined(_LINUX) || defined(_DARWIN)
    int32 numBytesInSocket = 0;
    if ( ioctl(m_socket, FIONREAD, (char*)&numBytesInSocket) < 0 )
        return -1;

    return numBytesInSocket;
#else
#error missing implementation for GetNumReceivableBytes()
#endif
}

//------------------------------------------------------------------------------
//
// SetNonblocking()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::SetNonblocking(void)
{
    int32  nCurFlags;

#if WIN32
    nCurFlags = 1;

    if (ioctlsocket(m_socket, FIONBIO, (ULONG *)&nCurFlags) != 0)
    {
        TranslateSocketError();
        return false;
    }
#else
    if ((nCurFlags = fcntl(m_socket, F_GETFL)) < 0)
    {
        TranslateSocketError();
        return false;
    }

    nCurFlags |= O_NONBLOCK;

    if (fcntl(m_socket, F_SETFL, nCurFlags) != 0)
    {
        TranslateSocketError();
        return false;
    }
#endif

    m_bIsBlocking = false;

    return true;
}


//------------------------------------------------------------------------------
//
// SetBlocking()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::SetBlocking(void)
{
    int32 nCurFlags;

#if WIN32
    nCurFlags = 0;

    if (ioctlsocket(m_socket, FIONBIO, (ULONG *)&nCurFlags) != 0)
    {
        return false;
    }
#else
    if ((nCurFlags = fcntl(m_socket, F_GETFL)) < 0)
    {
        TranslateSocketError();
        return false;
    }

    nCurFlags &= (~O_NONBLOCK);

    if (fcntl(m_socket, F_SETFL, nCurFlags) != 0)
    {
        TranslateSocketError();
        return false;
    }
#endif
    m_bIsBlocking = true;

    return true;
}


//------------------------------------------------------------------------------
//
// SendFile() - stands-in for system provided sendfile
//
//------------------------------------------------------------------------------
int32 CSimpleSocket::SendFile(int32 nOutFd, int32 nInFd, off_t *pOffset, int32 nCount)
{
    int32  nOutCount = CSimpleSocket::SocketError;

    static char szData[SOCKET_SENDFILE_BLOCKSIZE];
    int32       nInCount = 0;

    if (lseek(nInFd, *pOffset, SEEK_SET) == -1)
    {
        return -1;
    }

    while (nOutCount < nCount)
    {
        nInCount = (nCount - nOutCount) < SOCKET_SENDFILE_BLOCKSIZE ? (nCount - nOutCount) : SOCKET_SENDFILE_BLOCKSIZE;

        if ((read(nInFd, szData, nInCount)) != (int32)nInCount)
        {
            return -1;
        }

        if ((SEND(nOutFd, szData, nInCount, 0)) != (int32)nInCount)
        {
            return -1;
        }

        nOutCount += nInCount;
    }

    *pOffset += nOutCount;

    TranslateSocketError();

    return nOutCount;
}


bool CSimpleSocket::IsSocketValid(void)
{
#if PRINT_CLOSE
  if ( m_socket == SocketError )
    fprintf(stderr, "reporting CSimpleSocket::IsSocketValid() == false.\n" );
#endif
    return (m_socket != SocketError);
}


bool CSimpleSocket::IsSocketPeerClosed(void)
{
#if PRINT_CLOSE
  if ( m_bPeerHasClosed )
    fprintf(stderr, "reporting CSimpleSocket::IsSocketPeerClosed() == true.\n" );
#endif
  return m_bPeerHasClosed;
}


//------------------------------------------------------------------------------
//
// TranslateSocketError() -
//
//------------------------------------------------------------------------------
void CSimpleSocket::TranslateSocketError(void)
{
#if defined(_LINUX) || defined(_DARWIN)
    switch (errno)
    {
    case EXIT_SUCCESS:
        SetSocketError(CSimpleSocket::SocketSuccess);
        break;
    case ENOTCONN:
        SetSocketError(CSimpleSocket::SocketNotconnected);
        break;
    case ENOTSOCK:
    case EBADF:
    case EACCES:
    case EAFNOSUPPORT:
    case EMFILE:
    case ENFILE:
    case ENOBUFS:
    case ENOMEM:
    case EPROTONOSUPPORT:
    case EPIPE:
    case EOPNOTSUPP:
        SetSocketError(CSimpleSocket::SocketInvalidSocket);
        break;
    case ECONNREFUSED :
        SetSocketError(CSimpleSocket::SocketConnectionRefused);
        break;
    case ETIMEDOUT:
        SetSocketError(CSimpleSocket::SocketTimedout);
        break;
    case EINPROGRESS:
        SetSocketError(CSimpleSocket::SocketEinprogress);
        break;
    case EWOULDBLOCK:
        //        case EAGAIN:
        SetSocketError(CSimpleSocket::SocketEwouldblock);
        break;
    case EINTR:
        SetSocketError(CSimpleSocket::SocketInterrupted);
        break;
    case ECONNABORTED:
        SetSocketError(CSimpleSocket::SocketConnectionAborted);
        break;
    case EINVAL:
    case EPROTO:
        SetSocketError(CSimpleSocket::SocketProtocolError);
        break;
    case EPERM:
        SetSocketError(CSimpleSocket::SocketFirewallError);
        break;
    case EFAULT:
        SetSocketError(CSimpleSocket::SocketInvalidSocketBuffer);
        break;
    case ECONNRESET:
    case ENOPROTOOPT:
        SetSocketError(CSimpleSocket::SocketConnectionReset);
        break;
    case EADDRINUSE:
        SetSocketError(CSimpleSocket::SocketAddressInUse);
        break;
    default:
        SetSocketError(CSimpleSocket::SocketEunknown);
        break;
    }
#elif defined(WIN32)
    int32 nError = WSAGetLastError();
    switch (nError)
    {
    case EXIT_SUCCESS:
        SetSocketError(CSimpleSocket::SocketSuccess);
        break;
    case WSAEBADF:
    case WSAENOTCONN:
        SetSocketError(CSimpleSocket::SocketNotconnected);
        break;
    case WSAEINTR:
        SetSocketError(CSimpleSocket::SocketInterrupted);
        break;
    case WSAEACCES:
    case WSAEAFNOSUPPORT:
    case WSAEINVAL:
    case WSAEMFILE:
    case WSAENOBUFS:
    case WSAEPROTONOSUPPORT:
        SetSocketError(CSimpleSocket::SocketInvalidSocket);
        break;
    case WSAECONNREFUSED :
        SetSocketError(CSimpleSocket::SocketConnectionRefused);
        break;
    case WSAETIMEDOUT:
        SetSocketError(CSimpleSocket::SocketTimedout);
        break;
    case WSAEINPROGRESS:
        SetSocketError(CSimpleSocket::SocketEinprogress);
        break;
    case WSAECONNABORTED:
        SetSocketError(CSimpleSocket::SocketConnectionAborted);
        break;
    case WSAEWOULDBLOCK:
        SetSocketError(CSimpleSocket::SocketEwouldblock);
        break;
    case WSAENOTSOCK:
        SetSocketError(CSimpleSocket::SocketInvalidSocket);
        break;
    case WSAECONNRESET:
        SetSocketError(CSimpleSocket::SocketConnectionReset);
        break;
    case WSANO_DATA:
        SetSocketError(CSimpleSocket::SocketInvalidAddress);
        break;
    case WSAEADDRINUSE:
        SetSocketError(CSimpleSocket::SocketAddressInUse);
        break;
    case WSAEFAULT:
        SetSocketError(CSimpleSocket::SocketInvalidPointer);
        break;
    default:
        SetSocketError(CSimpleSocket::SocketEunknown);
        break;
    }
#else
#error unsupported platform!
#endif
}

//------------------------------------------------------------------------------
//
// DescribeError()
//
//------------------------------------------------------------------------------

const char *CSimpleSocket::DescribeError(CSocketError err)
{
    switch (err) {
        case CSimpleSocket::SocketError:
            return "Generic socket error translates to error below.";
        case CSimpleSocket::SocketSuccess:
            return "No socket error.";
        case CSimpleSocket::SocketInvalidSocket:
            return "Invalid socket handle.";
        case CSimpleSocket::SocketInvalidAddress:
            return "Invalid destination address specified.";
        case CSimpleSocket::SocketInvalidPort:
            return "Invalid destination port specified.";
        case CSimpleSocket::SocketConnectionRefused:
            return "No server is listening at remote address.";
        case CSimpleSocket::SocketTimedout:
            return "Timed out while attempting operation.";
        case CSimpleSocket::SocketEwouldblock:
            return "Operation would block if socket were blocking.";
        case CSimpleSocket::SocketNotconnected:
            return "Currently not connected.";
        case CSimpleSocket::SocketEinprogress:
            return "Socket is non-blocking and the connection cannot be completed immediately";
        case CSimpleSocket::SocketInterrupted:
            return "Call was interrupted by a signal that was caught before a valid connection arrived.";
        case CSimpleSocket::SocketConnectionAborted:
            return "The connection has been aborted.";
        case CSimpleSocket::SocketProtocolError:
            return "Invalid protocol for operation.";
        case CSimpleSocket::SocketFirewallError:
            return "Firewall rules forbid connection.";
        case CSimpleSocket::SocketInvalidSocketBuffer:
            return "The receive buffer point outside the process's address space.";
        case CSimpleSocket::SocketConnectionReset:
            return "Connection was forcibly closed by the remote host.";
        case CSimpleSocket::SocketAddressInUse:
            return "Address already in use.";
        case CSimpleSocket::SocketInvalidPointer:
            return "Pointer type supplied as argument is invalid.";
        case CSimpleSocket::SocketEunknown:
            return "Unknown error";
        default:
            return "No such CSimpleSocket error";
    }
}

//------------------------------------------------------------------------------
//
// Select()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::Select(int32 nTimeoutSec, int32 nTimeoutUSec, bool bAwakeWhenReadable, bool bAwakeWhenWritable)
{
    bool            bRetVal = false;
    struct timeval *pTimeout = NULL;
    struct timeval  timeout;
    int32           nNumDescriptors = -1;
    int32           nError = 0;

    FD_ZERO(&m_errorFds);
    FD_ZERO(&m_readFds);
    FD_ZERO(&m_writeFds);

    if ( bAwakeWhenReadable )
        FD_SET(m_socket, &m_readFds);
    if ( bAwakeWhenWritable )
        FD_SET(m_socket, &m_writeFds);

    FD_SET(m_socket, &m_errorFds);

    //---------------------------------------------------------------------
    // If timeout has been specified then set value, otherwise set timeout
    // to NULL which will block until a descriptor is ready for read/write
    // or an error has occurred.
    //---------------------------------------------------------------------
    if ((nTimeoutSec > 0) || (nTimeoutUSec > 0))
    {
        timeout.tv_sec = nTimeoutSec;
        timeout.tv_usec = nTimeoutUSec;
        pTimeout = &timeout;
    }

    nNumDescriptors = SELECT(m_socket+1, &m_readFds, &m_writeFds, &m_errorFds, pTimeout);

    //----------------------------------------------------------------------
    // Handle timeout
    //----------------------------------------------------------------------
    if (nNumDescriptors == 0)
    {
        SetSocketError(CSimpleSocket::SocketTimedout);
    }
    //----------------------------------------------------------------------
    // If a file descriptor (read/write) is set then check the
    // socket error (SO_ERROR) to see if there is a pending error.
    //----------------------------------------------------------------------
    else if ((FD_ISSET(m_socket, &m_readFds)) || (FD_ISSET(m_socket, &m_writeFds)))
    {
        int32 nLen = sizeof(nError);

        if (GETSOCKOPT(m_socket, SOL_SOCKET, SO_ERROR, &nError, &nLen) == 0)
        {
            errno = nError;

            if (nError == 0)
            {
                bRetVal = true;
            }
        }

        TranslateSocketError();
    }

    return bRetVal;
}

