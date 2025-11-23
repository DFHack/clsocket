/*---------------------------------------------------------------------------*/
/*                                                                           */
/* PassiveSocket.cpp - Passive Socket Implementation                         */
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
#include "PassiveSocket.h"



CPassiveSocket::CPassiveSocket(CSocketType nType) : CSimpleSocket(nType)
{
}

bool CPassiveSocket::BindMulticast(const char *pInterface, const char *pGroup, uint16 nPort)
{
#ifdef WIN32
    ULONG          inAddr;
#else
    in_addr_t      inAddr;
#endif

    //--------------------------------------------------------------------------
    // Set the following socket option SO_REUSEADDR.  This will allow the file
    // descriptor to be reused immediately after the socket is closed instead
    // of setting in a TIMED_WAIT state.
    //--------------------------------------------------------------------------
    memset(&m_stMulticastGroup,0,sizeof(m_stMulticastGroup));
    m_stMulticastGroup.sin_family = AF_INET;
    m_stMulticastGroup.sin_port = htons(nPort);

    //--------------------------------------------------------------------------
    // If no IP Address (interface ethn) is supplied, or the loop back is
    // specified then bind to any interface, else bind to specified interface.
    //--------------------------------------------------------------------------
    if ((pInterface == NULL) || (!strlen(pInterface)))
    {
        m_stMulticastGroup.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    else
    {
        if ((inAddr = inet_addr(pInterface)) != INADDR_NONE)
        {
            m_stMulticastGroup.sin_addr.s_addr = inAddr;
        }
    }

    if (bind(m_socket, (struct sockaddr *)&m_stMulticastGroup, sizeof(m_stMulticastGroup)) == CSimpleSocket::SocketError)
    {
        TranslateSocketError();
        Close();
        return false;
    }

    // Join the multicast group
    m_stMulticastRequest.imr_multiaddr.s_addr = inet_addr(pGroup);
    m_stMulticastRequest.imr_interface.s_addr = m_stMulticastGroup.sin_addr.s_addr;

    if (SETSOCKOPT(m_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                    (void *)&m_stMulticastRequest,
                    sizeof(m_stMulticastRequest)) == CSimpleSocket::SocketError)
    {

        TranslateSocketError();
        Close();
        return false;
    }

    m_timer.SetEndTime();
    m_timer.Initialize();
    m_timer.SetStartTime();

    return true;
}


//------------------------------------------------------------------------------
//
// Listen() -
//
//------------------------------------------------------------------------------
bool CPassiveSocket::Listen(const char *pAddr, uint16 nPort, int32 nConnectionBacklog)
{
#ifdef WIN32
    ULONG          inAddr;
#else
    in_addr_t      inAddr;

    int32          nReuse;
    nReuse = IPTOS_LOWDELAY;

    //--------------------------------------------------------------------------
    // Set the following socket option SO_REUSEADDR.  This will allow the file
    // descriptor to be reused immediately after the socket is closed instead
    // of setting in a TIMED_WAIT state.
    //--------------------------------------------------------------------------
    SETSOCKOPT(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&nReuse, sizeof(int32));
    SETSOCKOPT(m_socket, IPPROTO_TCP, IP_TOS, &nReuse, sizeof(int32));
#endif

    memset(&m_stServerSockaddr,0,sizeof(m_stServerSockaddr));
    m_stServerSockaddr.sin_family = AF_INET;
    m_stServerSockaddr.sin_port = htons(nPort);

    //--------------------------------------------------------------------------
    // If no IP Address (interface ethn) is supplied, or the loop back is
    // specified then bind to any interface, else bind to specified interface.
    //--------------------------------------------------------------------------
    if ((pAddr == NULL) || (!strlen(pAddr)))
    {
        m_stServerSockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    else
    {
        if ((inAddr = inet_addr(pAddr)) != INADDR_NONE)
        {
            m_stServerSockaddr.sin_addr.s_addr = inAddr;
        }
    }

    m_timer.Initialize();
    m_timer.SetStartTime();

    if (bind(m_socket, (struct sockaddr *)&m_stServerSockaddr, sizeof(m_stServerSockaddr)) == CSimpleSocket::SocketError)
    {
        TranslateSocketError();
        Close();
        return false;
    }

    if (m_nSocketType == CSimpleSocket::SocketTypeTcp)
    {
        if (listen(m_socket, nConnectionBacklog) == CSimpleSocket::SocketError)
        {
            TranslateSocketError();
            Close();
            return false;
        }
    }

    m_timer.SetEndTime();
    return true;
}


//------------------------------------------------------------------------------
//
// Accept() -
//
//------------------------------------------------------------------------------
CActiveSocket *CPassiveSocket::Accept()
{
    if (m_nSocketType != CSimpleSocket::SocketTypeTcp)
    {
        SetSocketError(CSimpleSocket::SocketProtocolError);
        return nullptr;
    }

    auto pClientSocket = new CActiveSocket();

    //--------------------------------------------------------------------------
    // Wait for incoming connection.
    //--------------------------------------------------------------------------
    if (pClientSocket != nullptr) {
        CSocketError socketErrno = CSimpleSocket::SocketError;
        m_timer.Initialize();
        m_timer.SetStartTime();

        do {
            socketErrno = CSimpleSocket::SocketSuccess;
            uint32 nSockLen = sizeof(m_stClientSockaddr);

            SOCKET socket = accept(m_socket, (struct sockaddr *)&m_stClientSockaddr, (socklen_t *)&nSockLen);

            if (socket == INVALID_SOCKET) {
                TranslateSocketError();
                socketErrno = GetSocketError();
                continue;
            }

            pClientSocket->SetSocketHandle(socket);
            pClientSocket->m_stClientSockaddr = m_stClientSockaddr;
            socklen_t sslen = sizeof(pClientSocket->m_stServerSockaddr);
            if (getsockname(socket, (struct sockaddr *)&pClientSocket->m_stServerSockaddr, &sslen) == -1) {
                TranslateSocketError();
                socketErrno = GetSocketError();
                break;

            }
        } while (socketErrno == CSimpleSocket::SocketInterrupted);

        m_timer.SetEndTime();

        if (socketErrno != CSimpleSocket::SocketSuccess) {
            delete pClientSocket;
            pClientSocket = nullptr;
        }
    }

    return pClientSocket;
}


//------------------------------------------------------------------------------
//
// Send() - Send data on a valid socket
//
//------------------------------------------------------------------------------
int32 CPassiveSocket::Send(const uint8 *pBuf, size_t bytesToSend)
{
    m_nBytesSent = 0;

    switch(m_nSocketType)
    {
    case CSimpleSocket::SocketTypeUdp:
    {
        if (IsSocketValid())
        {
            if ((bytesToSend > 0) && (pBuf != NULL))
            {
                m_timer.Initialize();
                m_timer.SetStartTime();

                m_nBytesSent = SENDTO(m_socket, pBuf, bytesToSend, 0,
                                      (const sockaddr *)&m_stClientSockaddr,
                                      sizeof(m_stClientSockaddr));

                m_timer.SetEndTime();
            }
        }
        break;
    }
    case CSimpleSocket::SocketTypeTcp:
            // This function is for UDP. Shouldn't even get here.
        m_nBytesSent = CSimpleSocket::Send(pBuf, bytesToSend);
        break;
    default:
        SetSocketError(SocketProtocolError);
        break;
    }

    if (m_nBytesSent == CSimpleSocket::SocketError)
    {
        TranslateSocketError();
    }

    return m_nBytesSent;
}
