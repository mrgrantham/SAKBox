#include <CosmicHex/Handheld/StartMenu.hpp>

namespace CosmicHex::Handheld {

namespace {
constexpr int kRows = 6;
// The layout, as parts of the screen: a margin, each row's height, and the
// arrows' width
constexpr float kMargin = 0.05f;
constexpr float kArrow = 0.11f;
} // namespace

StartMenu::StartMenu() : boards_(boardChoices(players_)) {}

std::array<int, kMaxPlayers> StartMenu::seatBonus() const {
  if (seatBonus_ && boards_[board_].hasSeatBonus())
    return boards_[board_].seatBonus;
  return {};
}

std::string StartMenu::label(Row row) const {
  switch (row) {
  case Row::Players:
    return "Players";
  case Row::Board:
    return "Board";
  case Row::End:
    return "End";
  case Row::SeatBonus:
    return "Bonus";
  case Row::Mode:
    return "Mode";
  case Row::Start:
    return "";
  }
  return "";
}

std::string StartMenu::value(Row row) const {
  switch (row) {
  case Row::Players:
    return players_ == 4 ? "4: you + 3 bots" : "3: you + 2 bots";
  case Row::Board:
    return boardName() + (board_ == 0 ? " (default)" : "");
  case Row::End:
    return finishRound_ ? "finish the round" : "first to the target";
  case Row::SeatBonus:
    if (!boards_[board_].hasSeatBonus())
      return "none for this board";
    return seatBonus_ ? "later seats +1" : "off";
  case Row::Mode:
    return watching_ ? "watch the bots" : "you play";
  case Row::Start:
    return "";
  }
  return "";
}

void StartMenu::change(Row row, int step) {
  switch (row) {
  case Row::Players:
    players_ = players_ == 4 ? 3 : 4;
    boards_ = boardChoices(players_);
    board_ = 0; // (the new count's default)
    break;
  case Row::Board: {
    const std::size_t count = boards_.size();
    board_ = step > 0 ? (board_ + 1) % count : (board_ + count - 1) % count;
    break;
  }
  case Row::End:
    finishRound_ = !finishRound_;
    break;
  case Row::SeatBonus:
    if (boards_[board_].hasSeatBonus())
      seatBonus_ = !seatBonus_;
    break;
  case Row::Mode:
    watching_ = !watching_;
    break;
  case Row::Start:
    back_ = !back_;
    break;
  }
}

StartMenu::Outcome StartMenu::press(Key key) {
  const int row = static_cast<int>(focus_);
  switch (key) {
  case Key::Up:
    focus_ = static_cast<Row>((row + kRows - 1) % kRows);
    return Outcome::None;
  case Key::Down:
    focus_ = static_cast<Row>((row + 1) % kRows);
    return Outcome::None;
  case Key::Left:
  case Key::Right:
    change(focus_, key == Key::Right ? 1 : -1);
    return Outcome::None;
  case Key::Confirm:
    return focus_ == Row::Start && back_ ? Outcome::Back : Outcome::Start;
  case Key::Back:
    return Outcome::Back;
  }
  return Outcome::None;
}

std::vector<StartMenu::Button> StartMenu::buttons(int width, int height) const {
  const float w = static_cast<float>(width), h = static_cast<float>(height);
  const float rowHeight = (h * (1 - 2 * kMargin)) / kRows;
  const float left = w * kMargin, right = w * (1 - kMargin);
  std::vector<Button> out;
  for (int r = 0; r < kRows - 1; ++r) {
    const float y = h * kMargin + rowHeight * static_cast<float>(r) + rowHeight * 0.4f;
    const float height = rowHeight * 0.55f;
    const auto row = static_cast<Row>(r);
    out.push_back({row, -1, Outcome::None, left + w * 0.22f, y, w * kArrow, height});
    out.push_back({row, +1, Outcome::None, right - w * kArrow, y, w * kArrow, height});
  }
  const float y = h * kMargin + rowHeight * (kRows - 1) + rowHeight * 0.15f;
  const float half = (right - left) / 2;
  out.push_back({Row::Start, 0, Outcome::Start, left, y, half - w * 0.02f, rowHeight * 0.75f});
  out.push_back({Row::Start, 0, Outcome::Back, left + half + w * 0.02f, y, half - w * 0.02f,
                 rowHeight * 0.75f});
  return out;
}

StartMenu::Area StartMenu::valueArea(Row row, int width, int height) const {
  for (const Button &left : buttons(width, height))
    if (left.row == row && left.step == -1)
      for (const Button &right : buttons(width, height))
        if (right.row == row && right.step == +1)
          return {left.x + left.width, left.y, right.x - (left.x + left.width), left.height};
  return {};
}

StartMenu::Outcome StartMenu::touch(int x, int y, int width, int height) {
  const float fx = static_cast<float>(x), fy = static_cast<float>(y);
  for (const Button &button : buttons(width, height)) {
    if (fx < button.x || fx >= button.x + button.width || fy < button.y ||
        fy >= button.y + button.height)
      continue;
    focus_ = button.row;
    if (button.step != 0) {
      change(button.row, button.step);
      return Outcome::None;
    }
    back_ = button.starts == Outcome::Back;
    return button.starts;
  }
  return Outcome::None;
}

} // namespace CosmicHex::Handheld
