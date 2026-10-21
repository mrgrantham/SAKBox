/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
    anonymous contributors,

  to the public domain (http://creativecommons.org/publicdomain/zero/1.0/).

  Note that some moral obligations still exist in the absence of legal ones.
  For example, it would still be dishonest to deliberately misrepresent the
  origin of a work. Although we impose no legal requirements to obtain a
  license, it is beseeming for those who build on the works of others to
  give back useful improvements, or pay it forward in their own field. If
  you would like to cite us, a published paper about Waffles can be found
  at http://jmlr.org/papers/volume12/gashler11a/gashler11a.pdf. If you find
  our code to be useful, the Waffles team would love to hear how you use it.
*/

#ifndef SERVER
#define SERVER

#include <map>
#include <iostream>
#include <string>
#include <WClasses/WFile.h>
#include <WClasses/WDynamicPage.h>

class Account;

using namespace WClasses;


class Server : public WDynamicPageServer
{
protected:
	WFileCache m_fileCache;

	/// Stops once the browser has had the page for a while
	void onIdle(double secondsSinceLastRequest) override
	{
		if(secondsSinceLastRequest > 8.0)
		{
			std::cout << "Whelp, the browser should probably have it all by now, so I'm exiting...\n";
			stopServing();
		}
	}

public:
	std::string m_basePath;

	Server(int port, WRand* pRand);
	~Server() override;
	WDynamicPageSessionExtension* deserializeSessionExtension(const WDomNode* pNode) override;
	WDynamicPageConnection* makeConnection(SOCKET sock) override;
};






class Terminal : public WDynamicPageSessionExtension
{
public:
	Terminal() : WDynamicPageSessionExtension()
	{
	}

	Terminal(const WDomNode* pNode, Server* pServer)
	{
	}

	~Terminal() override
	{
	}

	WDomNode* serialize(WDom* pDoc) override
	{
		return nullptr;
	}

	/// Called when the sessions are destroyed, or a new WDynamicPageSessionExtension is
	/// explicitly associated with this cookie.
	void onDisown() override
	{
	}
};


class Connection : public WDynamicPageConnection
{
public:
	Connection(SOCKET sock, WDynamicPageServer* pServer) : WDynamicPageConnection(sock, pServer)
	{
	}

	~Connection() override
	{
	}

	void handleRequest(WDynamicPageSession* pSession, std::ostream& response) override;
};







Terminal* getTerminal(WDynamicPageSession* pSession);

Account* getAccount(WDynamicPageSession* pSession);



#endif // SERVER
