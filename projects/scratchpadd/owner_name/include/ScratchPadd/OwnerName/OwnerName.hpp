// OwnerName.hpp - the name the console's owner gave it, for an app to
// greet them or show as theirs in a game (the user's: "the console's own,
// nothing to type"): on a DS the nickname in its settings, on a 3DS its
// user's name, on a Mac or Linux the account's full name (its first word,
// else the login name), on Windows the user name; nothing when there's
// none (an app falls back on its own word).
//
//   const std::string name = ScratchPadd::OwnerName::get(); // "James", or ""
//
// UTF-8. Letters outside plain ASCII come out as '?' (the screens' font
// has only those).
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace ScratchPadd::OwnerName {

std::string get();

// UTF-16 code units as UTF-8, ASCII kept, others '?', spaces at either end
// trimmed (on its own, to test)
std::string fromUtf16(std::span<const std::uint16_t> units);
// A desktop account's: the first word of its full name ("James Grantham",
// and the ",,," Linux adds after it), else the login name, its first
// letter made capital (on its own, to test)
std::string fromAccount(std::string_view fullName, std::string_view login);

} // namespace ScratchPadd::OwnerName
