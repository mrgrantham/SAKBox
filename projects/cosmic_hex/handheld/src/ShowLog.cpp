#include <CosmicHex/Handheld/ShowLog.hpp>

#include <cstdio>

namespace CosmicHex::Handheld {

std::string_view nameOf(ShowKind kind) {
  switch (kind) {
  case ShowKind::Banner:
    return "banner";
  case ShowKind::Dice:
    return "dice";
  case ShowKind::Pieces:
    return "pieces";
  case ShowKind::Discovery:
    return "discovery";
  case ShowKind::HomePay:
    return "home pay";
  case ShowKind::CardFlights:
    return "card flights";
  case ShowKind::CardPlay:
    return "card played";
  case ShowKind::Dealt:
    return "cards dealt";
  case ShowKind::FirstRoll:
    return "first roll";
  case ShowKind::Hand:
    return "hand";
  case ShowKind::Celebration:
    return "celebration";
  }
  return "?";
}

std::string_view ShowLog::nameOf(Cut cut) {
  switch (cut) {
  case Cut::None:
    return "played";
  case Cut::SkipPressed:
    return "skip pressed";
  case Cut::AnimationsOff:
    return "animations off";
  case Cut::SkippedQueued:
    return "skipped queued";
  case Cut::Replaced:
    return "replaced";
  case Cut::Covered:
    return "covered";
  case Cut::LateStart:
    return "late start";
  case Cut::NeverSeen:
    return "never seen";
  }
  return "?";
}

ShowLog::Cut ShowLog::Entry::cut() const {
  if (animationsOff)
    return Cut::AnimationsOff;
  if (skipPressed)
    return Cut::SkipPressed;
  if (skippedQueued && !everMoved)
    return Cut::SkippedQueued;
  if (replaced)
    return Cut::Replaced;
  if (!everMoved || seenSeconds <= 0)
    return Cut::NeverSeen;
  if (lateBy > 0)
    return Cut::LateStart;
  if (covered)
    return Cut::Covered;
  return Cut::None;
}

void ShowLog::frame(double now, double step, std::span<const ShowSeen> shows, ShowKind onTop) {
  now_ = now;
  for (const ShowSeen &seen : shows) {
    const auto k = static_cast<std::size_t>(seen.kind);
    Open &open = open_[k];
    // (gone, or a new one in its place; one that hadn't moved yet keeps
    // its entry as its start is put back: a close-up drawn first, say)
    if (open.on && !seen.on)
      close(k, now);
    else if (open.on && seen.key != open.key && open.entry.everMoved) {
      // (B moving a show on in place, as the banner to its waiting
      // prompt: the same show, skipped, not a new one starting late)
      const bool skippedNow = open.entry.skipPressed && skippedSinceFrame_;
      close(k, now);
      if (skippedNow) {
        open.on = true;
        open.key = seen.key;
        open.entry = Entry{.kind = seen.kind, .startedAt = now, .endedAt = now, .note = context_};
        open.entry.skipPressed = open.entry.everMoved = true;
        open.entry.seenSeconds = open.entry.movingSeconds = step;
        continue;
      }
    } else if (open.on) {
      open.key = seen.key;
    }
    if (!seen.on)
      continue;
    if (!open.on) {
      open.on = true;
      open.key = seen.key;
      open.entry = Entry{.kind = seen.kind, .startedAt = now, .endedAt = now, .note = context_};
    }
    Entry &entry = open.entry;
    if (!seen.moving)
      continue;
    // (first seen moving more than a frame or two in: its start was missed)
    const double slack = 2.5 * step * seen.rate + 1e-6;
    if (!entry.everMoved && seen.elapsed > slack)
      entry.lateBy = seen.elapsed;
    entry.everMoved = true;
    entry.movingSeconds += step;
    if (seen.visible) {
      entry.seenSeconds += step;
    } else if (!entry.covered) {
      entry.covered = true;
      entry.coveredBy = onTop;
    }
  }
  skippedSinceFrame_ = false;
}

void ShowLog::skipped(bool pressed) {
  skippedSinceFrame_ = true;
  for (Open &open : open_) {
    if (!open.on)
      continue;
    if (!pressed)
      open.entry.animationsOff = true;
    else if (open.entry.everMoved)
      open.entry.skipPressed = true;
    else
      open.entry.skippedQueued = true;
  }
}

void ShowLog::replaced(ShowKind kind) {
  Open &open = open_[static_cast<std::size_t>(kind)];
  if (open.on)
    open.entry.replaced = true;
}

void ShowLog::close(std::size_t kind, double now) {
  Open &open = open_[kind];
  open.on = false;
  open.entry.endedAt = now;
  ++counts_[kind][static_cast<std::size_t>(open.entry.cut())];
  if (keepEntries_)
    entries_.push_back(open.entry);
}

void ShowLog::finish(double now) {
  for (std::size_t k = 0; k < open_.size(); ++k)
    if (open_[k].on)
      close(k, now);
}

std::size_t ShowLog::count(ShowKind kind, Cut cut) const {
  return counts_[static_cast<std::size_t>(kind)][static_cast<std::size_t>(cut)];
}

std::size_t ShowLog::count(Cut cut) const {
  std::size_t n = 0;
  for (const auto &kind : counts_)
    n += kind[static_cast<std::size_t>(cut)];
  return n;
}

std::size_t ShowLog::shows() const {
  std::size_t n = 0;
  for (const auto &kind : counts_)
    for (const std::size_t c : kind)
      n += c;
  return n;
}

std::string ShowLog::summary() const {
  std::string table = "show          ";
  for (std::size_t c = 0; c < kCuts; ++c) {
    char heading[24];
    std::snprintf(heading, sizeof heading, "%15.15s",
                  std::string(nameOf(static_cast<Cut>(c))).c_str());
    table += heading;
  }
  table += '\n';
  for (std::size_t k = 0; k < kShowKinds; ++k) {
    char row[24];
    std::snprintf(row, sizeof row, "%-14.14s",
                  std::string(Handheld::nameOf(static_cast<ShowKind>(k))).c_str());
    table += row;
    for (std::size_t c = 0; c < kCuts; ++c) {
      char cell[24];
      std::snprintf(cell, sizeof cell, "%15zu", counts_[k][c]);
      table += cell;
    }
    table += '\n';
  }
  return table;
}

} // namespace CosmicHex::Handheld
