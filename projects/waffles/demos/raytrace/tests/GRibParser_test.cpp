// GRibParser: the RenderMan scene reader behind the raytrace demo.
#include <gtest/gtest.h>

#include "GRibParser.h"

#include <WClasses/GError.h>
#include <WClasses/GImage.h>
#include <WClasses/GRand.h>
#include <WClasses/GRayTrace.h>

#include <cstdio>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <string>

using namespace GClasses;

namespace {
const std::string kScenes = "projects/waffles/demos/raytrace/bin/";

std::string readFile(const std::string &path) {
  std::ifstream in(path);
  std::stringstream text;
  text << in.rdbuf();
  return text.str();
}

// Parses a scene from text, as if it were a file
std::unique_ptr<GRayTraceScene> parse(const std::string &text, GRand &rand) {
  FILE *file = tmpfile();
  if (file == nullptr || fputs(text.c_str(), file) < 0 || fseek(file, 0, SEEK_SET) != 0) {
    ADD_FAILURE() << "couldn't write the scene to a temporary file";
    return nullptr;
  }
  RibParser parser(&rand);
  const bool parsed = parser.createScene(file); // (closes the file)
  EXPECT_TRUE(parsed);
  return std::unique_ptr<GRayTraceScene>(parser.releaseScene());
}

GImage *render(GRayTraceScene &scene) {
  scene.setRenderMode(GRayTraceScene::FAST_RAY_TRACE);
  scene.renderBegin();
  while (scene.renderLine()) {
  }
  return scene.image();
}

size_t colourCount(GImage &image) {
  std::set<unsigned int> colours(image.pixels(), image.pixels() + image.width() * image.height());
  return colours.size();
}

bool samePixels(GImage &a, GImage &b) {
  return a.width() == b.width() && a.height() == b.height() &&
         std::equal(a.pixels(), a.pixels() + a.width() * a.height(), b.pixels());
}

std::string removeIndents(const std::string &text) {
  std::stringstream in(text), out;
  std::string line;
  while (std::getline(in, line))
    out << line.substr(std::min(line.find_first_not_of(" \t"), line.size())) << "\n";
  return out.str();
}

std::string replaced(std::string text, const std::string &from, const std::string &to) {
  const size_t at = text.find(from);
  EXPECT_NE(at, std::string::npos) << from;
  return text.replace(at, from.size(), to);
}
} // namespace

// Every sample scene loads and renders a picture of the size its Format line
// asks for, with something in it.
TEST(RibParserTest, SampleScenesRender) {
  for (const char *name : {"fiveSpheres.rib", "billiards.rib", "arvo.rib"}) {
    SCOPED_TRACE(name);
    GRand rand(0);
    std::unique_ptr<GRayTraceScene> scene(RibParser::LoadScene((kScenes + name).c_str(), &rand));
    ASSERT_NE(scene, nullptr);
    GImage *image = render(*scene);
    EXPECT_GT(image->width(), 0u);
    EXPECT_GT(colourCount(*image), 10u);
  }
  GRand rand(0);
  std::unique_ptr<GRayTraceScene> scene(
      RibParser::LoadScene((kScenes + "fiveSpheres.rib").c_str(), &rand));
  ASSERT_NE(scene, nullptr);
  GImage *image = render(*scene);
  EXPECT_EQ(image->width(), 256u); // Format 256 256 1
  EXPECT_EQ(image->height(), 256u);
}

// Indenting lines with spaces or tabs doesn't change a scene. (buildData's
// check for leading whitespace was always true, so it skipped none; the
// tokenizer copes with them anyway, so fixing it changed nothing visible.)
TEST(RibParserTest, IndentedLinesReadTheSameAsUnindented) {
  const std::string text = readFile(kScenes + "fiveSpheres.rib");
  ASSERT_NE(text.find("\n    Translate"), std::string::npos); // the sample is indented
  GRand rand1(0), rand2(0);
  std::unique_ptr<GRayTraceScene> indented = parse(text, rand1);
  std::unique_ptr<GRayTraceScene> flat = parse(removeIndents(text), rand2);
  ASSERT_NE(indented, nullptr);
  ASSERT_NE(flat, nullptr);
  EXPECT_TRUE(samePixels(*render(*indented), *render(*flat)));
  const std::string tabbed = replaced(text, "\n    Translate", "\n\t \tTranslate");
  GRand rand3(0);
  std::unique_ptr<GRayTraceScene> withTabs = parse(tabbed, rand3);
  ASSERT_NE(withTabs, nullptr);
  EXPECT_TRUE(samePixels(*render(*withTabs), *render(*flat)));
}

// A scene that doesn't set rayDepth gets the default, 4.
TEST(RibParserTest, RayDepthDefaultsToFour) {
  const std::string text = readFile(kScenes + "fiveSpheres.rib");
  const std::string rayDepth = "Attribute \"cs655Setting\" \"rayDepth\" [ 4 ]";
  GRand rand1(0), rand2(0), rand3(0);
  std::unique_ptr<GRayTraceScene> four = parse(text, rand1);
  std::unique_ptr<GRayTraceScene> unset = parse(replaced(text, rayDepth, ""), rand2);
  std::unique_ptr<GRayTraceScene> one =
      parse(replaced(text, rayDepth, "Attribute \"cs655Setting\" \"rayDepth\" [ 1 ]"), rand3);
  ASSERT_TRUE(four && unset && one);
  EXPECT_TRUE(samePixels(*render(*unset), *render(*four)));
  EXPECT_FALSE(samePixels(*render(*one), *render(*four))); // (so rayDepth is read)
}

// A number that isn't one is an error, rather than read as 0.
TEST(RibParserTest, ANumberThatIsNotOneIsAnError) {
  const std::string text =
      replaced(readFile(kScenes + "fiveSpheres.rib"), "Format 256 256 1", "Format wide 256 1");
  GRand rand(0);
  try {
    parse(text, rand);
    FAIL() << "expected an error";
  } catch (const std::exception &e) {
    EXPECT_NE(std::string(e.what()).find("wide"), std::string::npos) << e.what();
  }
}
