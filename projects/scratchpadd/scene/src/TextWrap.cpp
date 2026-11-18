#include <ScratchPadd/Scene/TextWrap.hpp>

namespace ScratchPadd::Scene {

namespace {

// Adds one paragraph's lines (no '\n' in it) to `lines`
void wrapParagraph(std::string_view paragraph, float width, const MeasureText &measure,
                   std::vector<std::string> &lines) {
  std::string line;
  std::size_t at = 0;
  while (true) {
    // The next word
    while (at < paragraph.size() && paragraph[at] == ' ')
      ++at;
    if (at == paragraph.size())
      break;
    std::size_t end = paragraph.find(' ', at);
    if (end == std::string_view::npos)
      end = paragraph.size();
    std::string_view word = paragraph.substr(at, end - at);
    at = end;

    // On this line, if it fits
    const std::string joined = line.empty() ? std::string(word) : line + " " + std::string(word);
    if (measure(joined) <= width) {
      line = joined;
      continue;
    }
    if (!line.empty()) {
      lines.push_back(line);
      line.clear();
    }
    // Wider than a whole line: as much as fits on each, then the rest
    while (word.size() > 1 && measure(word) > width) {
      std::size_t fits = 1;
      while (fits + 1 < word.size() && measure(word.substr(0, fits + 1)) <= width)
        ++fits;
      lines.emplace_back(word.substr(0, fits));
      word.remove_prefix(fits);
    }
    line = std::string(word);
  }
  lines.push_back(line);
}

} // namespace

std::vector<std::string> wrapText(std::string_view text, float width, const MeasureText &measure) {
  std::vector<std::string> lines;
  while (true) {
    const std::size_t newline = text.find('\n');
    wrapParagraph(text.substr(0, newline), width, measure, lines);
    if (newline == std::string_view::npos)
      break;
    text.remove_prefix(newline + 1);
  }
  return lines;
}

} // namespace ScratchPadd::Scene
