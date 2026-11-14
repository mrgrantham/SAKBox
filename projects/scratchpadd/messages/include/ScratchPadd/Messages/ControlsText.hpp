// ControlsText.hpp - the control messages (Controls.hpp) as lines of text,
// to carry them between devices (a console's RemoteControlsPadd and the
// Mac's remote controls app). One message a line:
//
//   R                                   a ControlRequest: every padd's snapshot, please
//   S <padd> {<control> <kind> <fields>}...   a ControlSnapshot
//   C <padd> <control> <kind> <fields>         a ControlChange, for that padd
//
// the fields separated by tabs, a control's kind one letter and its fields:
//   D value hasRange low high     a Double      I value hasRange low high   an Integer
//   B value (0 or 1)              a Boolean     N text                      Info
//   S text count choice...        a String (count 0: free text)
// Text in a field has \\, a tab and a newline escaped (\\\\, \\t, \\n), so
// an Info table's lines and columns survive it. Numbers are written in full
// (17 significant digits), so a Double comes back the same.
#pragma once

#include <ScratchPadd/Messages/Controls.hpp>

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ScratchPadd::ControlsText {

// What a line held
struct Decoded {
  enum class Kind { Request, Snapshot, Change };
  Kind kind = Kind::Request;
  MessageType::ControlSnapshot snapshot; // (Snapshot)
  std::string paddName;                  // (Change: whose)
  MessageType::ControlChange change;     // (Change)
};

namespace Detail {

inline std::string escaped(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    if (c == '\\')
      out += "\\\\";
    else if (c == '\t')
      out += "\\t";
    else if (c == '\n')
      out += "\\n";
    else
      out += c;
  }
  return out;
}

inline std::string unescaped(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\\' && i + 1 < text.size()) {
      const char next = text[++i];
      out += next == 't' ? '\t' : next == 'n' ? '\n' : next;
    } else {
      out += text[i];
    }
  }
  return out;
}

inline std::string number(double value) {
  char text[32];
  std::snprintf(text, sizeof text, "%.17g", value);
  return text;
}

// A control's kind and fields, each with a tab before it
inline std::string fieldsOf(const ControlTypeVariant &control) {
  std::string out;
  auto field = [&](const std::string &text) { out += '\t' + text; };
  if (const auto *d = std::get_if<ControlType::Double>(&control)) {
    field("D");
    field(number(d->value));
    field(d->enforcableRange ? "1" : "0");
    field(number(d->enforcableRange ? d->enforcableRange->first : 0));
    field(number(d->enforcableRange ? d->enforcableRange->second : 0));
  } else if (const auto *i = std::get_if<ControlType::Integer>(&control)) {
    field("I");
    field(std::to_string(i->value));
    field(i->enforcableRange ? "1" : "0");
    field(std::to_string(i->enforcableRange ? i->enforcableRange->first : 0));
    field(std::to_string(i->enforcableRange ? i->enforcableRange->second : 0));
  } else if (const auto *b = std::get_if<ControlType::Boolean>(&control)) {
    field("B");
    field(b->value ? "1" : "0");
  } else if (const auto *info = std::get_if<ControlType::Info>(&control)) {
    field("N");
    field(escaped(info->value));
  } else {
    const auto &s = std::get<ControlType::String>(control);
    field("S");
    field(escaped(s.value));
    field(std::to_string(s.enforcableRange ? s.enforcableRange->size() : 0));
    if (s.enforcableRange)
      for (const std::string &choice : *s.enforcableRange)
        field(escaped(choice));
  }
  return out;
}

// Reads a control's kind and fields from `fields` at `at`, moving it on
inline std::optional<ControlTypeVariant> controlFrom(const std::vector<std::string> &fields,
                                                     std::size_t &at) {
  auto next = [&]() -> std::optional<std::string> {
    if (at >= fields.size())
      return std::nullopt;
    return fields[at++];
  };
  const auto kind = next();
  if (!kind)
    return std::nullopt;
  if (*kind == "D" || *kind == "I") {
    const auto value = next(), ranged = next(), low = next(), high = next();
    if (!high)
      return std::nullopt;
    if (*kind == "D") {
      ControlType::Double d(std::strtod(value->c_str(), nullptr));
      if (*ranged == "1")
        d.enforcableRange =
            std::make_pair(std::strtod(low->c_str(), nullptr), std::strtod(high->c_str(), nullptr));
      return d;
    }
    ControlType::Integer i(static_cast<int32_t>(std::strtol(value->c_str(), nullptr, 10)));
    if (*ranged == "1")
      i.enforcableRange =
          std::make_pair(static_cast<int32_t>(std::strtol(low->c_str(), nullptr, 10)),
                         static_cast<int32_t>(std::strtol(high->c_str(), nullptr, 10)));
    return i;
  }
  if (*kind == "B") {
    const auto value = next();
    if (!value)
      return std::nullopt;
    return ControlType::Boolean(*value == "1");
  }
  if (*kind == "N") {
    const auto text = next();
    if (!text)
      return std::nullopt;
    return ControlType::Info(unescaped(*text));
  }
  if (*kind == "S") {
    const auto text = next(), count = next();
    if (!count)
      return std::nullopt;
    ControlType::String s(unescaped(*text));
    const long choices = std::strtol(count->c_str(), nullptr, 10);
    if (choices > 0) {
      std::vector<std::string> allowed;
      for (long c = 0; c < choices; ++c) {
        const auto choice = next();
        if (!choice)
          return std::nullopt;
        allowed.push_back(unescaped(*choice));
      }
      s.enforcableRange = std::move(allowed);
    }
    return s;
  }
  return std::nullopt;
}

} // namespace Detail

inline std::string request() { return "R"; }

inline std::string snapshot(const MessageType::ControlSnapshot &snapshot) {
  std::string line = "S\t" + Detail::escaped(snapshot.paddName);
  for (const auto &[name, control] : snapshot.controlMap)
    line += '\t' + Detail::escaped(name) + Detail::fieldsOf(control);
  return line;
}

inline std::string change(const std::string &paddName, const MessageType::ControlChange &change) {
  return "C\t" + Detail::escaped(paddName) + '\t' + Detail::escaped(change.controlName) +
         Detail::fieldsOf(change.controlTypeValue);
}

// What `line` holds, or nothing if it isn't one of these
inline std::optional<Decoded> decode(std::string_view line) {
  std::vector<std::string> fields;
  for (std::size_t start = 0;;) {
    const std::size_t tab = line.find('\t', start);
    fields.emplace_back(line.substr(start, tab == std::string_view::npos ? tab : tab - start));
    if (tab == std::string_view::npos)
      break;
    start = tab + 1;
  }
  Decoded decoded;
  if (fields[0] == "R" && fields.size() == 1)
    return decoded;
  if (fields[0] == "S" && fields.size() >= 2) {
    decoded.kind = Decoded::Kind::Snapshot;
    decoded.snapshot.paddName = Detail::unescaped(fields[1]);
    for (std::size_t at = 2; at < fields.size();) {
      const std::string name = Detail::unescaped(fields[at++]);
      const auto control = Detail::controlFrom(fields, at);
      if (!control)
        return std::nullopt;
      decoded.snapshot.controlMap.emplace(name, *control);
    }
    return decoded;
  }
  if (fields[0] == "C" && fields.size() >= 4) {
    decoded.kind = Decoded::Kind::Change;
    decoded.paddName = Detail::unescaped(fields[1]);
    decoded.change.controlName = Detail::unescaped(fields[2]);
    std::size_t at = 3;
    const auto control = Detail::controlFrom(fields, at);
    if (!control || at != fields.size())
      return std::nullopt;
    decoded.change.controlTypeValue = *control;
    return decoded;
  }
  return std::nullopt;
}

} // namespace ScratchPadd::ControlsText
