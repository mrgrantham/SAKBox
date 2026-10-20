/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
    Eric Moyer,
    Michael R. Smith,
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

#include "GApp.h"
#include "GError.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef WINDOWS
#include <windows.h>
#include <direct.h>
#include <float.h>
#else
#define _GNU_SOURCE 1
#include <fenv.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
//#	include <termios.h>
//#	include <fcntl.h>
#ifndef __linux__
#ifndef __APPLE__
#include <xmmintrin.h>
#endif
#ifndef __FreeBSD__
#include <mach-o/dyld.h>
#endif
#endif
#endif
#include "GHolders.h"
#include "GFile.h"
#include <errno.h>
#include <iostream>
#include <string>
#include <string.h>
#include <sstream>
#include <fstream>

using namespace GClasses;
using std::cout;
using std::string;

GPipe::GPipe()
: m_handle(INVALID_HANDLE)
{
}

GPipe::~GPipe()
{
	if(m_handle != INVALID_HANDLE)
	{
#ifdef WINDOWS
		CloseHandle(m_handle);
#else
		close(m_handle);
#endif
	}
}

void GPipe::set(HANDLE h)
{
	if(m_handle != INVALID_HANDLE)
	{
#ifdef WINDOWS
		CloseHandle(m_handle);
#else
		close(m_handle);
#endif
	}
	m_handle = h;
	if(h != INVALID_HANDLE)
	{
#ifndef WINDOWS
		// Ensure that the handle is non-blocking
		int flags = fcntl(h, F_GETFL, 0);
		if((flags & O_NONBLOCK) == 0)
			fcntl(h, F_SETFL, flags | O_NONBLOCK);
#endif
	}
}

ssize_t GPipe::read(char* buf, size_t bufSize)
{
#ifdef WINDOWS
	DWORD dwRead;
	ReadFile(m_handle, buf, (DWORD)bufSize, &dwRead, NULL);
	return dwRead;
#else
	return ::read(m_handle, buf, bufSize);
#endif
}

std::string GPipe::read(size_t maxSize)
{
	const unsigned bufsize = 256;
	char buf[bufsize];
	std::ostringstream out;
	size_t size = 0;
	while(true)
	{
		size_t bytes = read(buf, bufsize);
		out.write(buf, bytes);
		size += bytes;
		if(bytes < bufsize || size >= maxSize)
			break;
	}
	return out.str();
}

void GPipe::write(const char* buf, size_t bufSize)
{
#ifdef WINDOWS
	if(!WriteFile(m_handle, buf, (DWORD)bufSize, NULL, NULL))
		throw Ex("Error writing to pipe");
#else
	if(::write(m_handle, buf, bufSize) == -1)
		throw Ex("Error writing to pipe");
#endif
}

void GPipe::toFile(const char* szFilename)
{
	std::ofstream s;
	s.exceptions(std::ios::badbit | std::ios::failbit);
	try
	{
		s.open(szFilename, std::ios::binary);
	}
	catch(const std::exception&)
	{
		throw Ex("Error while trying to create the file, ", szFilename, ". ", strerror(errno));
	}
	char buf[256];
	while(true)
	{
		size_t bytes = read(buf, 256);
		s.write(buf, bytes);
		if(bytes < 256)
			break;
	}
}






/*static*/ void GApp::launchDaemon(DaemonMainFunc pDaemonMain, void* pArg, const char* stdoutFilename, const char* stderrFilename)
{
#ifdef WINDOWS
	// This method isn't implemented on Windows yet, so we'll just
	// call straight through for now.
	pDaemonMain(pArg);
	return;
#else

	// Fork the first time
	int firstPid = fork();
	if(firstPid < 0)
		throw Ex("Error forking off the child process in GApp::LaunchDaemon");
	if(firstPid) // If I am the parent process...
	{
		int status;
		waitpid(firstPid, &status, 0); // Wait for the child process to return the pid of the grand-child daemon
		if(WIFEXITED(status)) // If the child process terminated properly
		{
			int childExitStatus = WEXITSTATUS(status);
			switch(childExitStatus)
			{
				case 0: return; // Everything was successfully
				case 1: throw Ex("Failed to redirect stdout to append to ", stdoutFilename);
				case 2: throw Ex("Failed to redirect stderr to append to ", stderrFilename);
				case 3: throw Ex("Failed to fork off the grand-child daemon process");
				default: throw Ex("Internal error. Unknown exit code");
			}
		}
		else
			throw Ex("The child process failed to fork off the grand-child daemon");
	}

	// If it gets to here, I am the child process

	// Redirect standard output stream to a file
	if(stdoutFilename)
	{
		if(!freopen(stdoutFilename, "a", stdout))
			exit(1);
	}

	// Redirect standard error stream to a file
	if(stderrFilename)
	{
		if(!freopen(stderrFilename, "a", stderr))
			exit(2);
	}

	// Fork the second time
	int secondPid = fork();
	if(secondPid < 0)
		exit(3); // error forking off the grand-child process
	if(secondPid) // If I am the child process...
		exit(0); // Everything worked!

	// If it gets to here, I am the grand-child daemon

	// Drop my process group leader and become my own process group leader
	// (so the process isn't terminated when the group leader is killed)
	setsid();

	// Set the file creation mask. (I don't know why we do this.)
	umask(0);

	// Get off any mounted drives so that they can be unmounted without killing the daemon
	// No, some daemons may use the current directory, so they should do this themselves if not.
	//	if(chdir("/") != 0)
	//	{
	//	}

	// Launch the daemon
	pDaemonMain(pArg);

	exit(0);
#endif
}

/*static*/ int GApp::appPath(char* pBuf, size_t len, bool clipFilename)
{
#ifdef WINDOWS
	int bytes = GetModuleFileName(NULL, pBuf, (unsigned int)len);
	if(bytes == 0)
		return -1;
	else
	{
		if(clipFilename)
		{
			while(bytes > 0 && pBuf[bytes - 1] != '\\' && pBuf[bytes - 1] != '/')
				pBuf[--bytes] = '\0';
		}
		return bytes;
	}
#else
#if defined(__linux__) || defined(__FreeBSD__)
	std::ostringstream os;
	os << "/proc/" << getpid() << "/exe";
	string tmp = os.str();
	int bytes = std::min((int)readlink(tmp.c_str(), pBuf, len), (int)len - 1);
#else
	// __APPLE__
	uint32_t l = len;
	_NSGetExecutablePath(pBuf, &l); // todo: this returns the path to the symlink, not the actual app.
	int bytes = strlen(pBuf);
#endif
	if(bytes >= 0)
	{
		pBuf[bytes] = '\0';
		if(clipFilename)
		{
			while(bytes > 0 && pBuf[bytes - 1] != '/')
				pBuf[--bytes] = '\0';
		}
	}
	return bytes;
#endif
}

int GApp_measureParamLen(const char* sz)
{
	int len = 0;
	while(true)
	{
		if(*sz == '"')
		{
			len++;
			sz++;
			while(*sz != '"' && *sz != '\0')
			{
				len++;
				sz++;
			}
			if(*sz == '"')
			{
				len++;
				sz++;
			}
			continue;
		}
		else if(*sz == '\'')
		{
			len++;
			sz++;
			while(*sz != '\'' && *sz != '\0')
			{
				len++;
				sz++;
			}
			if(*sz == '\'')
			{
				len++;
				sz++;
			}
			continue;
		}
		else if(*sz <= ' ')
			break;
		len++;
		sz++;
	}
	return len;
}

int GApp_measureWhitespaceLen(const char* sz)
{
	int len = 0;
	while(*sz <= ' ' && *sz != '\0')
	{
		len++;
		sz++;
	}
	return len;
}

int GApp_CountArgs(const char* sz)
{
	int count = 0;
	while(true)
	{
		sz += GApp_measureWhitespaceLen(sz);
		if(*sz == '\0')
			break;
		count++;
		sz += GApp_measureParamLen(sz);
	}
	return count;
}

void GApp_ParseArgs(char* sz, char* argv[], int cap)
{
	int count = 0;
	while(true)
	{
		if(*sz != '\0' && *sz <= ' ')
		{
			*sz = '\0';
			sz++;
		}
		sz += GApp_measureWhitespaceLen(sz);
		if(*sz == '\0')
			break;
		argv[count++] = sz;
		if(count >= cap)
			break;
		sz += GApp_measureParamLen(sz);
	}
	argv[count] = nullptr;
}

int GApp::systemCall(const char* szCommand, bool wait, bool show)
{
#ifdef WINDOWS
	// Parse the args
	GTEMPBUF(char, szCopy, (int)strlen(szCommand) + 1);
	strcpy(szCopy, szCommand);
	int argc = GApp_CountArgs(szCopy);
	if(argc == 0)
		return 0;
	char* argv[3];
	GApp_ParseArgs(szCopy, argv, 2);

	// Call it
	SHELLEXECUTEINFO sei;
	memset(&sei, '\0', sizeof(SHELLEXECUTEINFO));
	sei.cbSize = sizeof(SHELLEXECUTEINFO);
	sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_DDEWAIT;
	sei.hwnd = NULL;
	sei.lpVerb = NULL;
	sei.lpFile = argv[0];
	sei.lpParameters = argv[1];
	sei.lpDirectory = NULL;
	sei.nShow = show ? SW_SHOW : SW_HIDE;
	if(!ShellExecuteEx(&sei))
		throw Ex("An error occurred while executing the command \"", argv[0], " ", argv[1], "\"");
	DWORD ret = 0;
	if(wait)
	{
		WaitForSingleObject(sei.hProcess, INFINITE);
		if(!GetExitCodeProcess(sei.hProcess, &ret))
		{
			CloseHandle(sei.hProcess);
			throw Ex("Failed to obtain exit code");
		}
		CloseHandle(sei.hProcess);
	}
	return ret;
#else
	// SAKBox: run the program directly, as on Windows and in systemExecute.
	// This went through the shell (system), so text in the command could
	// run more commands.
	int ret = systemExecute(szCommand, wait);
	return wait ? ret : 0; // (systemExecute gives the process id when not waiting)
#endif
}

#ifndef WINDOWS
// Removes the quotes that group an argument, as a shell does: "a b" and
// 'a b' become a b. (GApp_measureParamLen decides where arguments end.)
void GApp_unquote(char* sz)
{
	char* pOut = sz;
	char quote = '\0';
	for(; *sz != '\0'; sz++)
	{
		if(quote == '\0' && (*sz == '"' || *sz == '\''))
			quote = *sz;
		else if(*sz == quote)
			quote = '\0';
		else
			*pOut++ = *sz;
	}
	*pOut = '\0';
}

// Runs argv[0] with the arguments argv (ending with a null), without a
// shell. See GApp::systemExecute.
int GApp_execute(char* argv[], bool wait, GPipe* pStdOut, GPipe* pStdErr, GPipe* pStdIn)
{
	// Create a pipe for stdout
	int stdOutPipe[2]; // Element 0 is the read end. Element 1 is the write end.
	if(pStdOut)
	{
		if(pipe(stdOutPipe) == -1)
			throw Ex("Error creating pipe for stdout. errno=", to_str(errno));
	}

	// Create a pipe for stderr
	int stdErrPipe[2]; // Element 0 is the read end. Element 1 is the write end.
	if(pStdErr)
	{
		if(pipe(stdErrPipe) == -1)
			throw Ex("Error creating pipe for stderr. errno=", to_str(errno));
	}

	// Call it
	pid_t pid = fork();
	if(pid == 0) // if I am the forked process...
	{
		// Use the provided stdin
		if(pStdIn)
			dup2(pStdIn->get(), STDIN_FILENO);

		// Redirect stdout into the pipe
		if(pStdOut)
		{
			close(stdOutPipe[0]); // close the read end of the stdout pipe since we won't need it
			dup2(stdOutPipe[1], STDOUT_FILENO); // redirect stdout into the write end of the pipe
		}

		// Redirect stderr into the pipe
		if(pStdErr)
		{
			close(stdErrPipe[0]); // close the read end of the stderr pipe since we won't need it
			dup2(stdErrPipe[1], STDERR_FILENO); // redirect stderr into the write end of the pipe
		}

		// Call the child process
		execvp(argv[0], argv);

		// execvp only returns if there is an error. Otherwise, it replaces the
		// current process. SAKBox: exit with 127, as a shell does. (This threw,
		// and the child then carried on as a second copy of the caller.)
		_exit(127);
	}
	else if(pid > 0) // else if I am the calling process...
	{
		// Return the read end of the stdout pipe
		if(pStdOut)
		{
			close(stdOutPipe[1]); // close the write end of the stdout pipe since we won't need it
			pStdOut->set(stdOutPipe[0]);
		}

		// Return the read end of the stderr pipe
		if(pStdErr)
		{
			close(stdErrPipe[1]); // close the write end of the stderr pipe since we won't need it
			pStdErr->set(stdErrPipe[0]);
		}

		// Wait for the process to terminate
		if(wait)
		{
			int status;
			waitpid(pid, &status, 0);
			if(WIFEXITED(status))
			{
				int ret = WEXITSTATUS(status);
				return ret;
			}
			else if(WIFSIGNALED(status))
				throw Ex("The process was interruped with signal ", to_str(WTERMSIG(status)));
			else
				throw Ex("The process stopped without exiting, and it cannot be restarted.");
		}
	}
	else // else fork failed...
		throw Ex("There was an error forking the process");
	return pid;
}
#endif // !WINDOWS

int GApp::systemExecute(const char* szCommand, bool wait, GPipe* pStdOut, GPipe* pStdErr, GPipe* pStdIn)
{
#ifdef WINDOWS
	// Initialize a STARTUPINFO structure
	STARTUPINFO siStartInfo;
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES; // Let us to specify the std handles

	// Set the bInheritHandle flag so pipe handles can be inherited.
	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE; // pipe handles are inherited
	saAttr.lpSecurityDescriptor = NULL;

	// Create a pipe for stdout
	HANDLE hChildStdoutRd, hChildStdoutWr;
	if(pStdOut)
	{
		if(!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0))
			throw Ex("Failed to create a pipe for the child processes stdout");
		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0); // Ensure that the read handle to the child process's pipe for stdout is not inherited
		pStdOut->set(hChildStdoutRd);
		siStartInfo.hStdOutput = hChildStdoutWr;
	}
	else
		siStartInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);

	// Create a pipe for stderr
	HANDLE hChildStderrRd, hChildStderrWr;
	if(pStdErr)
	{
		if(!CreatePipe(&hChildStderrRd, &hChildStderrWr, &saAttr, 0))
			throw Ex("Failed to create a pipe for the child processes stderr");
		SetHandleInformation(hChildStderrRd, HANDLE_FLAG_INHERIT, 0); // Ensure that the read handle to the child process's pipe for stderr is not inherited
		pStdErr->set(hChildStderrRd);
		siStartInfo.hStdError = hChildStderrWr;
	}
	else
		siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);

	// Set up stdin
	if(pStdIn)
		siStartInfo.hStdInput = pStdIn->get();
	else
		siStartInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

	// Create the child process.
	PROCESS_INFORMATION piProcInfo;
	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
	if(!CreateProcess(NULL,
	                  (LPSTR)szCommand, // the command line
	                  NULL, // process security attributes
	                  NULL, // primary thread security attributes
	                  TRUE, // handles are inherited
	                  0, // creation flags (create no window)
	                  NULL, // NULL means use parent's environment
	                  NULL, // NULL means use parent's current working directory
	                  &siStartInfo, // STARTUPINFO pointer
	                  &piProcInfo)) // receives PROCESS_INFORMATION
	{
		DWORD dwErr = GetLastError();
		char buf[256];
		if(FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwErr, 0, buf, 256, NULL) == 0)
		{
			DWORD dwErrErr = GetLastError();
			GAssert(false);
			buf[0] = '\0';
		}
		char buf2[256];
		getcwd(buf2, 256);
		throw Ex("Failed to create process: ", buf, "(cwd=", buf2, ")");
	}

	// Close the child processes' stdin pipe, since we don't really need it
	if(pStdOut)
		CloseHandle(hChildStdoutWr);
	if(pStdErr)
		CloseHandle(hChildStderrWr);

	// Wait for the child process
	DWORD ret = 0; //piProcInfo.hProcess; // Todo: BUG: Fix this line
	if(wait)
	{
		WaitForSingleObject(piProcInfo.hProcess, 10000 /*INFINITE*/);
		if(!GetExitCodeProcess(piProcInfo.hProcess, &ret))
		{
			CloseHandle(piProcInfo.hProcess);
			CloseHandle(piProcInfo.hThread);
			throw Ex("Failed to obtain exit code");
		}
	}
	CloseHandle(piProcInfo.hProcess);
	CloseHandle(piProcInfo.hThread);

	return ret;
#else
	// Parse the args
	GTEMPBUF(char, szCopy, strlen(szCommand) + 1);
	strcpy(szCopy, szCommand);
	int argc = GApp_CountArgs(szCopy);
	if(argc == 0)
		return 0;
	GTEMPBUF(char*, argv, argc + 1);
	GApp_ParseArgs(szCopy, argv, 0x7fffffff);

	for(int i = 0; i < argc; i++)
		GApp_unquote(argv[i]);
	return GApp_execute(argv, wait, pStdOut, pStdErr, pStdIn);
#endif
}

// static
void GApp::enableFloatingPointExceptions()
{
#ifdef WINDOWS
	unsigned int cw = _control87(0, 0) & MCW_EM; // should we use _controlfp instead?
	cw &= ~(_EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW);
	_control87(cw, MCW_EM);
#else
#ifdef __APPLE__
	//_MM_SET_EXCEPTION_MASK(_MM_GET_EXCEPTION_MASK() & ~_MM_MASK_INVALID);
#else
	feenableexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW);
#endif
#endif
}

// static
bool GApp::openFile(const char* szPathOrUrl)
{
#ifdef WINDOWS
	int nRet = (int)ShellExecute(NULL, NULL, szPathOrUrl, NULL, NULL, SW_SHOW);
	return nRet > 32;
#else
	// SAKBox: the path or URL is passed as one argument, not through the shell.
	// ("open " + url + " &" was run by the shell, so a ; or $(...) in a URL or
	// file name ran commands.)
#ifdef __APPLE__
	const char* openers[] = {"open"};
#else
	const char* openers[] = {"xdg-open", "gnome-open"}; // (xdg-open works on KDE too)
#endif
	for(const char* szOpener : openers)
	{
		char* argv[] = {(char*)szOpener, (char*)szPathOrUrl, nullptr};
		try
		{
			if(GApp_execute(argv, true, nullptr, nullptr, nullptr) == 0)
				return true;
		}
		catch(const std::exception&)
		{
			continue; // it was killed by a signal: try the next one
		}
	}
	return false;
#endif
}

// static
bool GApp::openUrlInBrowser(const char* szUrl)
{
	return openFile(szUrl);
}






GSignalHandler* g_pSignalHandler = nullptr;
#ifndef WINDOWS
void GApp_onSigInt(int n)
{
	g_pSignalHandler->onSignal(SIGINT);
}

void GApp_onSigTerm(int n)
{
	g_pSignalHandler->onSignal(SIGTERM);
}

void GApp_onSigPipe(int n)
{
	g_pSignalHandler->onSignal(SIGPIPE);
}

void GApp_onSigSegV(int n)
{
	g_pSignalHandler->onSignal(SIGSEGV);
}
#endif

GSignalHandler::GSignalHandler()
{
	if(g_pSignalHandler)
		throw Ex("GSignalHandler is not reentrant, so it cannot be nested");
	g_pSignalHandler = this;
	m_gotSignal = 0;
#ifndef WINDOWS
	m_prevSigInt = signal(SIGINT, GApp_onSigInt);
	if(m_prevSigInt == SIG_ERR) m_prevSigInt = SIG_DFL;
	m_prevSigTerm = signal(SIGTERM, GApp_onSigTerm);
	if(m_prevSigTerm == SIG_ERR) m_prevSigTerm = SIG_DFL; // SAKBox: these three reset m_prevSigInt
	m_prevSigPipe = signal(SIGPIPE, GApp_onSigPipe);
	if(m_prevSigPipe == SIG_ERR) m_prevSigPipe = SIG_DFL;
	m_prevSigSegV = signal(SIGSEGV, GApp_onSigSegV);
	if(m_prevSigSegV == SIG_ERR) m_prevSigSegV = SIG_DFL;
#endif
}

GSignalHandler::~GSignalHandler()
{
#ifndef WINDOWS
	signal(SIGINT, m_prevSigInt);
	signal(SIGTERM, m_prevSigTerm);
	signal(SIGPIPE, m_prevSigPipe);
	signal(SIGSEGV, m_prevSigSegV);
#endif
	g_pSignalHandler = nullptr;
}

void GSignalHandler::onSignal(int sig)
{
	g_pSignalHandler->m_gotSignal = sig;
}

int GSignalHandler::check()
{
	int oldSignal = m_gotSignal;
	m_gotSignal = 0;
	return oldSignal;
}

/*static*/
const char* GSignalHandler::to_str(int sig)
{
	switch(sig)
	{
		case SIGABRT: return "SIGABRT";
		case SIGFPE: return "SIGFPE";
		case SIGILL: return "SIGILL";
		case SIGINT: return "SIGINT";
		case SIGSEGV: return "SIGSEGV";
		case SIGTERM: return "SIGTERM";
		case SIGPIPE: return "SIGPIPE";
		default: return "UNRECOGNIZED";
	}
}






#ifdef WINDOWS
GPassiveConsole::GPassiveConsole(bool echo)
{
	m_hStdin = GetStdHandle(STD_INPUT_HANDLE);
	GetConsoleMode(m_hStdin, &m_oldMode);
	DWORD newMode = m_oldMode & (~ENABLE_LINE_INPUT);
	if(!echo)
		newMode &= (~ENABLE_ECHO_INPUT);
	SetConsoleMode(m_hStdin, newMode);
}

GPassiveConsole::~GPassiveConsole()
{
	SetConsoleMode(m_hStdin, m_oldMode);
}

char GPassiveConsole::getChar()
{
	DWORD n;
	while(true)
	{
		if(!PeekConsoleInput(m_hStdin, &m_inputRecord, 1, &n))
			throw Ex("PeekConsoleInput failed");
		if(n == 0)
			return '\0';
		if(m_inputRecord.EventType == KEY_EVENT && m_inputRecord.Event.KeyEvent.bKeyDown && m_inputRecord.Event.KeyEvent.uChar.AsciiChar != 0)
			break;
		if(!ReadConsoleInput(m_hStdin, &m_inputRecord, 1, &n))
			throw Ex("ReadConsoleInput failed");
	}
	char c;
	if(ReadConsole(m_hStdin /*hConsoleInput*/, &c, 1, &n, NULL))
	{
		GAssert(n > 0);
		return c;
	}
	else
	{
		throw Ex("ReadConsole failed");
		return '\0';
	}
}
#else
GPassiveConsole::GPassiveConsole(bool echo)
{
	if(tcgetattr(0, &m_old) < 0)
		throw Ex("Error getting terminal settings");
	struct termios tmp;
	memcpy(&tmp, &m_old, sizeof(struct termios));
	tmp.c_lflag &= ~ICANON;
	if(!echo)
		tmp.c_lflag &= ~ECHO;
	tmp.c_cc[VMIN] = 1;
	tmp.c_cc[VTIME] = 0;
	if(tcsetattr(0, TCSANOW, &tmp) < 0)
		throw Ex("Error setting terminal settings");
	m_stdin = fileno(stdin);
	m_oldStreamFlags = fcntl(m_stdin, F_GETFL, 0);
	if(fcntl(m_stdin, F_SETFL, m_oldStreamFlags | O_NONBLOCK) == -1)
		throw Ex("Error setting stdin to non-blocking");
}

GPassiveConsole::~GPassiveConsole()
{
	if(tcsetattr(0, TCSANOW, &m_old) < 0)
	{
		std::cerr << "Error restoring terminal settings. Terminating.";
		exit(1);
	}
	if(fcntl(m_stdin, F_SETFL, m_oldStreamFlags) == -1)
	{
		std::cerr << "Error restoring stdin flags. Termination.";
		exit(1);
	}
}

char GPassiveConsole::getChar()
{
	char c;
	if(read(m_stdin, &c, 1) > 0)
		return c;
	else
		return '\0';
}
#endif







GArgReader::GArgReader(int argc, char* argv[])
: m_argc(argc), m_argv(argv), m_argPos(0)
{
}

GArgReader::GArgReader(GArgReader& other)
: m_argc(other.m_argc), m_argv(other.m_argv), m_argPos(other.m_argPos)
{
}

int GArgReader::get_pos()
{
	return m_argPos;
}

void GArgReader::set_pos(int n)
{
	m_argPos = n;
}

const char* GArgReader::peek()
{
	return m_argv[m_argPos];
}

const char* GArgReader::pop_string()
{
	if(m_argPos >= m_argc)
		throw Ex("Unexpected end of arguments");
	return m_argv[m_argPos++];
}

unsigned int GArgReader::pop_uint()
{
	const char* str = pop_string();
	unsigned int n;
	if(!parseNumber(str, &n)) // SAKBox: checked for digits only, so "" gave 0 and big numbers overflowed
		throw Ex("Expected an unsigned integer value for parameter ", to_str(m_argPos), ". (Got \"", str, "\".)");
	return n;
}

double GArgReader::pop_double()
{
	// SAKBox: reject anything that isn't a number (atof turned "abc" into 0),
	// as pop_uint does
	const char* str = pop_string();
	char* end = nullptr;
	double value = strtod(str, &end);
	if(end == str || *end != '\0')
		throw Ex("Expected a number for parameter ", to_str(m_argPos), ". (Got \"", str, "\".)");
	return value;
}

bool GArgReader::if_pop(const char* flagName)
{
	if(m_argPos >= m_argc)
		return false;
	if(_stricmp(peek(), flagName) == 0)
	{
		m_argPos++;
		return true;
	}
	else
		return false;
}

int GArgReader::size()
{
	return m_argc - m_argPos;
}

bool GArgReader::next_is_flag()
{
	if(size() == 0)
		return false;
	return (peek()[0] == '-');
}

bool GArgReader::next_is_uint()
{
	if(size() == 0)
	{
		return false;
	}
	const char* s = peek();
	if(s == nullptr || *s == '\0')
	{
		return false;
	}
	char* p;
	if(strtoul(s, &p, 10) == 0)
	{
		// no-op to circumvent a g++ warning
	}
	return *p == '\0';
}

void GArgReader::clear_args()
{
	m_argPos = m_argc;
}
