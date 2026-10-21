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
#include <string>
#include <WClasses/WFile.h>
#include <WClasses/WDynamicPage.h>
#include "recommender.h"

class Account;
class Connection;

using namespace WClasses;

class Server : public WDynamicPageServer
{
protected:
	std::map<std::string, size_t> m_username_to_index; // Mapping from username to account index
	std::vector<Account*> m_accounts;
	std::set<std::string> m_banned_addresses;
	NeuralRecommender m_recommender; // The recommender system
	WFileCache m_fileCache;
	WJsonAsADatabase* m_pJaad;
	double m_lastMaintenance; // (WTime::seconds)

	/// Does maintenance every four hours, between requests
	void onIdle(double secondsSinceLastRequest) override;

public:
	std::string m_basePath;
	std::string m_toolsPath;

	Server(int port, WRand* pRand);
	~Server() override;
	NeuralRecommender& recommender() { return m_recommender; }
	WJsonAsADatabase& jaad() { return *m_pJaad; }
	void saveState();
	void doSomeRecommenderTraining();
	std::vector<Account*>& accounts() { return m_accounts; }
	bool isUsernameTaken(const char* szUsername);
	Account* findAccount(const char* szUsername);
	size_t account_count() { return m_accounts.size(); }
	Account* get_account(size_t index)
	{
		if(index >= m_accounts.size()) throw Ex("Account index out of range");
		return m_accounts[index];
	}
	size_t user_id(const char* szUsername);
	bool isValidUsername(const char* szUsername);
	Account* newAccount(const char* szUsername, const char* szPasswordHash);
	//void deleteAccount(Account* pAccount);
	bool onRenameAccount(const char* szOldName, const char* szNewName);
	WDomNode* serializeState(WDom* pDoc);
	void deserializeState(const WDomNode* pNode);
	WDynamicPageSessionExtension* deserializeSessionExtension(const WDomNode* pNode) override;
	WDynamicPageConnection* makeConnection(SOCKET sock) override;
	std::string& cache(const char* szFilename);
	static void getLocalStorageFolder(char* buf);

	/// Make the header part of standardized pages
	static void makeHeader(WDynamicPageSession* pSession, std::ostream& response, const char* szParamsMessage);

	/// Make the footer part of standardized pages
	static void makeFooter(WDynamicPageSession* pSession, std::ostream& response);

	/// Prints a timestamp and message to stdout, and flushes stdout
	void log(const char* message);

	/// This is called at periodic intervals, a few times per day
	void do_maintenance();

	/// Return true iff this account or its current IP address has been banned
	bool isBanned(Connection* pConnection, Account* pAccount);
	void loadBannedAddresses();
	void saveBannedAddresses();
};






class Account
{
protected:
	std::string m_username;
	std::string m_passwordHash;
	std::string m_path; // Within .../usercontent/username/pages/. Does not start with a slash.
	User* m_pUser; // A redundant pointer to the User object that goes with this account
	bool m_admin;
	bool m_assistant;
	bool m_banned;

public:
	Account(const char* szUsername, const char* szPasswordHash)
	: m_username(szUsername), m_passwordHash(szPasswordHash), m_pUser(nullptr), m_admin(false), m_assistant(false), m_banned(false)
	{
	}

	virtual ~Account()
	{
	}

	bool isAdmin()
	{
		return m_admin;
	}

	bool isAssistant()
	{
		return m_assistant;
	}

	bool isBanned()
	{
		return m_banned;
	}

	void makeAdmin(bool admin)
	{
		m_admin = admin;
	}

	void makeAssistant(bool assistant)
	{
		m_assistant = assistant;
	}

	void makeBanned(bool banned)
	{
		m_banned = banned;
	}

	static Account* fromDom(WDomNode* pNode, WRand& rand)
	{
		const char* un = pNode->getString("username");
		const char* pw = pNode->getString("password");
		Account* pAccount = new Account(un, pw);
		if(pNode->getIfExists("admin"))
			pAccount->makeAdmin(true);
		if(pNode->getIfExists("assistant"))
			pAccount->makeAssistant(true);
		if(pNode->getIfExists("banned"))
			pAccount->makeBanned(true);
		return pAccount;
	}

	WDomNode* toDom(WDom* pDoc)
	{
		WDomNode* pAccount = pDoc->newObj();
		pAccount->add(pDoc, "username", m_username.c_str());
		pAccount->add(pDoc, "password", m_passwordHash.c_str());
		if(m_admin)
			pAccount->add(pDoc, "admin", "true");
		if(m_assistant)
			pAccount->add(pDoc, "assistant", "true");
		if(m_banned)
			pAccount->add(pDoc, "banned", "true");
		return pAccount;
	}

	std::string& path() { return m_path; }
	const char* username() { return m_username.c_str(); }
	const char* passwordHash() { return m_passwordHash.c_str(); }

	User* getUser(NeuralRecommender& recommender)
	{
		if(!m_pUser)
			m_pUser = recommender.findOrMakeUser(m_username.c_str());
		return m_pUser;
	}

	User* user() { return m_pUser; }
	void setUser(User* pUser) { m_pUser = pUser; }

	bool changeUsername(Server* pServer, const char* szNewName)
	{
		if(!pServer->onRenameAccount(m_username.c_str(), szNewName))
			return false;
		m_username = szNewName;
		return true;
	}

	void changePasswordHash(Server* pServer, const char* szNewPasswordHash)
	{
		m_passwordHash = szNewPasswordHash;
	}

	bool doesHavePassword()
	{
		return m_passwordHash.length() > 0;
	}
};




class Terminal : public WDynamicPageSessionExtension
{
protected:
	size_t m_currentAccount; // The currently logged-in account. (nullptr if not logged in.)
	std::vector<Account*> m_accounts; // All accounts associated with this machine
	std::vector<bool> m_requirePassword; // Whether or not the password is required on this machine

public:
	Terminal() : WDynamicPageSessionExtension(), m_currentAccount((size_t)-1)
	{
	}

	Terminal(const WDomNode* pNode, Server* pServer)
	{
		m_currentAccount = pNode->getInt("loggedin");
		WDomListIterator it(pNode->get("accounts"));
		while(it.remaining() > 0)
		{
			const char* username = it.currentString();
			Account* pAccount = pServer->findAccount(username);
			if(!pAccount)
				throw Ex("Account not found for ", username);
			m_accounts.push_back(pAccount);
			it.advance();
		}
		WDomListIterator it2(pNode->get("reqpw"));
		while(it2.remaining() > 0)
		{
			bool b = it2.currentBool();
			m_requirePassword.push_back(b);
			it2.advance();
		}
		if(m_accounts.size() != m_requirePassword.size())
			throw Ex("Mismatching sizes");
	}

	~Terminal() override
	{
		for(size_t i = 0; i < m_accounts.size(); i++)
			delete(m_accounts[i]);
	}

	/// Called when the sessions are destroyed, or a new WDynamicPageSessionExtension is
	/// explicitly associated with this cookie.
	void onDisown() override
	{
	}

	WDomNode* serialize(WDom* pDoc) override
	{
		WDomNode* pObj = pDoc->newObj();
		pObj->add(pDoc, "loggedin", m_currentAccount);
		WDomNode* pAccList = pDoc->newList();
		pObj->add(pDoc, "accounts", pAccList);
		for(size_t i = 0; i < m_accounts.size(); i++)
			pAccList->add(pDoc, m_accounts[i]->username());
		WDomNode* pReqList = pDoc->newList();
		pObj->add(pDoc, "reqpw", pReqList);
		for(size_t i = 0; i < m_requirePassword.size(); i++)
			pReqList->add(pDoc, m_requirePassword[i]);
		return pObj;
	}

	/// Returns the current account, or nullptr if not logged in.
	Account* currentAccount()
	{
		return (m_currentAccount == (size_t)-1 ? nullptr : m_accounts[m_currentAccount]);
	}

	size_t accountCount() { return m_accounts.size(); }
	Account* account(size_t index) { return m_accounts[index]; }
	bool requirePassword() { return m_requirePassword[m_currentAccount]; }
	bool requirePassword(size_t index) { return m_requirePassword[index]; }

	void logOut()
	{
		m_currentAccount = (size_t)-1;
	}

	bool forgetAccount(const char* szUsername)
	{
		for(size_t i = 0; i < m_accounts.size(); i++)
		{
			if(strcmp(m_accounts[i]->username(), szUsername) == 0)
			{
				m_accounts.erase(m_accounts.begin() + i);
				m_requirePassword.erase(m_requirePassword.begin() + i);
				return true;
			}
		}
		return false;
	}

	static std::string generateUsername(WRand& rand);

	Account* makeNewAccount(Server* pServer)
	{
		size_t new_account_index = pServer->account_count();
		Account* pNewAccount = nullptr;
		for(size_t patience = 10; patience > 0; patience--)
		{
			std::string userName = generateUsername(*pServer->prng());
			if(!pServer->findAccount(userName.c_str()))
			{
				pNewAccount = pServer->newAccount(userName.c_str(), nullptr);
				if(!pNewAccount)
					throw Ex("Failed to create account");
				break;
			}
		}
		if(!pNewAccount)
			throw Ex("Failed to generate a unique username");
		m_currentAccount = m_accounts.size();
		m_accounts.push_back(pNewAccount);
		m_requirePassword.push_back(false);
		if(new_account_index == 0)
			pNewAccount->makeAdmin(true);
		return pNewAccount;
	}

	void setRequirePassword(bool require)
	{
		if(m_currentAccount == (size_t)-1)
			throw Ex("not logged in");
		m_requirePassword[m_currentAccount] = require;
	}

	bool logIn(Account* pAccount, const char* szPasswordHash)
	{
		bool found = false;
		bool requirePassword = true;
		size_t accountIndex = (size_t)-1;
		for(size_t i = 0; i < m_accounts.size() && !found; i++)
		{
			if(m_accounts[i] == pAccount)
			{
				found = true;
				accountIndex = i;
				requirePassword = m_requirePassword[i];
			}
		}
		if(requirePassword)
		{
			if(!pAccount->passwordHash())
				return false;
			if(strlen(pAccount->passwordHash()) < 1)
				return false;
			if(!szPasswordHash)
				return false;
			if(strcmp(pAccount->passwordHash(), szPasswordHash) != 0)
				return false;
		}
		if(!found)
		{
			accountIndex = m_accounts.size();
			m_accounts.push_back(pAccount);
			m_requirePassword.push_back(requirePassword);
		}
		m_currentAccount = accountIndex;
		return true;
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
	void handleAjax(Server* pServer, WDynamicPageSession* pSession, std::ostream& response);
	void handleTools(Server* pServer, WDynamicPageSession* pSession, std::ostream& response);
	bool is_admin(Terminal* pTerminal, const char** ppParamsMessage);
	const char* processParams(WDynamicPageSession* pSession);
};







Terminal* getTerminal(WDynamicPageSession* pSession);

Account* getAccount(WDynamicPageSession* pSession);



#endif // SERVER
