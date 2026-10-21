/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
    anonymous contributors,

  to the public domain (http://creativecommons.org/publicdomain/zero/1.0/).

  Note that some moral obligations still exist in the absence of legal ones.
  For example, it would still be dishonest to deliberately misrepresent the
  origin of a work. Although we impose no legal requirements to obtain a
  license, it is beseeming for those who build on the works of others to
  give back useful improvements, or find a way to pay it forward. If
  you would like to cite us, a published paper about Waffles can be found
  at http://jmlr.org/papers/volume12/gashler11a/gashler11a.pdf. If you find
  our code to be useful, the Waffles team would love to hear how you use it.
*/

#include "WSocket.h"
#include <time.h>
#include "WHolders.h"
#include "WError.h"
#include "WThread.h"
#include "WRand.h"
#include "WApp.h"
#include "WTime.h"
#include <wchar.h>
#include <sstream>
#include <iostream>
#include <queue>
#ifdef WINDOWS
#include "WWindows.h"
#include <Ws2tcpip.h>
#else
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <errno.h>
#define SOCKET_ERROR -1
#endif

using std::cerr;
using std::vector;
using std::string;
using std::set;

#ifndef WINDOWS
typedef struct sockaddr SOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN;
#endif

namespace WClasses
{


#ifdef WINDOWS
string winstrerror(int err)
{
	char buf[1024];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)buf, 1024, NULL);
	return string(buf);
}
#endif

bool WSocket_isReady(SOCKET s)
{
	fd_set sockSet;
	FD_ZERO(&sockSet);
	FD_SET(s, &sockSet);

	// Check which sockets are ready for reading
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	int ret = select((int)s + 1, &sockSet, nullptr, nullptr, &timeout);
	if(ret < 0)
	{
#ifdef WINDOWS
		throw Ex("select failed: ", winstrerror(WSAGetLastError()));
#else
		throw Ex("select failed: ", strerror(errno));
#endif
	}
	return ret == 0 ? false : true;
}

void WSocket_setSocketMode(SOCKET s, bool blocking)
{
	unsigned long ulMode = blocking ? 0 : 1;
#ifdef WINDOWS
	if(ioctlsocket(s, FIONBIO, &ulMode) != 0)
#else
	if(ioctl(s, FIONBIO, &ulMode) != 0)
#endif
		throw Ex("Error changing the mode of a socket");
}

size_t WSocket_bytesReady(SOCKET s)
{
	if(s == INVALID_SOCKET)
		return 0;
	unsigned long bytesReadyToRead = 0;
#ifdef WINDOWS
	WWindows::yield(); // This is necessary because incoming packets go through the Windows message pump
	if(ioctlsocket(s, FIONREAD, &bytesReadyToRead) != 0)
		throw Ex("ioctlsocket failed: ", winstrerror(WSAGetLastError()));
#else
	if(ioctl(s, FIONREAD, &bytesReadyToRead) != 0)
		throw Ex("ioctl failed: ", strerror(errno));
#endif
	return bytesReadyToRead;
}

void WSocket_closeSocket(SOCKET s)
{
	shutdown(s, 2 /*SHUT_RDWR*/);
#ifdef WINDOWS
	closesocket(s);
#else
	close(s);
#endif
}

in_addr WSocket_ipAddr(SOCKET s)
{
	struct sockaddr sAddr;
	socklen_t l;
	l = sizeof(SOCKADDR);
	if(getpeername(s, &sAddr, &l))
	{
#ifdef WINDOWS
		throw Ex("getpeername failed: ", winstrerror(WSAGetLastError()));
#else
		throw Ex("getpeername failed: ", strerror(errno));
#endif
	}
	if(sAddr.sa_family != AF_INET)
		throw Ex("Error, expected family to be AF_INET");
	SOCKADDR_IN* pInfo = (SOCKADDR_IN*)&sAddr;
	return pInfo->sin_addr;
}

size_t WSocket_send(SOCKET s, const char* buf, size_t len)
{
	if(s == INVALID_SOCKET)
		throw Ex("Tried to send over a socket that was not connected");
	// SAKBox: writing to a connection the other side has closed raised
	// SIGPIPE, which ends the process; now it's an error, thrown below
	int flags = 0;
#ifdef MSG_NOSIGNAL
	flags |= MSG_NOSIGNAL;
#endif
#ifdef SO_NOSIGPIPE
	int on = 1;
	setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
	ssize_t bytesSent = ::send(s, buf, (int)len, flags);
	if(bytesSent < 0)
	{
#ifdef WINDOWS
		int err = WSAGetLastError();
		if(err == WSAEWOULDBLOCK)
			return 0;
		else
			throw Ex("Error sending in WTCPClient::send: ", winstrerror(err));
#else
		if(errno == EWOULDBLOCK)
			return 0;
		else
			throw Ex("Error sending in WTCPClient::send: ", strerror(errno));
#endif
		return 0;
	}
	else
		return (size_t)bytesSent;
}

void WSocket_init()
{
#ifdef WINDOWS
	// Initialize Winsock
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(1, 1);
	int err = WSAStartup(wVersionRequested, &wsaData);
	if(err != 0)
		throw Ex("Failed to find a usable WinSock DLL");

	// Confirm that the WinSock DLL supports at least 2.2.
	if(LOBYTE(wsaData.wVersion) != 1 || HIBYTE(wsaData.wVersion) != 1)
	{
		int n1 = LOBYTE(wsaData.wVersion);
		int n2 = HIBYTE(wsaData.wVersion);
		WSACleanup();
		throw Ex("Found a Winsock DLL, but it only supports an older version. It needs to support version 2.2");
	}
#endif
}

SOCKET WSocket_connect(const char* addr, unsigned short port, int timeoutSecs)
{
	struct addrinfo hints, *res, *res0;
	int error;
	res0 = nullptr;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	std::ostringstream os;
	os << port;
	string tmp = os.str();
	error = getaddrinfo(addr, tmp.c_str(), &hints, &res0);
	if(error)
		throw Ex(gai_strerror(error));
	SOCKET sock = INVALID_SOCKET;
	for(res = res0; res; res = res->ai_next)
	{
		sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if(sock < 0)
			continue;
		struct timeval timeout;
		fd_set socketSet;
		WSocket_setSocketMode(sock, false);

		// Trying to connect with timeout
		if(::connect(sock, res->ai_addr, (int)res->ai_addrlen) < 0)
		{
#ifdef WINDOWS
			int n = WSAGetLastError();
			if(n == WSAEWOULDBLOCK || n == WSAEINPROGRESS)
#else
			if(errno == EINPROGRESS)
#endif
			{
				timeout.tv_sec = timeoutSecs;
				timeout.tv_usec = 0;
				FD_ZERO(&socketSet);
				FD_SET(sock, &socketSet);
#ifdef WINDOWS
				WWindows::yield();
				int result = select((int)sock + 1, NULL, &socketSet, NULL, &timeout);
				if(result < 0 && WSAGetLastError() != WSAEINTR)
#else
				int result = select(sock + 1, nullptr, &socketSet, nullptr, &timeout);
				if(result < 0 && errno != EINTR)
#endif
					throw Ex("Failed to connect to ", addr, " on port ", to_str(port));
				else if(result > 0)
				{
					// Socket selected for write
					socklen_t lon = sizeof(int);
					int valopt;
					if(getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)(&valopt), &lon) < 0)
						throw Ex("getsockopt failed");
					if(valopt)
					{
						WSocket_closeSocket(sock);
						sock = INVALID_SOCKET;
						continue;
					}

					// Got a connection!
					break;
				}
				else
				{
					// Timeout exceeded
					WSocket_closeSocket(sock);
					sock = INVALID_SOCKET;
					continue;
				}
			}
			else
			{
				// Failed to connect to this address
				WSocket_closeSocket(sock);
				sock = INVALID_SOCKET;
				continue;
			}
		}
	}
	freeaddrinfo(res0);
	if(sock == INVALID_SOCKET)
		throw Ex("Failed to connect to ", addr, " on port ", to_str(port));
	return sock;
}




WTCPClient::WTCPClient()
: m_sock(INVALID_SOCKET)
{
	WSocket_init();
}

WTCPClient::~WTCPClient()
{
	disconnect();
}

void WTCPClient::disconnect()
{
	if(m_sock == INVALID_SOCKET)
		return;
	onDisconnect();
	WSocket_closeSocket(m_sock);
	m_sock = INVALID_SOCKET;
}

bool WTCPClient::isConnected()
{
	return m_sock == INVALID_SOCKET ? false : true;
}

void WTCPClient::connect(const char* addr, unsigned short port, int timeoutSecs)
{
	disconnect();
	m_sock = WSocket_connect(addr, port, timeoutSecs);
}

void WTCPClient::send(const char* buf, size_t len)
{
	try
	{
		while(len > 0)
		{
			size_t bytesSent = WSocket_send(m_sock, buf, len);
			if(bytesSent > 0)
			{
				buf += bytesSent;
				len -= bytesSent;
			}
			else
				WThread::sleep(0);
		}
	}
	catch(const std::exception&)
	{
		disconnect();
		throw; // SAKBox: was "throw e", which rethrew a plain std::exception without the message
	}
}

size_t WTCPClient::receive(char* buf, size_t len)
{
	size_t bytesReady = WSocket_bytesReady(m_sock);
	if(bytesReady > 0)
	{
		ssize_t bytesReceived = recv(m_sock, buf, (int)len, 0);
		if(bytesReceived > 0)
			return size_t(bytesReceived);
		else if(bytesReceived == 0)
		{
			disconnect();
			return 0;
		}
		else
		{
#ifdef WINDOWS
			throw Ex("Error calling recv: ", winstrerror(WSAGetLastError()));
#else
			throw Ex("Error calling recv: ", strerror(errno));
#endif
		}
	}
	return 0;
}










in_addr WTCPConnection::ipAddr()
{
	return WSocket_ipAddr(m_sock);
}

const char* WTCPConnection::getIPAddress()
{
	return inet_ntoa(ipAddr());
}






WTCPServer::WTCPServer(unsigned short port, bool networkAccessible)
: m_networkAccessible(networkAccessible)
{
	WSocket_init();
	m_sock = socket(AF_INET, SOCK_STREAM, 0); // use SOCK_DGRAM for UDP

	// Tell the socket that it's okay to reuse an old crashed socket that hasn't timed out yet
	int flag = 1;
	setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&flag, sizeof(flag));

	// Bind the socket to the port
	SOCKADDR_IN sHostAddrIn;
	memset(&sHostAddrIn, '\0', sizeof(SOCKADDR_IN));
	sHostAddrIn.sin_family = AF_INET;
	sHostAddrIn.sin_port = htons(port);
	// SAKBox: was always INADDR_ANY, so any device on the network could connect
	sHostAddrIn.sin_addr.s_addr = htonl(networkAccessible ? INADDR_ANY : INADDR_LOOPBACK);
	if(bind(m_sock, (struct sockaddr*)&sHostAddrIn, sizeof(SOCKADDR)) != 0)
	{
#ifdef WINDOWS
		throw Ex("Failed to bind to port ", to_str(port), ": ", winstrerror(WSAGetLastError()));
#else
		throw Ex("Failed to bind to port ", to_str(port), ": ", strerror(errno));
#endif
	}

	// Start listening for connections
	if(listen(m_sock, SOMAXCONN) != 0)
#ifdef WINDOWS
		throw Ex("Failed to listen on the socket: ", winstrerror(WSAGetLastError()));
#else
		throw Ex("Failed to listen on the socket: ", strerror(errno));
#endif
}

unsigned short WTCPServer::port() const
{
	sockaddr_in addr;
	socklen_t len = sizeof(addr);
	if(getsockname(m_sock, (sockaddr*)&addr, &len) != 0)
		throw Ex("getsockname failed");
	return ntohs(addr.sin_port);
}

std::string WTCPServer::boundAddress() const
{
	sockaddr_in addr;
	socklen_t len = sizeof(addr);
	if(getsockname(m_sock, (sockaddr*)&addr, &len) != 0)
		throw Ex("getsockname failed");
	char buf[INET_ADDRSTRLEN];
	if(!inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf)))
		throw Ex("inet_ntop failed");
	return buf;
}

WTCPServer::~WTCPServer()
{
	while(m_socks.size() > 0)
		disconnect(*m_socks.begin());
	WSocket_closeSocket(m_sock);
}

void WTCPServer::disconnect(WTCPConnection* pConn)
{
	onDisconnect(pConn);
	WSocket_closeSocket(pConn->socket());
	m_socks.erase(pConn);
	delete(pConn);
}

void WTCPServer::checkForNewConnections()
{
	if(!WSocket_isReady(m_sock))
		return;

	// Accept the connection
	SOCKADDR_IN sHostAddrIn;
	socklen_t nStructSize = sizeof(struct sockaddr);
	SOCKET s = accept(m_sock, (struct sockaddr*)&sHostAddrIn, &nStructSize);
	if(s < 0)
	{
#ifdef WINDOWS
		if(WSAGetLastError() == WSAEWOULDBLOCK) // no connections are ready to be accepted
			return;
#else
		if(errno == EAGAIN) // no connections are ready to be accepted
			return;
#endif
		string s2 = "Received bad data while trying to accept a connection: ";
#ifdef WINDOWS
		s2 += winstrerror(WSAGetLastError());
#else
		s2 += strerror(errno);
#endif
		onReceiveBadData(s2.c_str());
		return;
	}
	WSocket_setSocketMode(s, false);
	m_socks.insert(makeConnection(s));
}

size_t WTCPServer::receive(char* buf, size_t len, WTCPConnection** pOutConn)
{
	checkForNewConnections();
	for(set<WTCPConnection*>::iterator it = m_socks.begin(); it != m_socks.end(); it++)
	{
		WTCPConnection* pConn = *it;
		if(WSocket_bytesReady(pConn->socket()) > 0)
		{
			ssize_t bytesReceived = recv(pConn->socket(), buf, (int)len, 0);
			if(bytesReceived > 0)
			{
				*pOutConn = pConn;
				return size_t(bytesReceived);
			}
			else if(bytesReceived == 0)
			{
				// The client has disconnected gracefully
				disconnect(pConn);

				// Recurse since the previous operation will invalidate the iterator
				return receive(buf, len, pOutConn);
			}
			else
			{
#ifdef WINDOWS
				throw Ex("Error calling recv: ", winstrerror(WSAGetLastError()));
#else
				throw Ex("Error calling recv: ", strerror(errno));
#endif
			}
		}
	}
	return 0;
}

void WTCPServer::send(const char* buf, size_t len, WTCPConnection* pConn)
{
	try
	{
		while(len > 0)
		{
			size_t bytesSent = WSocket_send(pConn->socket(), buf, len);
			if(bytesSent > 0)
			{
				buf += bytesSent;
				len -= bytesSent;
			}
			else
				WThread::sleep(0);
		}
	}
	catch(const std::exception&)
	{
		disconnect(pConn);
		throw; // SAKBox: was "throw e", which rethrew a plain std::exception without the message
	}
}

// static
void WTCPServer::hostName(char* buf, size_t len)
{
	if(gethostname(buf, (int)len) == SOCKET_ERROR)
		throw Ex("failed to get host noame");
}

/*
// static
in_addr WTCPServer::hostNameToIPAddress(char* szHostName)
{
	struct hostent* pHostEnt = gethostbyname(szHostName);
	if(pHostEnt == 0)
		throw Ex("Couldn't resolve an IP address for ", szHostName);
	struct in_addr addr, cand;
	int nGoodness = -1;

	// Find the most-accessible address
	for (int i = 0; pHostEnt->h_addr_list[i] != 0; ++i)
	{
		memcpy(&cand, pHostEnt->h_addr_list[i], sizeof(struct in_addr));
		const char* szAddr = inet_ntoa(cand);
		int nScore;
		if(strcmp(szAddr, "127.0.0.1") == 0) // 127.0.0.1 is the loopback address for localhost
			nScore = 0;
		else if(strncmp(szAddr, "10.", 3) == 0) // 10.0.0.0 - 10.255.255.255 are reserved for private networks
			nScore = 1;
		else if(strncmp(szAddr, "192.168.", 8) == 0) // 192.168.0.0 - 192.168.255.255 are reserved for private networks
			nScore = 2;
		else if(strncmp(szAddr, "169.254.", 8) == 0) // 169.254.0.0 - 169.254.255.255 are reserved for automatic private addressing
			nScore = 3;
		else if(strncmp(szAddr, "172.", 4) == 0) // 172.16.0.0 - 172.31.255.255 are reserved for private networks
			nScore = 4;
		else
			nScore = 5;
		if(nScore > nGoodness)
		{
			memcpy(&addr, pHostEnt->h_addr_list[i], sizeof(struct in_addr));
			nGoodness = nScore;
		}
	}
	return addr;
}
*/



#define MAGIC_VALUE 0x0b57ac1e

int WPackageConnection::receive(unsigned int maxBufSize, unsigned int maxPackageSize)
{
	if(WSocket_bytesReady(m_sock) == 0)
		return 0; // Nothing bad happened
	if(m_headerBytes < 2 * sizeof(unsigned int)) // if the header is still incomplete...
	{
		// Receive the header
		ssize_t bytesReceived = recv(m_sock, ((char*)m_header) + m_headerBytes, 2 * sizeof(unsigned int) - m_headerBytes, 0);
		if(bytesReceived > 0) // if we successfully received something...
		{
			m_headerBytes += (unsigned int)bytesReceived;
			if(m_headerBytes >= 2 * sizeof(unsigned int))
			{
				if(m_header[0] == MAGIC_VALUE)
				{
					if(m_header[1] > maxPackageSize)
						return 2; // The package is too big
					if(m_bufSize < m_header[1])
					{
						m_bufSize = std::max(m_header[1], std::min(maxBufSize, 2 * m_bufSize));
						delete[] m_pBuf;
						m_pBuf = new char[m_bufSize];
					}
					else if(m_bufSize > maxBufSize && m_header[1] < maxBufSize)
					{
						m_bufSize = m_header[1];
						delete[] m_pBuf;
						m_pBuf = new char[m_bufSize];
					}
					return receive(maxBufSize, maxPackageSize); // do it again to get the body of the package
				}
				else
				{
					m_headerBytes = 0;
					m_payloadBytes = 0;
					return 3; // The header is incorrect
				}
			}
			else
				return 0; // Nothing bad happened
		}
		else if(bytesReceived == 0)
			return 1; // The other end disconnected
		else
		{
#ifdef WINDOWS
			throw Ex("Error calling recv: ", winstrerror(WSAGetLastError()));
#else
			throw Ex("Error calling recv: ", strerror(errno));
#endif
			return 1; // The other end disconnected
		}
	}
	else
	{
		// Receive the payload
		ssize_t bytesReceived = recv(m_sock, m_pBuf + m_payloadBytes, m_header[1] - m_payloadBytes, 0);
		if(bytesReceived > 0) // if we successfully received something...
		{
			m_payloadBytes += (unsigned int)bytesReceived;
			if(m_payloadBytes >= m_header[1])
			{
				m_q.push(WPackageConnectionBuf(m_pBuf, m_bufSize, m_payloadBytes));
				m_pBuf = nullptr;
				m_bufSize = 0;
				m_payloadBytes = 0;
				m_headerBytes = 0;
			}
			return 0; // nothing bad happened
		}
		else if(bytesReceived == 0)
			return 1; // The other end disconnected
		else
		{
#ifdef WINDOWS
			throw Ex("Error calling recv: ", winstrerror(WSAGetLastError()));
#else
			throw Ex("Error calling recv: ", strerror(errno));
#endif
			return 1; // The other end disconnected
		}
	}
}

char* WPackageConnection::next(size_t* pOutSize)
{
	if(m_q.size() == 0)
		return nullptr;
	WPackageConnectionBuf& package = m_q.front();
	if(m_bufSize == 0)
	{
		m_pBuf = package.m_pBuf;
		m_bufSize = package.m_bufSize;
		*pOutSize = package.m_dataSize;
		m_q.pop();
		return m_pBuf;
	}
	else
	{
		delete[] m_pCondemned;
		m_pCondemned = package.m_pBuf;
		*pOutSize = package.m_dataSize;
		m_q.pop();
		return m_pCondemned;
	}
}




















WPackageClient::WPackageClient()
: m_conn(INVALID_SOCKET), m_maxBufSize(8192), m_maxPackageSize(0x1000000)
{
}

WPackageClient::~WPackageClient()
{
}

void WPackageClient::disconnect()
{
	if(m_conn.socket() == INVALID_SOCKET)
		return;
	onDisconnect();
	WSocket_closeSocket(m_conn.socket());
	m_conn.setSocket(INVALID_SOCKET);
}

void WPackageClient::connect(const char* addr, unsigned short port, int timeoutSecs)
{
	disconnect();
	m_conn.setSocket(WSocket_connect(addr, port, timeoutSecs));
}

// virtual
void WPackageClient::pump()
{
	int status = m_conn.receive(m_maxBufSize, m_maxPackageSize);
	if(status)
	{
		if(status == 1)
		{
			disconnect();
			return;
		}
		else if(status == 2)
			onReceiveBadData("Package too big");
		else if(status == 3)
			onReceiveBadData("Breach of protocol");
	}
	WThread::sleep(0);
}

void WPackageClient::send(const char* buf, size_t len)
{
	unsigned int header[2];
	header[0] = MAGIC_VALUE;
	header[1] = (unsigned int)len;
	char* pH = (char*)header;
	size_t hl = 2 * sizeof(unsigned int);
	try
	{
		while(hl > 0)
		{
			size_t bytesSent = WSocket_send(m_conn.socket(), pH, hl);
			if(bytesSent > 0)
			{
				pH += bytesSent;
				hl -= bytesSent;
			}
			else
				pump();
		}
		while(len > 0)
		{
			size_t bytesSent = WSocket_send(m_conn.socket(), buf, len);
			if(bytesSent > 0)
			{
				buf += bytesSent;
				len -= bytesSent;
			}
			else
				pump();
		}
	}
	catch(const std::exception&)
	{
		disconnect();
		throw; // SAKBox: was "throw e", which rethrew a plain std::exception without the message
	}
}

char* WPackageClient::receive(size_t* pLen)
{
	int status = m_conn.receive(m_maxBufSize, m_maxPackageSize);
	if(status)
	{
		if(status == 1)
			disconnect();
		else if(status == 2)
			onReceiveBadData("Package too big");
		else if(status == 3)
			onReceiveBadData("Breach of protocol");
		return nullptr;
	}
	else
		return m_conn.next(pLen);
}








WPackageServer::WPackageServer(unsigned short port, bool networkAccessible)
: m_maxBufSize(8192), m_maxPackageSize(0x1000000)
{
	WSocket_init();
	m_sock = socket(AF_INET, SOCK_STREAM, 0); // use SOCK_DGRAM for UDP

	// Tell the socket that it's okay to reuse an old crashed socket that hasn't timed out yet
	int flag = 1;
	setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&flag, sizeof(flag));

	// Bind the socket to the port
	SOCKADDR_IN sHostAddrIn;
	memset(&sHostAddrIn, '\0', sizeof(SOCKADDR_IN));
	sHostAddrIn.sin_family = AF_INET;
	sHostAddrIn.sin_port = htons(port);
	// SAKBox: was always INADDR_ANY, so any device on the network could connect
	sHostAddrIn.sin_addr.s_addr = htonl(networkAccessible ? INADDR_ANY : INADDR_LOOPBACK);
	if(bind(m_sock, (struct sockaddr*)&sHostAddrIn, sizeof(SOCKADDR)) != 0)
	{
#ifdef WINDOWS
		throw Ex("Failed to bind to port ", to_str(port), ": ", winstrerror(WSAGetLastError()));
#else
		throw Ex("Failed to bind to port ", to_str(port), ": ", strerror(errno));
#endif
	}

	// Start listening for connections
	if(listen(m_sock, SOMAXCONN) != 0)
#ifdef WINDOWS
		throw Ex("Failed to listen on the socket: ", winstrerror(WSAGetLastError()));
#else
		throw Ex("Failed to listen on the socket: ", strerror(errno));
#endif
}

WPackageServer::~WPackageServer()
{
	while(m_socks.size() > 0)
		disconnect(*m_socks.begin());
	WSocket_closeSocket(m_sock);
}

void WPackageServer::disconnect(WPackageConnection* pConn)
{
	onDisconnect(pConn);
	WSocket_closeSocket(pConn->socket());
	m_socks.erase(pConn);
	delete(pConn);
}

// virtual
void WPackageServer::pump(WPackageConnection* pConn)
{
	int status = pConn->receive(m_maxBufSize, m_maxPackageSize);
	if(status)
	{
		if(status == 2)
			onReceiveBadData("Package too big");
		else if(status == 3)
			onReceiveBadData("Breach of protocol");
		disconnect(pConn);
	}
	WThread::sleep(0);
}

void WPackageServer::checkForNewConnections()
{
	if(!WSocket_isReady(m_sock))
		return;

	// Accept the connection
	SOCKADDR_IN sHostAddrIn;
	socklen_t nStructSize = sizeof(struct sockaddr);
	SOCKET s = accept(m_sock, (struct sockaddr*)&sHostAddrIn, &nStructSize);
	if(s < 0)
	{
#ifdef WIN32
		if(WSAGetLastError() == WSAEWOULDBLOCK) // no connections are ready to be accepted
			return;
#else
		if(errno == EAGAIN) // no connections are ready to be accepted
			return;
#endif
		string s2 = "Received bad data while trying to accept a connection: ";
#ifdef WINDOWS
		s2 += winstrerror(WSAGetLastError());
#else
		s2 += strerror(errno);
#endif
		onReceiveBadData(s2.c_str());
		return;
	}
	WSocket_setSocketMode(s, false);
	m_socks.insert(makeConnection(s));
}

void WPackageServer::send(const char* buf, size_t len, WPackageConnection* pConn)
{
	unsigned int header[2];
	header[0] = MAGIC_VALUE;
	header[1] = (unsigned int)len;
	char* pH = (char*)header;
	size_t hl = 2 * sizeof(unsigned int);
	try
	{
		while(hl > 0)
		{
			size_t bytesSent = WSocket_send(pConn->socket(), pH, hl);
			if(bytesSent > 0)
			{
				pH += bytesSent;
				hl -= bytesSent;
			}
			else
				pump(pConn);
		}
		while(len > 0)
		{
			size_t bytesSent = WSocket_send(pConn->socket(), buf, len);
			if(bytesSent > 0)
			{
				buf += bytesSent;
				len -= bytesSent;
			}
			else
				pump(pConn);
		}
	}
	catch(const std::exception& e)
	{
		onReceiveBadData(e.what());
	}
}

char* WPackageServer::receive(size_t* pOutLen, WPackageConnection** pOutConn)
{
	checkForNewConnections();
	for(set<WPackageConnection*>::iterator it = m_socks.begin(); it != m_socks.end(); it++)
	{
		WPackageConnection* pConn = *it;
		int status = pConn->receive(m_maxBufSize, m_maxPackageSize);
		if(status)
		{
			if(status == 2)
				onReceiveBadData("Package too big");
			else if(status == 3)
				onReceiveBadData("Breach of protocol");
			disconnect(pConn);
		}
		else
		{
			char* pPackage = pConn->next(pOutLen);
			if(pPackage)
			{
				*pOutConn = pConn;
				return pPackage;
			}
		}
	}
	return nullptr;
}


#define TEST_PORT 7251
#define CLIENT_COUNT 5
#define TEST_LEN 1000
#define MAX_PACKET_LEN 257

void WPackageServer_serial_test()
{
	WPackageServer server(TEST_PORT);
	WPackageClient clients[CLIENT_COUNT];
	for(size_t i = 0; i < CLIENT_COUNT; i++)
		clients[i].connect("localhost", TEST_PORT, 5);
	std::queue<size_t> q[CLIENT_COUNT];
	WRand randMaster(0);
	WRand randData(1234);
	char buf[MAX_PACKET_LEN];
	size_t sends = 0;
	size_t receives = 0;
	[[maybe_unused]] size_t bounces = 0;
	[[maybe_unused]] size_t iters = 0;
	while(receives < TEST_LEN)
	{
		size_t turn = size_t(randMaster.next(3 * CLIENT_COUNT));
		if(turn < CLIENT_COUNT)
		{
			if(sends < TEST_LEN && sends < receives + 50)
			{
				// The client sends a random packet of random size
				size_t len = (size_t)randMaster.next(MAX_PACKET_LEN - 5) + 5;
				size_t seed = (size_t)randMaster.next(1000000);
				randData.setSeed(seed);
				q[turn].push(len);
				q[turn].push(seed);
				char* pB = buf;
				for(size_t i = 0; i < len; i++)
					*(pB++) = (char)randData.next();
				//cout << "Client " << to_str(turn) << " sent seed=" << to_str(seed) << ", len=" << len << ", data=" << to_str((int)buf[0]) << to_str((int)buf[1]) << to_str((int)buf[2]) << "\n"; cout.flush();
				clients[turn].send(buf, len);
				sends++;
			}
			else
				WThread::sleep(0);
		}
		else if(turn < CLIENT_COUNT + CLIENT_COUNT)
		{
			// The client receives the next packet and checks it
			turn -= CLIENT_COUNT;
			size_t len;
			char* pPackage = clients[turn].receive(&len);
			if(pPackage)
			{
				if(q[turn].size() == 0)
					throw Ex("unexpected package");
				size_t ll = q[turn].front();
				q[turn].pop();
				if(len != ll)
					throw Ex("incorrect package size");
				size_t seed = q[turn].front();
				q[turn].pop();
				randData.setSeed(seed);
				char* pB = pPackage;
				//cout << "Client " << to_str(turn) << " received seed=" << to_str(seed) << ", len=" << len << ", data=" << to_str((int)pB[0]) << to_str((int)pB[1]) << to_str((int)pB[2]) << "\n"; cout.flush();
				for(size_t i = 0; i < len; i++)
				{
					if(*(pB++) != (char)randData.next())
						throw Ex("package corruption");
				}
				receives++;
			}
			else
				WThread::sleep(0);
		}
		else
		{
			size_t len;
			WPackageConnection* pConn;
			char* pPackage = server.receive(&len, &pConn);
			if(pPackage)
			{
				//cout << "Server bounced " << to_str((int)pPackage[0]) << to_str((int)pPackage[1]) << to_str((int)pPackage[2]) << "\n"; cout.flush();
				server.send(pPackage, len, pConn);
				bounces++;
			}
			else
				WThread::sleep(0);
		}
		iters++;
	}
	if(sends != TEST_LEN || receives != TEST_LEN)
		throw Ex("something is amiss");
}

void WPackageServer::test()
{
	WPackageServer_serial_test();
	//WPackageServer_threaded_test();
}





void WDomClient::send(WDomNode* pNode)
{
	std::ostringstream os;
	m_doc.setRoot(pNode);
	m_doc.writeJson(os);
	string s = os.str();
	WPackageClient::send(s.c_str(), s.length());
}

const WDomNode* WDomClient::receive()
{
	m_doc.clear();
	size_t len;
	char* pPackage = WPackageClient::receive(&len);
	if(pPackage)
	{
		m_doc.parseJson(pPackage, len);
		return m_doc.root();
	}
	else
		return nullptr;
}





void WDomServer::send(WDomNode* pNode, WPackageConnection* pConn)
{
	std::ostringstream os;
	m_doc.setRoot(pNode);
	m_doc.writeJson(os);
	string s = os.str();
	WPackageServer::send(s.c_str(), s.length(), pConn);
}

const WDomNode* WDomServer::receive(WPackageConnection** pOutConn)
{
	m_doc.clear();
	size_t len;
	char* pPackage = WPackageServer::receive(&len, pOutConn);
	if(pPackage)
	{
		m_doc.parseJson(pPackage, len);
		return m_doc.root();
	}
	else
		return nullptr;
}




} // namespace WClasses
