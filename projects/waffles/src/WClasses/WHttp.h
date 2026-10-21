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

#ifndef __GHTTP_H__
#define __GHTTP_H__

#include <time.h>
#include <atomic>
#include <vector>
#include <sstream>
#include <map>
#include <string.h>
#include "WSocket.h"

namespace WClasses
{

class WHttpClientSocket;
class WWebSocketClientSocket;
class WHttpServerSocket;
class WTCPServer;
class WHttpConnection;
class WHeap;
class WConstStringHashTable;


/// This class allows you to get files using the HTTP protocol
class WHttpClient
{
public:
	enum Status
	{
		Downloading,
		Error,
		NotFound,
		Done,
		Aborted,
	};

protected:
	char* m_pReceiveBuf;
	char m_szHeaderBuf[258];
	char m_szServer[256];
	char m_szClientName[32];
	size_t m_nHeaderPos;
	size_t m_nContentSize;
	bool m_bChunked;
	bool m_aborted;
	unsigned char* m_pData;
	size_t m_nDataPos;
	WHttpClientSocket* m_pSocket;
	Status m_status;
	std::ostringstream m_chunkBuf;
	bool m_bPastHeader;
	char* m_szRedirect;
	double m_dLastReceiveTime;
	bool m_bAmCurrentlyDoingJustHeaders;

public:
	WHttpClient();
	virtual ~WHttpClient();

	/// Gets a file from the specified URL. You are responsible to delete[] the data this returns. The size of the
	/// data is returned at *pOutSize. An exception is thrown if any error occurs. sleepMiliSecs specifies how long
	/// to sleep while waiting for more data. If no progress is made for timeoutSecs seconds, an exception is thrown.
	/// (This is a high-level method that calls the other low-level methods of this class. Generally, you will either
	/// use just this method, or all of the other methods.)
	unsigned char* get(const char* url, size_t* pOutSize, unsigned int sleepMiliSecs = 200, unsigned int timeoutSecs = 30);

	/// Send a request to get a file.  Returns immediately (before the file
	/// is downloaded). If headersOnly is true, then only the headers will
	/// be requested, and not the file. Returns true if the request was
	/// sent successfully. Returns false if it could not connect to the
	/// specified URL.
	bool sendGetRequest(const char* szUrl, bool headersOnly = false);

	/// See what the status of the download is.  If everything is going okay,
	/// it will return "Downloading" while downloading and "Done" when the file
	/// is available.  pfProgress is an optional parameter.  If it is non-NULL,
	/// it will return a number between 0 and 1 that indicates the ratio of
	/// content (not including header data) already downloaded.
	Status status(float* pfProgress);

	/// Don't call this until the status is "Done".  It returns a pointer to the
	/// file that was downloaded.  The buffer will be deleted when this object is
	/// deleted, so if you want to retain the buffer, call releaseData instead.
	unsigned char* data(size_t* pnSize);

	/// Just like getData except it forgets about the buffer so you'll have to
	/// delete it yourself.
	unsigned char* releaseData(size_t* pnSize);

	/// This is called when the connection is lost
	void onLoseConnection();

	/// Specify the name that the client uses to identify itself when requesting a file. The default is "WHttpClient/1.0".
	void setClientName(const char* szClientName);

	/// The client may call this method to abort the download
	void abort();

protected:
	/// This method is called whenever a chunk of data is received
	virtual void onReceiveData(const unsigned char* pData, size_t nLen) {}

	void processHeader(const unsigned char* szData, size_t nSize);
	void processBody(const unsigned char* szData, size_t nSize);
	void processChunkBody(const unsigned char* szData, size_t nSize);
	void gimmeWhatYouGot();
};

class WWebSocketClient
{
protected:
	WWebSocketClientSocket* m_pSocket;

public:
	WWebSocketClient();
	~WWebSocketClient();

	bool get(const char* szUrl);

	void onLoseConnection();
};



#define MAX_HEADER_LEN 8192
#define MAX_URL_LEN 2048
#define MAX_COOKIE_SIZE 4096
#define MAX_DATE_LEN 128

/// SAKBox: the largest Content-Length accepted, by the server (a POST body)
/// and the client (a reply): 64 MB
#define MAX_HTTP_CONTENT_LENGTH ((size_t)64 * 1024 * 1024)

/// SAKBox: reads a Content-Length header's value: a whole decimal number
/// (spaces and the line end around it are allowed) of at most
/// MAX_HTTP_CONTENT_LENGTH. Returns false for anything else, such as "-1",
/// "12x" or a number too big.
bool parseHttpContentLength(const char* szValue, size_t* pOut);

/// SAKBox: the port in a URL ("http://host:8080/page"), or 80 if it has none.
/// Throws if the port isn't a number from 1 to 65535.
int WHttp_urlPort(const char* szUrl);
#define MAX_MIME_LEN 64

/// Each WHttpConnection represents an HTTP connection between a client and the server.
/// A WHTTPServer is a collection of WHTTPConnection objects.
/// To implement a HTTP server, you will typically override the doGet and doPost methods of this class to generate web pages.
class WHttpConnection : public WTCPConnection
{
public:
	enum RequestType
	{
		None,
		Get,
		Head,
		Post,
	};

	size_t m_nPos;
	char m_szLine[MAX_HEADER_LEN];
	char m_szUrl[MAX_URL_LEN];
	char m_szParams[MAX_URL_LEN];
	char m_szDate[MAX_DATE_LEN];
	char m_szContentType[MAX_MIME_LEN];
	std::string m_sAllowOrigin;
	char m_szCookieIncoming[MAX_COOKIE_SIZE];
	char m_szCookieOutgoing[MAX_COOKIE_SIZE];
	bool m_bPersistCookie;
	char* m_pPostBuffer;
	char* m_pContent; // points to m_pPostBuffer when a POST is invoked. points to m_szParams when a GET is invoked.
	RequestType m_eRequestType;
	size_t m_nContentLength;
	time_t m_modifiedTime;

	/// General-purpose constructor
	WHttpConnection(SOCKET sock);

	~WHttpConnection() override;

	/// Clear all the buffers in this object.
	void reset();

	/// Specify the type of the content that is being sent to the client.
	void setContentType(const char* szContentType);

	/// Allow AJAX requests from the specified origin.
	void allowOrigin(const char* szOrigin);

	/// Specify a cookie to send to the client with this response.
	void setCookie(const char* szPayload, bool bPersist);

	/// The primary purpose of this method is to push a response into pResponse.
	/// Typically this method will call SetHeaders.
	virtual void doGet(std::ostream& response) = 0;

	/// This method takes ownership of pData. Don't forget to delete it. When the POST is
	/// caused by an HTML form, it's common for this method to just call DoGet (passing
	/// pData for szParams) and then delete pData. (For convenience, a '\0' is already appended
	/// at the end of pData.)
	virtual void doPost(std::ostream& response) = 0;

	/// This is called when the client does a conditional GET. It should return true
	/// if you wish to re-send the file, and DoGet will be called.
	virtual bool hasBeenModifiedSince(const char* szUrl, const char* szDate) = 0;

	/// This method should set the content type and the date headers, and any other
	/// headers deemed necessary
	virtual void setHeaders(const char* szUrl, const char* szParams) = 0;

	/// Sets the date (modified time) to be sent with the file so the client can cache it
	void setModifiedTime(time_t t) { m_modifiedTime = t; }
};



/// This class allows you to implement a simple HTTP daemon
class WHttpServer
{
protected:
	char* m_pReceiveBuf;
	WHttpServerSocket* m_pSocket;
	std::ostringstream m_stream;
	std::atomic<bool> m_keepServing{true};

	/// SAKBox: called by serveRequests whenever there's no request to handle,
	/// with the time since the last request (or since serving began). A server
	/// that shuts down after a while, or does maintenance, overrides it.
	virtual void onIdle(double secondsSinceLastRequest) {}

public:
	/// Listens on nPort (0 for any free one). SAKBox: only browsers on this
	/// computer can connect (localhost), unless networkAccessible is true,
	/// which lets any device on the network connect.
	WHttpServer(int nPort, bool networkAccessible = false);
	virtual ~WHttpServer();

	/// You should call this method constantly inside the main loop.
	/// It returns true if it did anything, and false if it didn't, so
	/// if it returns false you may want to sleep for a little while.
	bool process();

	/// SAKBox: handles requests until stopServing is called or the program is
	/// asked to stop (SIGINT or SIGTERM; SIGPIPE, a client hanging up, doesn't
	/// stop it). When there's no request it calls onIdle, then sleeps for a
	/// tenth of a second. (Each Waffles server used to run this loop itself.)
	void serveRequests();

	/// Makes serveRequests return, after the request it's handling, if any.
	/// It can be called from a request handler, onIdle or another thread.
	void stopServing() { m_keepServing = false; }

	/// Unescapes a URL. (i.e. replace "%20" with " ", etc.). szOut should point
	/// to a buffer at least as big as szIn (including the null terminator). This
	/// will stop when it hits the null-terminator in szIn or when nInLen characters
	/// have been parsed. So if szIn is null-terminated, you can safely pass in a
	/// huge arbitrary value for nInLen.
	static void unescapeUrl(char* szOut, const char* szIn, size_t nInLen);

	/// This is a rather hacky method that parses the parameters of a specific upload-file form
	static bool parseFileParam(const char* pParams, size_t nParamsLen, const char** ppFilename, size_t* pFilenameLen, const unsigned char** ppFile, size_t* pFileLen);

	/// Returns a reference to the socket on which this server listens
	WTCPServer* socket() { return (WTCPServer*)m_pSocket; }

	/// This method should return a new instance of an object that inherrits from WHttpConnection.
	virtual WHttpConnection* makeConnection(SOCKET s) = 0;

protected:
	virtual void onProcessLine(WHttpConnection* pConn, const char* szLine) {}
	void processPostData(WHttpConnection* pConn, const unsigned char* pData, size_t nDataSize);
	void processHeaderLine(WHttpConnection* pConn, const char* szLine);
	void beginRequest(WHttpConnection* pConn, int eType, const char* szIn);
	void sendResponse(WHttpConnection* pConn);
	void sendNotModifiedResponse(WHttpConnection* pConn);
	void onReceiveFullPostRequest(WHttpConnection* pConn);
};


struct strComp
{
	bool operator()(const char* a, const char* b) const { return strcmp(a, b) < 0; }
};


/// A class for parsing the name/value pairs that follow the "?" in a URL.
class WHttpParamParser
{
protected:
	WHeap* m_pHeap;
	std::map<const char*, const char*, strComp> m_map;

public:
	/// szParams should be everything in the URL after the "?".
	/// If scrub is true, then the values will be scrubbed: characters other
	/// than letters, digits and space ! # $ % ( ) * + , - . / : < = > ? @
	/// are replaced with '_'. (SAKBox: this used to list fewer than the code
	/// keeps; see isSafeParamChar in WHttp.cpp.)
	WHttpParamParser(const char* szParams, bool scrub = true);
	~WHttpParamParser();

	/// Returns the value associated with the specified name. Returns NULL if the name is not found.
	const char* find(const char* szName);

	/// Returns a map of the name/value pairs
	std::map<const char*, const char*, strComp>& map() { return m_map; }

	/// Returns a string representation of all the parameters
	std::string to_str(bool html);

protected:
	static void scrubValue(char* value);
};


class WHttpMultipartParser
{
protected:
	const char* m_pRawData;
	size_t m_sentinelLen;
	size_t m_repeatLen;
	size_t m_pos;
	size_t m_len;

public:
	WHttpMultipartParser(const char* pRawData, size_t len);
	~WHttpMultipartParser();

	bool next(size_t* pNameStart, size_t* pNameLen, size_t* pValueStart, size_t* pValueLen, size_t* pFilenameStart, size_t* pFilenameLen);
};

} // namespace WClasses

#endif // __GHTTP_H__
