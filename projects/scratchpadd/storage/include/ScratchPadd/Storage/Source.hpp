// Source.hpp - something with writes of its own to do (an AppendFile: a log,
// a recording), which StoragePadd does on its thread, when nothing else has
// work. A source keeps what's waiting and rings StoragePadd's doorbell when
// it's time to write; StoragePadd then calls its write(). Without
// StoragePadd attached (before the padds start, after they stop) a source
// writes for itself.
//
// Sources made outside the padds (the log's, in main) are registered with
// sources(), where StoragePadd finds them, including any registered while
// it runs:
//
//   ScratchPadd::Storage::sources().add(&file);   // and remove(&file) before it goes
#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ScratchPadd::Storage {

class Source {
public:
  virtual ~Source() = default;
  // For panels and logs: "log", "replay"
  virtual std::string name() const = 0;
  // Where it writes, for its Info line on StoragePadd's tab
  virtual std::string where() const = 0;
  // What its writing has cost so far, in a line ("16 syncs, 7.1 ms each...")
  virtual std::string figuresLine() const = 0;

  // The doorbell: called (from any thread) when it has something to write.
  // detach() stops it: once it returns, the doorbell isn't called again.
  virtual void attach(std::function<void()> ring) = 0;
  virtual void detach() = 0;
  // Writes what's waiting (on StoragePadd's thread)
  virtual void write() = 0;

  // How many pieces it writes together, for a "lines between writes"
  // control; nothing if it has no such setting
  virtual std::optional<unsigned> writeEvery() const { return std::nullopt; }
  virtual void setWriteEvery(unsigned) {}
};

// The sources registered so far. Thread-safe. StoragePadd uses a source
// only through forEach() and with(), under the registry's lock, so once
// remove() returns it isn't using it: then it can go.
class Sources {
public:
  void add(Source *source);
  void remove(Source *source);
  std::vector<Source *> all() const;
  // `use` for each source, under the lock
  void forEach(const std::function<void(Source &)> &use) const;
  // `use` for `source` if it's still registered (under the lock); false if not
  bool with(const Source *source, const std::function<void(Source &)> &use) const;
  // `onChange` is called (on the registering thread) after each add or
  // remove; one listener (StoragePadd), cleared with nullptr
  void listen(std::function<void()> onChange);

private:
  mutable std::mutex mutex_;
  std::vector<Source *> sources_;
  std::function<void()> onChange_;
};

// The app's
Sources &sources();

} // namespace ScratchPadd::Storage
