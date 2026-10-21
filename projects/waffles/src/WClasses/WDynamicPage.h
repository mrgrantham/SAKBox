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

#ifndef __GDYNAMICPAGE_H__
#define __GDYNAMICPAGE_H__

#include "WHttp.h"
#include <stddef.h>
#include <vector>
#include <string>
#include <map>

namespace WClasses
{

class WConstStringHashTable;
class WDynamicPageServer;
class WDynamicPageSession;
class WDynamicPageConnection;
class WRand;
class WDom;



class WDynamicPageSessionExtension
{
public:
	WDynamicPageSessionExtension() {}
	virtual ~WDynamicPageSessionExtension() {}

	/// This method is called when a parent session is deleted,
	/// or when another extension takes the place of this one.
	/// If you consider this extension to be owned by that session,
	/// then you should probably call "delete(this)" in
	/// this method. Otherwise, this method just exists to let
	/// you know that the session will no longer be referring to
	/// this extension.
	virtual void onDisown() = 0;

	/// Marshal this object into a Dom node.
	virtual WDomNode* serialize(WDom* pDoc) = 0;
};


class WDynamicPageSession
{
protected:
	unsigned long long m_id;
	time_t m_tLastAccessed;
	WDynamicPageSessionExtension* m_pExtension;
	WDynamicPageConnection* m_pConnection;

public:
	WDynamicPageSession(unsigned long long id);
	WDynamicPageSession(WDynamicPageServer* pServer, WDomNode* pNode);
	virtual ~WDynamicPageSession();

	/// Returns the server object associated with this session
	WDynamicPageServer* server();

	/// Returns the id associated with this session
	unsigned long long id() { return m_id; }

	/// Use this to store your own custom object with the session.
	/// Your object should inherit from WDynamicPageSessionExtension.
	/// (The extension will not be deleted when the session is deleted,
	/// but the WDynamicPageSessionExtension::onDisown method will be
	/// called, which you can use to delete the extension.)
	void setExtension(WDynamicPageSessionExtension* pExtension);

	/// Stamp the session as having been accessed at the current time
	void onAccess();

	/// Associate the current connection with this session
	void setConnection(WDynamicPageConnection* pConn);

	WDynamicPageConnection* connection() { return m_pConnection; }

	/// Retrieve the extension object that was associated with this
	/// session by a call to setExtension.
	WDynamicPageSessionExtension* extension() { return m_pExtension; }

	/// Marshal this object into a Dom node
	WDomNode* serialize(WDom* pDom);

	// Get the current URL from the connection. (Convenience method.)
	const char* url();

	// Get the current params from the connection. (Convenience method.)
	const char* params();

	// Get the length of the params
	size_t paramsLen();

	// Add a 'cookie' field to pOutNode with this session's cookie.
	// (This is needed because some browsers block cookies in AJAX requests.)
	void addAjaxCookie(WDom& docOut, WDomNode* pOutNode);
};




class WDynamicPageConnection : public WHttpConnection
{
public:
	WDynamicPageServer* m_pServer;


	WDynamicPageConnection(SOCKET sock, WDynamicPageServer* pServer);
	~WDynamicPageConnection() override;

	void doGet(std::ostream& response) override;
	void doPost(std::ostream& response) override;

	/// Concatenates szJailPath+szLocalPath, and makes sure that the result is within szJailPath, then
	/// it automatically determines the mime type from the extension, and sends the file.
	void sendFileSafe(const char* szJailPath, const char* localPath, std::ostream& response);

protected:
	/// This method is called by doGet or doPost when a client requests something from the server
	virtual void handleRequest(WDynamicPageSession* pSession, std::ostream& response) = 0;

	bool hasBeenModifiedSince(const char* szUrl, const char* szDate) override;
	void setHeaders(const char* szUrl, const char* szParams) override;
	WDynamicPageSession* establishSession();

	void sendFile(const char* szMimeType, const char* szFilename, std::ostream& response);

	/// Determines an appropriate mime type for the given filename based on its extension.
	/// (Currently only recognizes a very small number of extensions.)
	static const char* extensionToMimeType(const char* szFilename);
};



class WDynamicPageServer : public WHttpServer
{
protected:
	WRand* m_pRand;
	std::map<unsigned long long, WDynamicPageSession*> m_sessions;
	char* m_szMyAddress;
	char m_daemonSalt[16];
	char m_passwordSalt[16];

public:
	/// SAKBox: only browsers on this computer can connect unless
	/// networkAccessible is true (see WHttpServer).
	WDynamicPageServer(int port, WRand* pRand, bool networkAccessible = false);
	~WDynamicPageServer() override;

	virtual WDynamicPageSessionExtension* deserializeSessionExtension(const WDomNode* pNode)
	{
		throw Ex("deserializeSessionExtension has not be overridden");
	}

	void flushSessions();

	/// Returns the session with the specified id. If no session is found with that id, returns NULL.
	WDynamicPageSession* findSession(unsigned long long id);

	/// Makes a new session with the specified id.
	WDynamicPageSession* makeNewSession(unsigned long long id);

	/// Prints all known session ids to the stream. (This is used for debugging purposes.)
	void printSessionIds(std::ostream& stream);

	/// Returns the account if the password is correct. Returns NULL if not.
	const char* myAddress();

	void setDaemonSalt(const char* szSalt);
	const char* daemonSalt();
	const char* passwordSalt();

	WRand* prng() { return m_pRand; }
	void redirect(std::ostream& response, const char* szUrl);

	WDomNode* serialize(WDom* pDoc);
	void deserialize(const WDomNode* pNode);

protected:
	void computePasswordSalt();
};

} // namespace WClasses

#endif // __GDYNAMICPAGE_H__
