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

#include "WDynamicPage.h"
#include "WApp.h"
#include "WBits.h"
#include "WHashTable.h"
#include "WDom.h"
#include "WImage.h"
#include "WHolders.h"
#include "WFile.h"
#include "WThread.h"
#include "WString.h"
#include "WSocket.h"
#include "WTime.h"
#include "sha2.h"
#include "WRand.h"
#ifndef WINDOWS
#include <unistd.h>
#include <arpa/inet.h>
#endif
#include <stdlib.h>
#include <algorithm>
#include <string>
#include <sstream>
#include <iostream>
#include <memory>

using namespace WClasses;
using std::vector;
using std::string;
using std::cout;
using std::make_pair;
using std::map;
using std::ostream;
using std::ostringstream;


WDynamicPageSession::WDynamicPageSession(unsigned long long ident)
: m_pConnection(nullptr)
{
	m_id = ident;
	onAccess();
	m_pExtension = nullptr;
}

WDynamicPageSession::WDynamicPageSession(WDynamicPageServer* pServer, WDomNode* pNode)
: m_pConnection(nullptr)
{
	m_id = (unsigned long long)pNode->getInt("id");
	m_tLastAccessed = (time_t)pNode->getInt("acc");
	WDomNode* pExt = pNode->getIfExists("ext");
	if(pExt)
		m_pExtension = pServer->deserializeSessionExtension(pExt);
	else
		m_pExtension = nullptr;
}

// virtual
WDynamicPageSession::~WDynamicPageSession()
{
	if(m_pExtension)
		m_pExtension->onDisown();
}

WDynamicPageServer* WDynamicPageSession::server()
{
	return m_pConnection->m_pServer;
}

void WDynamicPageSession::setConnection(WDynamicPageConnection* pConn)
{
	m_pConnection = pConn;
}

void WDynamicPageSession::onAccess()
{
	time_t t;
	m_tLastAccessed = time(&t);
}

void WDynamicPageSession::setExtension(WDynamicPageSessionExtension* pExtension)
{
	if(m_pExtension)
		m_pExtension->onDisown();
	m_pExtension = pExtension;
}

WDomNode* WDynamicPageSession::serialize(WDom* pDoc)
{
	WDomNode* pObj = pDoc->newObj();
	pObj->add(pDoc, "id", (long long)m_id);
	pObj->add(pDoc, "acc", (long long)m_tLastAccessed);
	if(m_pExtension)
		pObj->add(pDoc, "ext", m_pExtension->serialize(pDoc));
	return pObj;
}

const char* WDynamicPageSession::url()
{
	return m_pConnection->m_szUrl;
}

const char* WDynamicPageSession::params()
{
	return m_pConnection->m_pContent;
}

size_t WDynamicPageSession::paramsLen()
{
	return m_pConnection->m_nContentLength;
}

void WDynamicPageSession::addAjaxCookie(WDom& docOut, WDomNode* pOutNode)
{
	std::ostringstream os;
	os << "GDPSI=";
	os << m_id;
	pOutNode->add(&docOut, "cookie", os.str().c_str());
}

// ------------------------------------------------------

WDynamicPageConnection::WDynamicPageConnection(SOCKET sock, WDynamicPageServer* pServer)
: WHttpConnection(sock), m_pServer(pServer)
{
}

// virtual
WDynamicPageConnection::~WDynamicPageConnection()
{
}

// virtual
bool WDynamicPageConnection::hasBeenModifiedSince(const char* szUrl, const char* szDate)
{
	return true;
}

string remove_cr_lf(const char* str)
{
	string s = str;
	for(size_t i = 0; i < s.length(); i++)
	{
		if(s[i] == '\r' || s[i] == '\n')
			s[i] = '_';
	}
	return s;
}

const char* _strnstr(const char* big, const char* small, size_t len)
{
	for(size_t i = 0; i < len; i++)
	{
		const char* a = big;
		const char* b = small;
		while(*b != '\0' && *a == *b)
		{
			++a;
			++b;
		}
		if(*b == '\0')
			return big;
		++big;
	}
	return nullptr;
}

WDynamicPageSession* WDynamicPageConnection::establishSession()
{
	// Check for a cookie in the HTTP header
	WDynamicPageSession* pSession = nullptr;
	if(m_szCookieIncoming[0] != '\0')
	{
		const char* crumb = strstr(m_szCookieIncoming, "GDPSI=");
		if(crumb)
		{
			crumb += 6;
#ifdef WINDOWS
			unsigned long long nSessionID = _strtoui64(crumb, NULL, 10);
#else
			unsigned long long nSessionID = strtoull(crumb, nullptr, 10);
#endif
			pSession = m_pServer->findSession(nSessionID);
			if(!pSession)
			{
				cout << "Unrecognized session id cookie crumb from " << inet_ntoa(ipAddr()) << ": " << remove_cr_lf(crumb) << "\n";
				cout.flush();
			}
		}
		else
		{
			cout << "Cookie with no GDPSI= cookie crumb from " << inet_ntoa(ipAddr()) << ": " << m_szCookieIncoming << "\n";
			cout.flush();
		}
	}

	// Check for a cookie in the content
	if(!pSession)
	{
		const char* crumb = _strnstr(m_pContent, "GDPSI=", m_nContentLength);
		if(crumb)
		{
			crumb += 6;
#ifdef WINDOWS
			unsigned long long nSessionID = _strtoui64(crumb, NULL, 10);
#else
			unsigned long long nSessionID = strtoull(crumb, nullptr, 10);
#endif
			pSession = m_pServer->findSession(nSessionID);
			if(!pSession)
			{
				cout << "Unrecognized session id cookie crumb from " << inet_ntoa(ipAddr()) << "\n";
				cout.flush();
			}
		}
	}

	// Make a new session
	if(!pSession)
	{
		// Make a new cookie
		unsigned long long nSessionID = (unsigned long long)m_pServer->prng()->next() ^ (unsigned long long)(WTime::seconds() * 10000);
		std::ostringstream os;
		os << "GDPSI=";
		os << nSessionID;
		string tmp = os.str();
		setCookie(tmp.c_str(), true);
		pSession = m_pServer->makeNewSession(nSessionID);
	}
	pSession->setConnection(this); // just sets a pointer back to this object

	return pSession;
}

// virtual
void WDynamicPageConnection::doGet(ostream& response)
{
	// Set up the session
	WDynamicPageSession* pSession = establishSession();

	// Handle the request
	setContentType("text/html");
	handleRequest(pSession, response);
}

// virtual
void WDynamicPageConnection::doPost(ostream& response)
{
	WDynamicPageSession* pSession = establishSession();
	handleRequest(pSession, response);
}

// virtual
void WDynamicPageConnection::setHeaders(const char* szUrl, const char* szParams)
{
	// todo: write me
}

void WDynamicPageConnection::sendFile(const char* szMimeType, const char* szFilename, ostream& response)
{
	// Load the file
	size_t nSize;
	char* pFile = WFile::loadFile(szFilename, &nSize);
	std::unique_ptr<char[]> hFile(pFile);

	// Set the headers
	setContentType(szMimeType);
	setModifiedTime(WFile::modifiedTime(szFilename));

	// Send the file
	response.write(pFile, nSize);
}

void WDynamicPageConnection::sendFileSafe(const char* szJailPath, const char* szLocalPath, ostream& response)
{
	//cout << "Requested file: " << szLocalPath;
	//cout.flush();

	// Make sure the file is within the jail
	size_t jailLen = strlen(szJailPath);
	size_t localLen = strlen(szLocalPath);
	WTEMPBUF(char, buf, jailLen + localLen + 1);
	strcpy(buf, szJailPath);
	strcpy(buf + jailLen, szLocalPath);
	WFile::condensePath(buf);
	if(strncmp(buf, szJailPath, jailLen) != 0)
	{
		//cout << "Denied because " << buf << " it is not within " << szJailPath << "\n";
		return;
	}

	// Send the file
	if(WFile::doesFileExist(buf))
	{
		// Send the file
		try
		{
			sendFile(extensionToMimeType(buf), buf, response);
			//cout << ", Sent: " << buf << "\n";
			//cout.flush();
		}
		catch(const char* szError)
		{
			//cout << ", Error sending file: " << buf << ", " << szError << "\n";
			//cout.flush();
			return;
		}
	}
	else
	{
		//cout << ", Not found: " << buf << "\n";
		//cout.flush();
		response << "404 - not found.<br><br>\n";
	}
}

const char* g_pExtensionToMimeTypeHackTable[] =
  {
    ".png",
    "image/png",
    ".js",
    "text/javascript",
    ".jpg",
    "image/jpeg",
    ".gif",
    "image/gif",
};

// static
const char* WDynamicPageConnection::extensionToMimeType(const char* szFilename)
{
	PathData pd;
	WFile::parsePath(szFilename, &pd);
	const char* szExt = szFilename + pd.extStart;
	const char* szMimeType = "text/html";
	for(size_t i = 0; i < sizeof(g_pExtensionToMimeTypeHackTable) / sizeof(const char*); i += 2)
	{
		if(_stricmp(szExt, g_pExtensionToMimeTypeHackTable[i]) == 0)
		{
			szMimeType = g_pExtensionToMimeTypeHackTable[i + 1];
			break;
		}
	}
	return szMimeType;
}


// ------------------------------------------------------

WDynamicPageServer::WDynamicPageServer(int port, WRand* pRand, bool networkAccessible)
: WHttpServer(port, networkAccessible), m_pRand(pRand)
{
	// Init captcha salt
	int i;
	for(i = 0; i < 14; i++)
	{
		do
		{
			m_daemonSalt[i] = ' ' + (char)m_pRand->next(95);
		} while(m_daemonSalt[i] == '"' || m_daemonSalt[i] == '&' || m_daemonSalt[i] == '<' || m_daemonSalt[i] == '>');
	}
	m_daemonSalt[i] = '\0';
	computePasswordSalt();

	// Determine my address
	char szBuf[300];
	strcpy(szBuf, "http://");
	try
	{
		WTCPServer::hostName(szBuf + 7, 256);
	}
	catch(...)
	{
		strcpy(szBuf + 7, "localhost");
	}
	strcat(szBuf + 7, ":");
	std::ostringstream os;
	os << port;
	string tmp = os.str();
	strcat(szBuf + 8, tmp.c_str());
	m_szMyAddress = new char[strlen(szBuf) + 1];
	strcpy(m_szMyAddress, szBuf);
}

WDynamicPageServer::~WDynamicPageServer()
{
	flushSessions();
	delete[] m_szMyAddress;
}

void WDynamicPageServer::flushSessions()
{
	for(map<unsigned long long, WDynamicPageSession*>::iterator it = m_sessions.begin(); it != m_sessions.end(); it++)
		delete(it->second);
	m_sessions.clear();
}

WDynamicPageSession* WDynamicPageServer::makeNewSession(unsigned long long id)
{
	WDynamicPageSession* pSession = new WDynamicPageSession(id);
	m_sessions.insert(make_pair(id, pSession));
	return pSession;
}

WDynamicPageSession* WDynamicPageServer::findSession(unsigned long long id)
{
	map<unsigned long long, WDynamicPageSession*>::iterator it = m_sessions.find(id);
	if(it == m_sessions.end())
		return nullptr;
	else
		return it->second;
}

void WDynamicPageServer::printSessionIds(std::ostream& stream)
{
	map<unsigned long long, WDynamicPageSession*>::iterator it;
	for(it = m_sessions.begin(); it != m_sessions.end(); it++)
	{
		stream << "	" << it->first << "\n";
	}
	stream.flush();
}

const char* WDynamicPageServer::myAddress()
{
	return m_szMyAddress;
}

const char* WDynamicPageServer::daemonSalt()
{
	return m_daemonSalt;
}

void WDynamicPageServer::setDaemonSalt(const char* szSalt)
{
	if(strlen(szSalt) != 14)
		throw Ex("Salt has unexpected length");
	strcpy(m_daemonSalt, szSalt);
	computePasswordSalt();
}

const char* WDynamicPageServer::passwordSalt()
{
	return m_passwordSalt;
}

void WDynamicPageServer::computePasswordSalt()
{
	unsigned char digest[SHA512_DIGEST_LENGTH];
	sha512_ctx ctx;
	memset(&ctx, '\0', sizeof(sha512_ctx));
	sha512_begin(&ctx);
	sha512_hash((unsigned char*)m_daemonSalt, (unsigned int)strlen(m_daemonSalt), &ctx);
	sha512_hash((unsigned char*)"ajbiekistwgcdpcm", 16, &ctx);
	sha512_end(digest, &ctx);
	for(int i = 0; i < 14; i++)
	{
		char c = digest[i] % 52;
		if(c >= 26)
			c += 6;
		c += 'A';
		m_passwordSalt[i] = c;
	}
	m_passwordSalt[14] = '\0';
}

void WDynamicPageServer::redirect(std::ostream& response, const char* szUrl)
{
	ostringstream& r = reinterpret_cast<ostringstream&>(response);
	r.str("");
	r.clear();
	r << "<html><head><META HTTP-EQUIV=\"Refresh\" CONTENT=\"0; URL=";
	r << szUrl;
	r << "\"></head><body>If your browser doesn't automatically redirect, please click <a href=\"";
	r << szUrl;
	r << "\">here</a>\n";
}

WDomNode* WDynamicPageServer::serialize(WDom* pDoc)
{
	WDomNode* pObj = pDoc->newObj();
	pObj->add(pDoc, "daemonSalt", m_daemonSalt);
	pObj->add(pDoc, "pwSalt", m_passwordSalt);
	WDomNode* pSessionList = pDoc->newList();
	pObj->add(pDoc, "sessions", pSessionList);
	for(std::map<unsigned long long, WDynamicPageSession*>::iterator it = m_sessions.begin(); it != m_sessions.end(); it++)
	{
		WDynamicPageSession* pSession = it->second;
		WDomNode* pSessionNode = pSession->serialize(pDoc);
		pSessionList->add(pDoc, pSessionNode);
	}
	return pObj;
}

void WDynamicPageServer::deserialize(const WDomNode* pNode)
{
	const char* szDaemonSalt = pNode->getString("daemonSalt");
	if(strlen(szDaemonSalt) > 14)
		throw Ex("bad daemon salt size");
	strcpy(m_daemonSalt, szDaemonSalt);
	const char* szPasswordSalt = pNode->getString("pwSalt");
	if(strlen(szPasswordSalt) > 14)
		throw Ex("bad pw salt size");
	strcpy(m_passwordSalt, szPasswordSalt);
	flushSessions();
	WDomNode* pSessionList = pNode->get("sessions");
	WDomListIterator it(pSessionList);
	while(it.remaining() > 0)
	{
		WDomNode* pSessionNode = it.current();
		unsigned long long id = (unsigned long long)pSessionNode->getInt("id");
		WDynamicPageSession* pSession = new WDynamicPageSession(this, pSessionNode);
		m_sessions.insert(std::pair<unsigned long long, WDynamicPageSession*>(id, pSession));
		it.advance();
	}
}
