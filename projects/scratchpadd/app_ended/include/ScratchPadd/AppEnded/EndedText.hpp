#pragma once

// What a handheld app shows once it has ended (see AppEnded.hpp): plain
// text, laid out for a text screen, so it's tested on desktops.

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace ScratchPadd::AppEnded {

// What a button does once the app has ended: switch the console off, or go
// back to the menu that started the app (on a DS, a homebrew menu that left
// calico a way back)
enum class Leaving { SwitchOff, BackToMenu };

// The lines for each screen, each at most `columns` wide
struct EndedText {
  std::vector<std::string> top;
  std::vector<std::string> bottom;
};

// `text` in lines at most `columns` wide, broken at spaces (a word wider
// than that is cut); a '\n' in it starts a new line
inline std::vector<std::string> wrapLines(std::string_view text, std::size_t columns) {
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t end = std::min(text.find('\n', start), text.size());
    std::string line;
    std::size_t at = start;
    while (at < end) {
      const std::size_t space = std::min(text.find(' ', at), end);
      const std::string_view word = text.substr(at, space - at);
      if (!line.empty() && line.size() + 1 + word.size() > columns) {
        lines.push_back(line);
        line.clear();
      }
      if (!line.empty())
        line += ' ';
      line += word.substr(0, columns);
      at = space + 1;
    }
    lines.push_back(line);
    start = end + 1;
  }
  return lines;
}

// The app's title, then `message` (by default "has ended."), on the top
// screen; what a button does on the bottom
inline EndedText endedText(std::string_view title, std::string_view message, Leaving leaving,
                           std::size_t columns) {
  EndedText text;
  text.top = wrapLines(title, columns);
  text.top.emplace_back();
  for (auto &line : wrapLines(message.empty() ? "has ended." : message, columns))
    text.top.push_back(std::move(line));
  text.bottom =
      wrapLines(leaving == Leaving::SwitchOff ? "Press any button\nto switch off."
                                              : "Press any button\nto go back to the menu.",
                columns);
  return text;
}

} // namespace ScratchPadd::AppEnded
