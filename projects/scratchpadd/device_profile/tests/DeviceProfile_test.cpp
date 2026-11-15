// The device profile: each platform's built-in profile, its adjustable values
// as text, and the current profile for the run.
#include <ScratchPadd/DeviceProfile/DeviceProfile.hpp>

#include <gtest/gtest.h>

#include <type_traits>

using ScratchPadd::DevicePlatform;
using ScratchPadd::DeviceProfile;

// It can be shared state: plain values only
static_assert(std::is_trivially_copyable_v<DeviceProfile>);

TEST(DeviceProfileTest, TheDsProfile) {
  const DeviceProfile ds = DeviceProfile::forPlatform(DevicePlatform::Ds);
  EXPECT_EQ(ds.nameView(), "DS Lite");
  EXPECT_FALSE(ds.device.fpu);
  EXPECT_EQ(ds.device.cpuMHz, 67);
  EXPECT_EQ(ds.screens.count, 2);
  EXPECT_TRUE(ds.screens.touch);
  EXPECT_EQ(ds.scene.maxPolygons, 2048);
  EXPECT_EQ(ds.scene.framesPerSecond, 30);
  EXPECT_EQ(ds.physics.stepsPerSecond, 15);
  EXPECT_EQ(ds.textures.colours, 16);
  EXPECT_TRUE(ds.audio.hardwareChannels);
  EXPECT_EQ(ds.controls.defaultKind(), ScratchPadd::Input::Kind::Ds);
}

TEST(DeviceProfileTest, TheThreeDsAndDesktops) {
  const DeviceProfile threeDs = DeviceProfile::forPlatform(DevicePlatform::ThreeDs);
  EXPECT_TRUE(threeDs.screens.stereo);
  EXPECT_EQ(threeDs.screens.width, 400);
  EXPECT_EQ(threeDs.physics.stepsPerSecond, 60);
  EXPECT_EQ(threeDs.controls.defaultKind(), ScratchPadd::Input::Kind::ThreeDs);
  for (DevicePlatform desktop :
       {DevicePlatform::Mac, DevicePlatform::Windows, DevicePlatform::Linux}) {
    const DeviceProfile p = DeviceProfile::forPlatform(desktop);
    EXPECT_EQ(p.scene.maxPolygons, 0); // no limit
    EXPECT_EQ(p.controls.defaultKind(), ScratchPadd::Input::Kind::Keyboard);
  }
  EXPECT_EQ(DeviceProfile::forThisDevice().platform, DeviceProfile::thisPlatform());
}

TEST(DeviceProfileTest, AdjustmentsAsText) {
  DeviceProfile ds = DeviceProfile::forPlatform(DevicePlatform::Ds);
  ds.physics.subSteps = 2;
  ds.scene.fog = false;
  const std::string text = ScratchPadd::profileText(ds);
  EXPECT_NE(text.find("physics.subSteps 2\n"), std::string::npos) << text;
  EXPECT_NE(text.find("scene.fog 0\n"), std::string::npos) << text;
  EXPECT_EQ(ScratchPadd::profileFromText(text, DeviceProfile::forPlatform(DevicePlatform::Ds)), ds);
}

// Lines that aren't values (an older or newer build's) are skipped, and
// values not given keep the start's
TEST(DeviceProfileTest, SavedAdjustmentsSurviveChanges) {
  const DeviceProfile start = DeviceProfile::forPlatform(DevicePlatform::Ds);
  const DeviceProfile read =
      ScratchPadd::profileFromText("physics.subSteps 8\nphysics.warp 3\nscene.fog maybe\n", start);
  EXPECT_EQ(read.physics.subSteps, 8);
  EXPECT_EQ(read.scene.fog, start.scene.fog);
  EXPECT_EQ(read.physics.stepsPerSecond, start.physics.stepsPerSecond);
}

TEST(DeviceProfileTest, TheCurrentProfile) {
  DeviceProfile changed = ScratchPadd::currentProfile();
  changed.audio.voices = 7;
  ScratchPadd::setCurrentProfile(changed);
  EXPECT_EQ(ScratchPadd::currentProfile().audio.voices, 7);
}
