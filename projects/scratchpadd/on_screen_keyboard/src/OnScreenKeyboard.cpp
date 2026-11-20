#include <ScratchPadd/OnScreenKeyboard/OnScreenKeyboard.hpp>

#include <array>
#include <cctype>
#include <string_view>

namespace ScratchPadd {

namespace {
constexpr std::array<std::string_view, 3> kRows{"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
} // namespace

OnScreenKeyboard::OnScreenKeyboard(std::string text, std::size_t most)
    : text_(std::move(text)), most_(most) {
  if (text_.size() > most_)
    text_.resize(most_);
  shift_ = text_.empty(); // (a name's first letter a capital)
  setKeys();
}

void OnScreenKeyboard::layOut(int width, int height) {
  width_ = width;
  height_ = height;
  setKeys();
}

void OnScreenKeyboard::setKeys() {
  const float k = static_cast<float>(width_) / 256;
  std::vector<Buttons::Button> keys;
  // Three rows of letters, each a little further in, as on a keyboard
  for (std::size_t r = 0; r < kRows.size(); ++r)
    for (std::size_t c = 0; c < kRows[r].size(); ++c) {
      const char letter = kRows[r][c];
      keys.push_back(
          {.id = letter,
           .area = {(8 + 12 * static_cast<float>(r) + 24 * static_cast<float>(c)) * k,
                    (44 + 30 * static_cast<float>(r)) * k, 22 * k, 26 * k},
           .label = std::string(1, shift_ ? letter : static_cast<char>(std::tolower(letter)))});
    }
  // Then Shift, Space, Del and Done along the bottom
  const float y = 140 * k, h = 24 * k;
  keys.push_back({.id = KeyShift, .area = {8 * k, y, 46 * k, h}, .label = "Shift", .on = shift_});
  keys.push_back({.id = KeySpace, .area = {58 * k, y, 90 * k, h}, .label = "Space"});
  keys.push_back({.id = KeyDelete, .area = {152 * k, y, 46 * k, h}, .label = "Del"});
  keys.push_back({.id = KeyDone, .area = {202 * k, y, 46 * k, h}, .label = "Done"});
  panel_.set(std::move(keys));
  if (!panel_.focus())
    panel_.setFocus(static_cast<int>('Q'));
}

void OnScreenKeyboard::type(int id) {
  if (outcome_ == Outcome::Done)
    return;
  switch (id) {
  case KeyShift:
    shift_ = !shift_;
    break;
  case KeySpace:
    if (!text_.empty() && text_.size() < most_)
      text_ += ' ';
    break;
  case KeyDelete:
    if (!text_.empty())
      text_.pop_back();
    shift_ = text_.empty();
    break;
  case KeyDone:
    // (trailing spaces dropped)
    while (!text_.empty() && text_.back() == ' ')
      text_.pop_back();
    outcome_ = Outcome::Done;
    break;
  default:
    if (text_.size() < most_) {
      text_ += shift_ ? static_cast<char>(id) : static_cast<char>(std::tolower(id));
      shift_ = false; // (one letter in capitals)
    }
    break;
  }
  setKeys();
}

void OnScreenKeyboard::press(Key key, double now) {
  switch (key) {
  case Key::Up:
    panel_.move(Buttons::Direction::Up);
    break;
  case Key::Down:
    panel_.move(Buttons::Direction::Down);
    break;
  case Key::Left:
    panel_.move(Buttons::Direction::Left);
    break;
  case Key::Right:
    panel_.move(Buttons::Direction::Right);
    break;
  case Key::Confirm:
    if (const auto focused = panel_.focus()) {
      panel_.press(*focused, now);
      type(*focused);
    }
    break;
  case Key::Back:
    panel_.press(KeyDelete, now);
    type(KeyDelete);
    break;
  case Key::Done:
    panel_.press(KeyDone, now);
    type(KeyDone);
    break;
  }
}

void OnScreenKeyboard::touch(int x, int y, double now) {
  if (const auto key = panel_.at(x, y)) {
    panel_.setFocus(key);
    panel_.press(*key, now);
    type(*key);
  }
}

void OnScreenKeyboard::draw(Canvas &canvas, const Style &style, double now) {
  const float k = static_cast<float>(width_) / 256;
  canvas.clear(style.background);
  // The field: the text so far, a cursor after it while typing
  canvas.outline(16 * k, 8 * k, 224 * k, 24 * k, style.field);
  canvas.text(24 * k, 15 * k, text_ + (outcome_ == Outcome::Typing ? "_" : ""), style.words,
              k >= 1.2f ? 2.0f : 1.5f);
  panel_.setTime(now);
  panel_.draw(canvas, style.keys);
}

} // namespace ScratchPadd
