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

#include <stdio.h>
#include <stdlib.h>
#ifdef WINDOWS
#include <windows.h>
#include <process.h>
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <WClasses/WDynamicPage.h>
#include <WClasses/WApp.h>
#include <WClasses/WDom.h>
#include <WClasses/WString.h>
#include <WClasses/WHolders.h>
#include <WClasses/WFile.h>
#include <WClasses/WTime.h>
#include <WClasses/WRand.h>
#include <WClasses/WHashTable.h>
#include <wchar.h>
#include <string>
#include <exception>
#include <iostream>
#include <sstream>

using namespace WClasses;
using std::cout;
using std::cerr;
using std::string;
using std::ostream;


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

class Server : public WDynamicPageServer
{
public:
	std::string m_basePath;

	Server(int port, WRand* pRand);
	~Server() override {}

	WDynamicPageConnection* makeConnection(SOCKET sock) override
	{
		return new Connection(sock, this);
	}
};


// virtual
void Connection::handleRequest(WDynamicPageSession* pSession, std::ostream& response)
{
	if(strcmp(m_szUrl, "/favicon.ico") == 0)
		return;
	else if(strncmp(m_szUrl, "/hello", 6) == 0)
	{
		response << "<html><head>\n";
		response << "	<title>My Hello Site</title>\n";
		response << "	<link rel=\"stylesheet\" type=\"text/css\" href=\"/style.css\" />\n";
		response << "</head><body>\n";
		response << "	Hello Web! <img src=\"smiley.png\"><br>\n";
		response << "</body></html>\n";
	}
	else if(strcmp(m_szUrl, "/smiley.png") == 0 || strcmp(m_szUrl, "/style.css") == 0) // SAKBox: the page's style sheet was a 404
		sendFileSafe(((Server*)m_pServer)->m_basePath.c_str(), m_szUrl + 1, response);
	else
		response << "<h1>404 - Not found!</h1>";
}


Server::Server(int port, WRand* pRand) : WDynamicPageServer(port, pRand, true) // SAKBox: opens myAddress(), the network name, so open to the network
{
	char buf[300];
	WTime::asciiTime(buf, 256, false);
	cout << "Server starting at: " << buf << "\n";
	m_basePath = "projects/waffles/demos/hello_web/bin/web/"; // SAKBox: where `bazel run` puts it (was next to the program)
}

void getLocalStorageFolder(char* buf)
{
	if(!WFile::localStorageDirectory(buf))
		throw Ex("Failed to find local storage folder");
	strcat(buf, "/.hello/");
	WFile::makeDir(buf);
	if(!WFile::doesDirExist(buf))
		throw Ex("Failed to create folder in storage area");
}

void LaunchBrowser(const char* szAddress)
{
	int addrLen = strlen(szAddress);
	WTEMPBUF(char, szUrl, addrLen + 20);
	strcpy(szUrl, szAddress);
	strcpy(szUrl + addrLen, "/hello");
	if(!WApp::openUrlInBrowser(szUrl))
	{
		cout << "Failed to open the URL: " << szUrl << "\nPlease open this URL manually.\n";
		cout.flush();
	}
}

void doit(void* pArg)
{
	int port = 8989;
	unsigned int seed = getpid() * (unsigned int)time(nullptr);
	WRand prng(seed);
	Server server(port, &prng);
	LaunchBrowser(server.myAddress());
	// Pump incoming HTTP requests (this is the main loop)
	cout << "Server is up\n";
	cout.flush();
	server.serveRequests(); // SAKBox: was go(), which WDynamicPageServer no longer has
	cout << "Goodbye.\n";
}

void doItAsDaemon()
{
	char path[300];
	getLocalStorageFolder(path);
	string s1 = path;
	s1 += "stdout.log";
	string s2 = path;
	s2 += "stderr.log";
	WApp::launchDaemon(doit, path, s1.c_str(), s2.c_str());
	cout << "Daemon running.\n	stdout >> " << s1.c_str() << "\n	stderr >> " << s2.c_str() << "\n";
}

int main(int nArgs, char* pArgs[])
{
	int nRet = 1;
	try
	{
		doit(nullptr);
		//doItAsDaemon();
	}
	catch(std::exception& e)
	{
		cerr << e.what() << "\n";
	}
	return nRet;
}
