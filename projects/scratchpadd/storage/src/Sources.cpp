#include <ScratchPadd/Storage/Source.hpp>

#include <algorithm>
#include <utility>

namespace ScratchPadd::Storage {

void Sources::add(Source *source) {
  std::function<void()> onChange;
  {
    std::lock_guard lock(mutex_);
    if (std::ranges::find(sources_, source) == sources_.end())
      sources_.push_back(source);
    onChange = onChange_;
  }
  if (onChange)
    onChange();
}

void Sources::remove(Source *source) {
  std::function<void()> onChange;
  {
    std::lock_guard lock(mutex_);
    std::erase(sources_, source);
    onChange = onChange_;
  }
  if (onChange)
    onChange();
}

std::vector<Source *> Sources::all() const {
  std::lock_guard lock(mutex_);
  return sources_;
}

void Sources::forEach(const std::function<void(Source &)> &use) const {
  std::lock_guard lock(mutex_);
  for (Source *source : sources_)
    use(*source);
}

bool Sources::with(const Source *source, const std::function<void(Source &)> &use) const {
  std::lock_guard lock(mutex_);
  const auto found = std::ranges::find(sources_, source);
  if (found == sources_.end())
    return false;
  use(**found);
  return true;
}

void Sources::listen(std::function<void()> onChange) {
  std::lock_guard lock(mutex_);
  onChange_ = std::move(onChange);
}

Sources &sources() {
  static Sources registered;
  return registered;
}

} // namespace ScratchPadd::Storage
