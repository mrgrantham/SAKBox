// GApp's command-line helpers.
#include <gtest/gtest.h>

#include <WClasses/GApp.h>

#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#ifndef WINDOWS
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace GClasses;

// Command-line numbers: "abc" used to become 0.
TEST(ArgReaderTest, PopDoubleRejectsNonNumbers) {
  char good[] = "2.5", sci[] = "-1e-3", bad[] = "abc", trailing[] = "3x";
  char *argv[] = {good, sci, bad, trailing};
  GArgReader args(4, argv);
  EXPECT_EQ(args.pop_double(), 2.5);
  EXPECT_EQ(args.pop_double(), -1e-3);
  EXPECT_THROW(args.pop_double(), std::exception);
  EXPECT_THROW(args.pop_double(), std::exception);
}

// pop_uint checked only for digits: "" gave 0 and 4294967296 overflowed.
TEST(ArgReaderTest, PopUintRejectsEmptyAndTooBig) {
  char good[] = "42", empty[] = "", big[] = "4294967296", neg[] = "-1";
  char *argv[] = {good, empty, big, neg};
  GArgReader args(4, argv);
  EXPECT_EQ(args.pop_uint(), 42u);
  EXPECT_THROW(args.pop_uint(), std::exception);
  EXPECT_THROW(args.pop_uint(), std::exception);
  EXPECT_THROW(args.pop_uint(), std::exception);
}

// Running other programs. Unix only: on Windows GApp uses ShellExecute.
#ifndef WINDOWS
namespace {
std::string tmpDir() {
  const char *t = getenv("TEST_TMPDIR");
  return t ? t : "/tmp";
}
bool exists(const std::string &path) {
  struct stat st;
  return stat(path.c_str(), &st) == 0;
}
std::string readFile(const std::string &path) {
  std::ifstream f(path);
  return std::string(std::istreambuf_iterator<char>(f), {});
}
} // namespace

// systemCall ran its command through the shell, so text in it could run
// more commands. It now runs one program with its arguments, as
// systemExecute (and systemCall on Windows) do.
TEST(SystemCallTest, RunsOneProgramNotAShellCommand) {
  const std::string marker = tmpDir() + "/system_call_marker";
  unlink(marker.c_str());
  // (true is run with the rest as its arguments, which it ignores)
  const std::string command =
      "/usr/bin/true ; /usr/bin/touch " + marker + " && /usr/bin/touch " + marker;
  EXPECT_EQ(GApp::systemCall(command.c_str(), true, false), 0);
  EXPECT_FALSE(exists(marker));
  EXPECT_EQ(GApp::systemCall("/bin/sh -c 'exit 3'", true, false), 3);
}

// Quotes group an argument, as in a shell, and are removed: they used to be
// passed on as part of it.
TEST(SystemExecuteTest, QuotesGroupAnArgument) {
  GPipe out;
  EXPECT_EQ(GApp::systemExecute("/usr/bin/printf %s|%s \"a b\" 'c d'", true, &out), 0);
  EXPECT_EQ(out.read(), "a b|c d");
}

// A program that can't be run exits with 127, as in a shell. The forked
// child used to throw instead, and carry on as a second copy of the caller.
TEST(SystemExecuteTest, MissingProgramExits127) {
  EXPECT_EQ(GApp::systemExecute("/nonexistent/program", true), 127);
}

// A program killed by a signal is reported with that signal's number (it
// gave the stop signal, 0).
TEST(SystemExecuteTest, KilledBySignalNamesTheSignal) {
  try {
    GApp::systemExecute("/bin/sh -c 'kill -TERM $$'", true);
    FAIL() << "expected an exception";
  } catch (const std::exception &e) {
    EXPECT_NE(std::string(e.what()).find("signal 15"), std::string::npos) << e.what();
  }
}

// Opening a URL or file built a shell command ("open " + url + " &"), so a
// URL or file name with ; or $(...) in it ran commands. It now passes the
// text to the opener as one argument. A fake opener on PATH records what it
// was given.
namespace {
#ifdef __APPLE__
const char *kOpener = "open";
#else
const char *kOpener = "xdg-open";
#endif
struct FakeOpener {
  std::string dir = tmpDir() + "/fake_opener", log = dir + "/opened.txt";
  std::string oldPath = getenv("PATH") ? getenv("PATH") : "";
  FakeOpener() {
    mkdir(dir.c_str(), 0755);
    const std::string script = dir + "/" + kOpener;
    std::ofstream(script) << "#!/bin/sh\nfor a in \"$@\"; do printf '%s\\n' \"$a\"; done >> '"
                          << log << "'\n";
    chmod(script.c_str(), 0755);
    unlink(log.c_str());
    setenv("PATH", dir.c_str(), 1);
  }
  ~FakeOpener() { setenv("PATH", oldPath.c_str(), 1); }
  // What the opener received, one argument per line (waiting a little, as
  // the old code ran it in the background)
  std::string opened() {
    for (int i = 0; i < 200 && !exists(log); i++)
      usleep(10000);
    usleep(50000);
    return readFile(log);
  }
};
} // namespace

TEST(OpenTest, UrlIsOneArgumentNotShellText) {
  FakeOpener opener;
  const std::string marker = tmpDir() + "/open_url_marker";
  unlink(marker.c_str());
  const std::string url = "http://localhost:8080/a?x=1&y=$(/usr/bin/touch " + marker + ")";
  EXPECT_TRUE(GApp::openUrlInBrowser(url.c_str()));
  EXPECT_EQ(opener.opened(), url + "\n");
  EXPECT_FALSE(exists(marker));
}

TEST(OpenTest, FileNameWithSpacesAndSemicolons) {
  FakeOpener opener;
  const std::string name = tmpDir() + "/my file; name.txt";
  EXPECT_TRUE(GApp::openFile(name.c_str()));
  EXPECT_EQ(opener.opened(), name + "\n");
}
#endif // !WINDOWS

// GSignalHandler turns SIGINT and SIGTERM into values check() returns, so a
// server can stop cleanly between requests.
TEST(SignalHandlerTest, ReportsSigintAndSigterm) {
  GSignalHandler handler;
  EXPECT_EQ(handler.check(), 0);
  raise(SIGTERM);
  EXPECT_EQ(handler.check(), SIGTERM);
  EXPECT_EQ(handler.check(), 0); // (reading it clears it)
  raise(SIGINT);
  EXPECT_EQ(handler.check(), SIGINT);
}

// It used to catch SIGSEGV too. A handler that returns from a real segfault
// sends the program back to the faulting instruction, which faults again, so
// a server that crashed spun at full CPU instead of ending (and leaving a
// crash report). Now a segfault ends the program as usual.
TEST(SignalHandlerDeathTest, SegfaultStillEndsTheProgram) {
  EXPECT_EXIT(
      {
        GSignalHandler handler;
        raise(SIGSEGV);
        std::exit(0);
      },
      testing::KilledBySignal(SIGSEGV), "");
}
