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
#include <WClasses/GFile.h>
#include <WClasses/GDynamicPage.h>

class Account;

using namespace GClasses;


class Server : public GDynamicPageServer
{
protected:
	GFileCache m_fileCache;

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

	Server(int port, GRand* pRand);
	~Server() override;
	GDynamicPageSessionExtension* deserializeSessionExtension(const GDomNode* pNode) override;
	GDynamicPageConnection* makeConnection(SOCKET sock) override;
};






class Terminal : public GDynamicPageSessionExtension
{
public:
	Terminal() : GDynamicPageSessionExtension()
	{
	}

	Terminal(const GDomNode* pNode, Server* pServer)
	{
	}

	~Terminal() override
	{
	}

	GDomNode* serialize(GDom* pDoc) override
	{
		return nullptr;
	}

	/// Called when the sessions are destroyed, or a new GDynamicPageSessionExtension is
	/// explicitly associated with this cookie.
	void onDisown() override
	{
	}
};


class Connection : public GDynamicPageConnection
{
public:
	Connection(SOCKET sock, GDynamicPageServer* pServer) : GDynamicPageConnection(sock, pServer)
	{
	}

	~Connection() override
	{
	}

	void handleRequest(GDynamicPageSession* pSession, std::ostream& response) override;
};







Terminal* getTerminal(GDynamicPageSession* pSession);

Account* getAccount(GDynamicPageSession* pSession);



#endif // SERVER
