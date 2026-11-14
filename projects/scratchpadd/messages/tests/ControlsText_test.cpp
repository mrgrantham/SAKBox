// The control messages as lines of text: every kind of control through a
// snapshot and back, a change and a request, text with tabs, newlines and
// backslashes surviving, and lines that aren't messages refused.
#include <ScratchPadd/Messages/ControlsText.hpp>

#include <gtest/gtest.h>

using namespace ScratchPadd;
namespace Text = ScratchPadd::ControlsText;

TEST(ControlsTextTest, ASnapshotOfEveryKindComesBackTheSame) {
  MessageType::ControlSnapshot sent{
      .paddName = "RagdollPadd",
      .controlMap = {
          {"Volume", ControlType::Double(0.1, std::make_pair(0.0, 1.0))},
          {"Free", ControlType::Double(1.0 / 3)},
          {"Sub-steps", ControlType::Integer(4, std::make_pair(1, 8))},
          {"Count", ControlType::Integer(-7)},
          {"Blend", ControlType::Boolean(true)},
          {"Steps a second", ControlType::String("15", std::vector<std::string>{"10", "15", "30"})},
          {"Name", ControlType::String("free\ttext")},
          {"Running padds", ControlType::Info("Padd\tRuns\nGamePadd\tOnMainThread\n\\end")},
      }};
  const std::string line = Text::snapshot(sent);
  EXPECT_EQ(line.find('\n'), std::string::npos); // one line
  const auto decoded = Text::decode(line);
  ASSERT_TRUE(decoded);
  const Text::Decoded got = decoded.value_or(Text::Decoded{});
  ASSERT_EQ(got.kind, Text::Decoded::Kind::Snapshot);
  const auto &controls = got.snapshot.controlMap;
  EXPECT_EQ(got.snapshot.paddName, "RagdollPadd");
  ASSERT_EQ(controls.size(), sent.controlMap.size());
  EXPECT_EQ(std::get<ControlType::Double>(controls.at("Volume")).value, 0.1); // exactly
  EXPECT_EQ(std::get<ControlType::Double>(controls.at("Volume")).enforcableRange,
            std::make_optional(std::make_pair(0.0, 1.0)));
  EXPECT_EQ(std::get<ControlType::Double>(controls.at("Free")).value, 1.0 / 3);
  EXPECT_FALSE(std::get<ControlType::Double>(controls.at("Free")).enforcableRange);
  EXPECT_EQ(std::get<ControlType::Integer>(controls.at("Sub-steps")).enforcableRange,
            std::make_optional(std::make_pair(1, 8)));
  EXPECT_EQ(std::get<ControlType::Integer>(controls.at("Count")).value, -7);
  EXPECT_TRUE(std::get<ControlType::Boolean>(controls.at("Blend")).value);
  const auto &steps = std::get<ControlType::String>(controls.at("Steps a second"));
  EXPECT_EQ(steps.value, "15");
  EXPECT_EQ(steps.enforcableRange, std::make_optional(std::vector<std::string>{"10", "15", "30"}));
  EXPECT_EQ(std::get<ControlType::String>(controls.at("Name")).value, "free\ttext");
  EXPECT_FALSE(std::get<ControlType::String>(controls.at("Name")).enforcableRange);
  EXPECT_EQ(std::get<ControlType::Info>(controls.at("Running padds")).value,
            "Padd\tRuns\nGamePadd\tOnMainThread\n\\end");
}

TEST(ControlsTextTest, AChangeAndARequest) {
  const auto decodedChange = Text::decode(Text::change(
      "AudioPadd", {.controlName = "master", .controlTypeValue = ControlType::Double(0.25)}));
  ASSERT_TRUE(decodedChange);
  const Text::Decoded change = decodedChange.value_or(Text::Decoded{});
  ASSERT_EQ(change.kind, Text::Decoded::Kind::Change);
  EXPECT_EQ(change.paddName, "AudioPadd");
  EXPECT_EQ(change.change.controlName, "master");
  EXPECT_EQ(std::get<ControlType::Double>(change.change.controlTypeValue).value, 0.25);

  const auto request = Text::decode(Text::request());
  ASSERT_TRUE(request);
  EXPECT_EQ(request.value_or(Text::Decoded{.kind = Text::Decoded::Kind::Change}).kind,
            Text::Decoded::Kind::Request);
}

TEST(ControlsTextTest, LinesThatArentMessagesAreRefused) {
  EXPECT_FALSE(Text::decode(""));
  EXPECT_FALSE(Text::decode("hello"));
  EXPECT_FALSE(Text::decode("C\tAudioPadd\tmaster"));         // no value
  EXPECT_FALSE(Text::decode("C\tAudioPadd\tmaster\tQ\t1"));   // no such kind
  EXPECT_FALSE(Text::decode("S\tPadd\tName\tS\tx\t3\tonly")); // fewer choices than it says
}
