#include <ScratchPadd/Network/Service.hpp>

#include <algorithm>
#include <utility>

namespace ScratchPadd::Network {

void Services::add(Service *service) {
  std::function<void(Service *, Service *)> onChange;
  {
    std::lock_guard lock(mutex_);
    if (std::ranges::find(services_, service) != services_.end())
      return;
    services_.push_back(service);
    onChange = onChange_;
  }
  if (onChange)
    onChange(service, nullptr);
}

void Services::remove(Service *service) {
  std::function<void(Service *, Service *)> onChange;
  {
    std::lock_guard lock(mutex_);
    if (std::erase(services_, service) == 0)
      return;
    onChange = onChange_;
  }
  if (onChange)
    onChange(nullptr, service); // (returns once the loop has let go)
}

std::vector<Service *> Services::all() const {
  std::lock_guard lock(mutex_);
  return services_;
}

void Services::listen(std::function<void(Service *, Service *)> onChange) {
  std::lock_guard lock(mutex_);
  onChange_ = std::move(onChange);
}

Services &services() {
  static Services registered;
  return registered;
}

} // namespace ScratchPadd::Network
