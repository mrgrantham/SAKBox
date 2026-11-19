// Vector.hpp - a list with its room inside it: up to N items, no
// allocation, copied as one block. It's C++26's
// std::inplace_vector in miniature, the parts the games use, named the
// same way so it can be swapped for the standard one once every compiler
// here has it (the Mac's doesn't yet).
//
// Why: a Cosmic Hex bot copies the game state at the start of every
// simulated game, and with std::vector each copy was a dozen or more
// allocations; on a DS that was over half a millisecond a copy (melonDS).
// With these, a State is one block: copying it is a memcpy, with nothing to
// allocate or free (Gregory, Game Engine Architecture 3.5.4: small,
// contiguous data, read in order, suits the cache).
//
// The room is fixed: pushing past N is a bug in the caller, so it stops the
// program at once (never a silent drop). Pick N from the rules' real limits.
//
//   Inplace::Vector<int, 32> deck{0, 19, 23};
//   deck.push_back(4);
//   for (int card : deck) ...
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>

namespace Inplace {

// (pushing past the room: a bug, stopped here)
[[noreturn]] inline void outOfRoom(std::size_t capacity) {
  std::fprintf(stderr, "Inplace::Vector: more than its %zu items\n", capacity);
  std::abort();
}

template <typename T, std::size_t N> class Vector {
  static_assert(N > 0 && N <= 65535, "room for 1 to 65,535 items");
  static_assert(std::is_default_constructible_v<T>, "its room is made of default items");

public:
  using value_type = T;
  using size_type = std::size_t;
  using iterator = T *;
  using const_iterator = const T *;
  using reference = T &;
  using const_reference = const T &;

  constexpr Vector() = default;
  constexpr Vector(std::initializer_list<T> items) { assign(items.begin(), items.end()); }
  constexpr Vector(size_type count, const T &value) { assign(count, value); }
  // From any other list (a std::vector, an array, another Vector)
  template <typename InputIt,
            typename = std::enable_if_t<!std::is_integral_v<InputIt>>> // (not (count, value))
  constexpr Vector(InputIt first, InputIt last) {
    assign(first, last);
  }

  // The items
  constexpr size_type size() const { return size_; }
  static constexpr size_type capacity() { return N; }
  static constexpr size_type max_size() { return N; }
  constexpr bool empty() const { return size_ == 0; }
  constexpr T *data() { return items_.data(); }
  constexpr const T *data() const { return items_.data(); }
  constexpr iterator begin() { return items_.data(); }
  constexpr iterator end() { return items_.data() + size_; }
  constexpr const_iterator begin() const { return items_.data(); }
  constexpr const_iterator end() const { return items_.data() + size_; }
  constexpr const_iterator cbegin() const { return begin(); }
  constexpr const_iterator cend() const { return end(); }
  constexpr T &operator[](size_type i) { return items_[i]; }
  constexpr const T &operator[](size_type i) const { return items_[i]; }
  constexpr T &front() { return items_[0]; }
  constexpr const T &front() const { return items_[0]; }
  constexpr T &back() { return items_[size_ - 1]; }
  constexpr const T &back() const { return items_[size_ - 1]; }

  // Adding
  constexpr void push_back(const T &value) {
    roomFor(1);
    items_[size_++] = value;
  }
  template <typename... Args> constexpr T &emplace_back(Args &&...args) {
    roomFor(1);
    items_[size_] = T{std::forward<Args>(args)...};
    return items_[size_++];
  }
  // `value` put in before `at`, the rest moved along
  constexpr iterator insert(const_iterator at, const T &value) {
    return insert(at, size_type{1}, value);
  }
  constexpr iterator insert(const_iterator at, size_type count, const T &value) {
    const size_type from = indexOf(at);
    roomFor(count);
    std::move_backward(begin() + from, end(), end() + count);
    std::fill(begin() + from, begin() + from + count, value);
    size_ = static_cast<std::uint16_t>(size_ + count);
    return begin() + from;
  }
  template <typename InputIt, typename = std::enable_if_t<!std::is_integral_v<InputIt>>>
  constexpr iterator insert(const_iterator at, InputIt first, InputIt last) {
    const size_type from = indexOf(at);
    const auto count = static_cast<size_type>(std::distance(first, last));
    roomFor(count);
    std::move_backward(begin() + from, end(), end() + count);
    std::copy(first, last, begin() + from);
    size_ = static_cast<std::uint16_t>(size_ + count);
    return begin() + from;
  }

  // Replacing
  template <typename InputIt, typename = std::enable_if_t<!std::is_integral_v<InputIt>>>
  constexpr void assign(InputIt first, InputIt last) {
    clear();
    insert(end(), first, last);
  }
  constexpr void assign(size_type count, const T &value) {
    clear();
    insert(end(), count, value);
  }
  constexpr void resize(size_type count, const T &value = T{}) {
    if (count > size_)
      insert(end(), count - size_, value);
    else
      erase(begin() + count, end());
  }

  // Taking away
  constexpr void clear() { size_ = 0; }
  constexpr void pop_back() { --size_; }
  constexpr iterator erase(const_iterator at) { return erase(at, at + 1); }
  constexpr iterator erase(const_iterator first, const_iterator last) {
    const size_type from = indexOf(first), to = indexOf(last);
    std::move(begin() + to, end(), begin() + from);
    size_ = static_cast<std::uint16_t>(size_ - (to - from));
    return begin() + from;
  }

  // Equal: the same items in the same order (the room past them aside)
  friend constexpr bool operator==(const Vector &a, const Vector &b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end());
  }

private:
  constexpr void roomFor(size_type more) const {
    if (size_ + more > N)
      outOfRoom(N);
  }
  constexpr size_type indexOf(const_iterator at) const {
    return static_cast<size_type>(at - items_.data());
  }

  std::array<T, N> items_{};
  std::uint16_t size_ = 0;
};

// std::erase and std::erase_if, as for std::vector
template <typename T, std::size_t N, typename U>
constexpr std::size_t erase(Vector<T, N> &list, const U &value) {
  const auto kept = std::remove(list.begin(), list.end(), value);
  const auto removed = static_cast<std::size_t>(list.end() - kept);
  list.erase(kept, list.end());
  return removed;
}
template <typename T, std::size_t N, typename Predicate>
constexpr std::size_t erase_if(Vector<T, N> &list, Predicate predicate) {
  const auto kept = std::remove_if(list.begin(), list.end(), predicate);
  const auto removed = static_cast<std::size_t>(list.end() - kept);
  list.erase(kept, list.end());
  return removed;
}

} // namespace Inplace
