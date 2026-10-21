// -------------------------------------------------------------
// The contents of this file may be distributed under the CC0
// license (http://creativecommons.org/publicdomain/zero/1.0/),
// or any compatible license, including (but not limited to) all
// OSI-approved licenses (http://www.opensource.org/licenses).
// -------------------------------------------------------------

#ifndef SATELLITE_H
#define SATELLITE_H

#include <time.h>
#include "../../WClasses/WBlob.h"

#define MAX_PACKET_SIZE 16384
#define BUF_SIZE 4096

namespace WClasses
{
class WPackageClient;
}

class Satellite
{
protected:
	bool m_keepRunning;
	int m_timeoutSecs;
	time_t m_lastPacketTime;
	WClasses::WPackageClient* m_pSocket;
	WClasses::WBlobIncoming m_blobIn;
	WClasses::WBlobOutgoing m_blobOut;

	// File transfer variables
	FILE* m_pFile;
	std::string m_filename;
	unsigned long long m_fileLen;
	size_t m_filePos;
	unsigned char m_fileHash[64];
	unsigned char* m_pBuf;
	bool m_uploading;

public:
	Satellite();
	~Satellite();

	void Connect(const char* szAddr, int port, int timeoutSecs);
	void Disconnect();

	// Returns true if there were packets to process
	bool Process();

	void Go(const char* szAddr, int port, int connectGapMins, int timeoutSecs);

protected:
	void HandlePacket(WClasses::WBlobIncoming* pBlob);
	void OnServerWantsFileList(WClasses::WBlobIncoming* pBlob);
	void OnServerWantsToDisconnect(WClasses::WBlobIncoming* pBlobIn);
	void OnServerWantsPing(WClasses::WBlobIncoming* pBlobIn);
	void OnServerWantsDirectoryChange(WClasses::WBlobIncoming* pBlobIn);
	void OnServerWantsToSendFile(WClasses::WBlobIncoming* pBlobIn);
	void OnServerSendsFileChunk(WClasses::WBlobIncoming* pBlobIn);
	void OnServerDoneSendingFile(WClasses::WBlobIncoming* pBlobIn);
	void OnServerWantsToDownloadFile(WClasses::WBlobIncoming* pBlobIn);
	void TransferFileChunk();
	void CompleteFileTransfer();
	void ExecuteCommand(WClasses::WBlobIncoming* pBlobIn);
	void Delete_File(WClasses::WBlobIncoming* pBlobIn);
};



void Sha512DigestFile(unsigned char* pOut64ByteHash, const char* filename);

void doShellCommandCenter(int port);

#endif // SATELLITE_H
