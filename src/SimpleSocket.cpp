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

CSimpleSocket::CSimpleSocket(CSocketType nType) :
    m_socket(INVALID_SOCKET),
    m_socketErrno(CSimpleSocket::SocketInvalidSocket),
    m_pBuffer(NULL), m_nBufferSize(0), m_nSocketDomain(AF_INET),
    m_nSocketType(SocketTypeInvalid), m_nBytesReceived(-1),
    m_nBytesSent(-1), m_nFlags(0),
    m_bIsBlocking(true), m_bIsMulticast(false)
{
    SetConnectTimeout(1, 0);
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
// Initialize() - Initialize socket class
//
//------------------------------------------------------------------------------
bool CSimpleSocket::Initialize()
{
    SetSocketError(CSimpleSocket::SocketSuccess);

#ifdef WIN32
    //-------------------------------------------------------------------------
    // Data structure containing general Windows Sockets Info
    //-------------------------------------------------------------------------
    memset(&m_hWSAData, 0, sizeof(m_hWSAData));
    auto starterr = WSAStartup(MAKEWORD(2, 0), &m_hWSAData);
    if (starterr != 0) {
        SetSocketError(CSimpleSocket::SocketInvalidSocket);
        return false;
    }
#endif

    //-------------------------------------------------------------------------
    // Create the basic Socket Handle
    //-------------------------------------------------------------------------
    m_timer.Initialize();
    m_timer.SetStartTime();

    m_socket = socket(m_nSocketDomain, m_nSocketType, 0);
    if (m_socket == INVALID_SOCKET) {
        TranslateSocketError();
        return false;
    }

    m_timer.SetEndTime();
    return true;
}


//------------------------------------------------------------------------------
//
// BindInterface()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::BindInterface(const char *pInterface)
{
    struct in_addr stInterfaceAddr;

    if (GetMulticast() && pInterface)
    {
        stInterfaceAddr.s_addr= inet_addr(pInterface);
        if (SETSOCKOPT(m_socket, IPPROTO_IP, IP_MULTICAST_IF, &stInterfaceAddr, sizeof(stInterfaceAddr)) == CSimpleSocket::SocketError)
        {
            TranslateSocketError();
            return false;
        }
        return true;
    }

    SetSocketError(CSimpleSocket::SocketProtocolError);
    return false;
}


//------------------------------------------------------------------------------
//
// SetMulticast()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::SetMulticast(bool bEnable, uint8 multicastTTL)
{
    if (GetSocketType() == CSimpleSocket::SocketTypeUdp)
    {
        m_bIsMulticast = bEnable;
        if (SETSOCKOPT(m_socket, IPPROTO_IP, IP_MULTICAST_TTL, (void *)&multicastTTL, sizeof(multicastTTL)) == CSimpleSocket::SocketError)
        {
            TranslateSocketError();
            return false;
        }
        return true;
    }

    SetSocketError(CSimpleSocket::SocketProtocolError);
    return false;
}


//------------------------------------------------------------------------------
//
// SetSocketDscp()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::SetSocketDscp(int32 nDscp)
{
    if (!IsSocketValid())
        return false;

    int32 nTempVal = nDscp;

    nTempVal <<= 4;
    nTempVal /= 4;

    if (SETSOCKOPT(m_socket, IPPROTO_IP, IP_TOS, &nTempVal, sizeof(nTempVal)) == CSimpleSocket::SocketError)
    {
        TranslateSocketError();
        return false;
    }

    return true;
}


//------------------------------------------------------------------------------
//
// GetSocketDscp()
//
//------------------------------------------------------------------------------
int32 CSimpleSocket::GetSocketDscp()
{
    if (!IsSocketValid())
        return 0;

    int32      nTempVal = 0;
    socklen_t  nLen = 0;
    if (GETSOCKOPT(m_socket, IPPROTO_IP, IP_TOS, &nTempVal, &nLen) == CSimpleSocket::SocketError)
    {
        TranslateSocketError();
        return 0;
    }

    nTempVal *= 4;
    nTempVal >>= 4;

    return nTempVal;
}


//------------------------------------------------------------------------------
//
// GetWindowSize()
//
//------------------------------------------------------------------------------
uint32 CSimpleSocket::GetWindowSize(uint32 nOptionName)
{
    if (!IsSocketValid())
        return 0;

    // query for buffer size
    uint32 nTcpWinSize = 0;
    socklen_t nLen = sizeof(nTcpWinSize);
    if (GETSOCKOPT(m_socket, SOL_SOCKET, nOptionName, &nTcpWinSize, &nLen) == CSimpleSocket::SocketError)
    {
        TranslateSocketError();
        return 0;
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
    if (!IsSocketValid())
        return 0;

    if (SETSOCKOPT(m_socket, SOL_SOCKET, nOptionName, &nWindowSize, sizeof(nWindowSize)) == CSimpleSocket::SocketError)
    {
        TranslateSocketError();
        return 0;
    }

    return nWindowSize;
}

bool CSimpleSocket::SetTcpNoDelay(bool enable)
{
    int32 v = enable ? 1 : 0;

    if (SETSOCKOPT(m_socket, IPPROTO_TCP, TCP_NODELAY, &v, sizeof(int32)) == CSimpleSocket::SocketError)
    {
        TranslateSocketError();
        return false;
    }
    return true;
}

bool CSimpleSocket::DisableNagleAlgoritm()
{
    return SetTcpNoDelay(true);
}

bool CSimpleSocket::EnableNagleAlgoritm()
{
    return SetTcpNoDelay(false);
}


//------------------------------------------------------------------------------
//
// Send() - Send data on a valid socket
//
//------------------------------------------------------------------------------
int32 CSimpleSocket::Send(const uint8 *pBuf, size_t bytesToSend)
{
    if (!IsSocketValid())
        return CSimpleSocket::SocketError;

    SetSocketError(CSimpleSocket::SocketSuccess);
    m_nBytesSent = 0;

    switch(m_nSocketType)
    {
    case CSimpleSocket::SocketTypeTcp:
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
                if (m_nBytesSent >= 0)
                    break;
                TranslateSocketError();
            } while (GetSocketError() == CSimpleSocket::SocketInterrupted);

            m_timer.SetEndTime();
        }
        break;
    }
    case CSimpleSocket::SocketTypeUdp:
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
                    m_nBytesSent = SENDTO(m_socket, pBuf, bytesToSend, 0, (const sockaddr *)&m_stServerSockaddr, sizeof(m_stServerSockaddr));
                    if (m_nBytesSent >= 0)
                        break;

                    TranslateSocketError();
                } while (GetSocketError() == CSimpleSocket::SocketInterrupted);
            }

            m_timer.SetEndTime();
        }
        break;
    }
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
bool CSimpleSocket::Close()
{
    //--------------------------------------------------------------------------
    // delete internal buffer
    //--------------------------------------------------------------------------
    if (m_pBuffer != NULL)
    {
        delete [] m_pBuffer;
        m_pBuffer = NULL;
    }

    if (!IsSocketValid())
    {
        return false;
    }

    m_socket = INVALID_SOCKET;

    if (CLOSE(m_socket) == CSimpleSocket::SocketError)
    {
        TranslateSocketError();
        return false;
    }

    return true;
}


//------------------------------------------------------------------------------
//
// Shtudown()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::Shutdown(CShutdownMode nShutdown)
{
    if (shutdown(m_socket, nShutdown) == CSimpleSocket::SocketError) {
        TranslateSocketError();
        return false;
    }

    return true;
}


//------------------------------------------------------------------------------
//
// Flush()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::Flush()
{
    // Can't flush a TCP socket.
    return true;
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
    size_t i          = 0;

    //--------------------------------------------------------------------------
    // Send each buffer as a separate send, windows does not support this
    // function call.
    //--------------------------------------------------------------------------
    for (i = 0; i < nCount; i++)
    {
        if ((nBytes = Send((uint8 *)pVector[i].iov_base, pVector[i].iov_len)) == CSimpleSocket::SocketError)
        {
            break;
        }

        nBytesSent += nBytes;
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
    m_nBytesSent = 0;

    if ((m_nBytesSent = WRITEV(m_socket, sendVector, nNumItems)) == CSimpleSocket::SocketError)
    {
        TranslateSocketError();
    }

    return m_nBytesSent;
}


//------------------------------------------------------------------------------
//
// SetReceiveTimeout()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::SetReceiveTimeout(int32 nRecvTimeoutSec, int32 nRecvTimeoutUsec)
{
    memset(&m_stRecvTimeout, 0, sizeof(struct timeval));

    m_stRecvTimeout.tv_sec = nRecvTimeoutSec;
    m_stRecvTimeout.tv_usec = nRecvTimeoutUsec;

    //--------------------------------------------------------------------------
    // Sanity check to make sure the options are supported!
    //--------------------------------------------------------------------------
    if (SETSOCKOPT(m_socket, SOL_SOCKET, SO_RCVTIMEO, &m_stRecvTimeout,
                   sizeof(struct timeval)) == CSimpleSocket::SocketError)
    {
        TranslateSocketError();
        return false;
    }

    return true;
}


//------------------------------------------------------------------------------
//
// SetSendTimeout()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::SetSendTimeout(int32 nSendTimeoutSec, int32 nSendTimeoutUsec)
{
    memset(&m_stSendTimeout, 0, sizeof(struct timeval));
    m_stSendTimeout.tv_sec = nSendTimeoutSec;
    m_stSendTimeout.tv_usec = nSendTimeoutUsec;

    //--------------------------------------------------------------------------
    // Sanity check to make sure the options are supported!
    //--------------------------------------------------------------------------
    if (SETSOCKOPT(m_socket, SOL_SOCKET, SO_SNDTIMEO, &m_stSendTimeout,
                   sizeof(struct timeval)) == CSimpleSocket::SocketError)
    {
        TranslateSocketError();
        return false;
    }

    return true;
}


//------------------------------------------------------------------------------
//
// SetOptionReuseAddr()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::SetOptionReuseAddr()
{
    int32 nReuse  = IPTOS_LOWDELAY;

    if (SETSOCKOPT(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&nReuse, sizeof(int32)) == CSimpleSocket::SocketError)
    {
        TranslateSocketError();
        return false;
    }

    return true;
}


//------------------------------------------------------------------------------
//
// SetOptionLinger()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::SetOptionLinger(bool bEnable, uint16 nTime)
{
    m_stLinger.l_onoff = int(bEnable);
    m_stLinger.l_linger = nTime;

    if (SETSOCKOPT(m_socket, SOL_SOCKET, SO_LINGER, &m_stLinger, sizeof(m_stLinger)) == CSimpleSocket::SocketError)
    {
        TranslateSocketError();
        return false;
    }

    return true;
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
        return 0;
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
            if (m_nBytesReceived != CSimpleSocket::SocketError)
                break;
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
                if (m_nBytesReceived != CSimpleSocket::SocketError)
                    break;
                TranslateSocketError();
            } while (GetSocketError() == CSimpleSocket::SocketInterrupted);
        }
        else
        {
            do
            {
                m_nBytesReceived = RECVFROM(m_socket, pWorkBuffer, nMaxBytes, 0,
                                            &m_stClientSockaddr, &srcSize);
                if (m_nBytesReceived != CSimpleSocket::SocketError)
                    break;
                TranslateSocketError();
            } while (GetSocketError() == CSimpleSocket::SocketInterrupted);
        }

        break;
    }
    default:
        break;
    }

    m_timer.SetEndTime();

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

    // Peer closed the connection
    if (m_nBytesReceived == 0)
        Close();

    return m_nBytesReceived;
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
        TranslateSocketError();
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

        if ((read(nInFd, szData, nInCount)) != nInCount)
        {
            return -1;
        }

        if ((SEND(nOutFd, szData, nInCount, 0)) != nInCount)
        {
            return -1;
        }

        nOutCount += nInCount;
    }

    *pOffset += nOutCount;

    return nOutCount;
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
    case 0:
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
#endif
#ifdef WIN32
    int32 nError = WSAGetLastError();
    switch (nError)
    {
    case 0:
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
bool CSimpleSocket::Select(int32 nTimeoutSec, int32 nTimeoutUSec)
{
    struct timeval *pTimeout = NULL;
    struct timeval  timeout;
    int32           nNumDescriptors = -1;
    int32           nError = 0;

    FD_ZERO(&m_errorFds);
    FD_ZERO(&m_readFds);
    FD_ZERO(&m_writeFds);
    FD_SET(m_socket, &m_errorFds);
    FD_SET(m_socket, &m_readFds);
    FD_SET(m_socket, &m_writeFds);

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

    // On Windows the first argument is ignored.
    nNumDescriptors = SELECT(m_socket+1, &m_readFds, &m_writeFds, &m_errorFds, pTimeout);
//    nNumDescriptors = SELECT(m_socket+1, &m_readFds, NULL, NULL, pTimeout);

    if (nNumDescriptors == CSimpleSocket::SocketError) {
        TranslateSocketError();
        return false;
    }

    //----------------------------------------------------------------------
    // Handle timeout
    //----------------------------------------------------------------------
    if (nNumDescriptors == 0)
    {
        SetSocketError(CSimpleSocket::SocketTimedout);
        return false;
    }
    //----------------------------------------------------------------------
    // If a file descriptor (read/write) is set then check the
    // socket error (SO_ERROR) to see if there is a pending error.
    //----------------------------------------------------------------------
    if ((FD_ISSET(m_socket, &m_readFds)) || (FD_ISSET(m_socket, &m_writeFds)))
    {
        int32 nLen = sizeof(nError);

        if (GETSOCKOPT(m_socket, SOL_SOCKET, SO_ERROR, &nError, &nLen) == 0)
        {
            if (nError != 0) {
                SetSocketError(CSocketError(nError));
                TranslateSocketError();
                return false;
            }
        } else { // getsockopt failed
            TranslateSocketError();
            return false;
        }
    }

    return true;
}