#include <ScratchPadd/OwnerName/OwnerName.hpp>

#include <array>

#if defined(__NDS__)
#include <nds.h>
#elif defined(__3DS__)
#include <3ds.h>
#elif defined(_WIN32)
#include <cstdlib>
#else
#include <pwd.h>
#include <unistd.h>
#endif

#include <cctype>

namespace ScratchPadd::OwnerName {

std::string fromUtf16(std::span<const std::uint16_t> units) {
  std::string out;
  for (const std::uint16_t unit : units) {
    if (unit == 0)
      break;
    out += unit >= 0x20 && unit < 0x7F ? static_cast<char>(unit) : '?';
  }
  const auto first = out.find_first_not_of(' ');
  if (first == std::string::npos)
    return {};
  return out.substr(first, out.find_last_not_of(' ') - first + 1);
}

std::string fromAccount(std::string_view fullName, std::string_view login) {
  const auto firstWord = [](std::string_view text) {
    const auto start = text.find_first_not_of(" ,");
    if (start == std::string_view::npos)
      return std::string();
    const auto end = text.find_first_of(" ,", start);
    return std::string(text.substr(start, end == std::string_view::npos ? end : end - start));
  };
  std::string name = firstWord(fullName);
  if (name.empty())
    name = firstWord(login);
  if (!name.empty())
    name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
  return name;
}

std::string get() {
#if defined(__NDS__)
  // (the firmware's user settings, copied to main memory at boot)
  std::array<std::uint16_t, 10> units{};
  const int length = PersonalData->nameLen < 10 ? PersonalData->nameLen : 10;
  for (int i = 0; i < length; ++i)
    units[static_cast<std::size_t>(i)] = static_cast<std::uint16_t>(PersonalData->name[i]);
  return fromUtf16(std::span(units).first(static_cast<std::size_t>(length)));
#elif defined(__3DS__)
  // The config block 0x000A0000: the user's name, 10 characters and a
  // terminator, then two bytes more (0x1C bytes in all)
  std::array<std::uint16_t, 14> units{};
  if (R_FAILED(cfguInit()))
    return {};
  const Result read = CFGU_GetConfigInfoBlk2(sizeof units, 0x000A0000, units.data());
  cfguExit();
  if (R_FAILED(read))
    return {};
  return fromUtf16(std::span(units).first(11));
#elif defined(_WIN32)
  const char *user = std::getenv("USERNAME");
  return fromAccount({}, user ? user : "");
#else
  // (the account's entry: its full name in the "GECOS" field)
  const passwd *account = getpwuid(getuid());
  if (!account)
    return {};
  return fromAccount(account->pw_gecos ? account->pw_gecos : "",
                     account->pw_name ? account->pw_name : "");
#endif
}

} // namespace ScratchPadd::OwnerName
