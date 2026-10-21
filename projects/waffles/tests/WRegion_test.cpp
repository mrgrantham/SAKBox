// GRegion: GSubImageFinder2.
#include <gtest/gtest.h>

#include <WClasses/GImage.h>
#include <WClasses/GRand.h>
#include <WClasses/GRect.h>
#include <WClasses/GRegion.h>

#include <utility>
#include <vector>

using namespace GClasses;

// findSubImage keeps the better half of its candidates each round, chosen
// with nth_element on their match errors, and flat areas give many equal
// errors. The positions found were recorded from the code before that moved
// to std::ranges; finding the right place is checked as well where the
// needle is distinctive.
TEST(SubImageFinderTest, FindsNeedlesInAFlatAndNoisyImage) {
  GRand rand(0);
  GImage haystack;
  haystack.setSize(128, 96);
  haystack.clear(0xff202020);
  haystack.boxFill(10, 10, 50, 30, 0xff808030);
  haystack.boxFill(70, 40, 40, 40, 0xffe0e0e0);
  for (int i = 0; i < 300; i++) // scattered noise
    haystack.setPixel((int)rand.next(128), (int)rand.next(96),
                      gARGB(0xff, (int)rand.next(256), (int)rand.next(256), (int)rand.next(256)));
  GSubImageFinder2 finder(&haystack);
  const std::pair<int, int> places[] = {{5, 5}, {60, 35}, {90, 60}, {20, 70}};
  std::vector<std::pair<int, int>> found;
  for (auto [x, y] : places) {
    GRect r(x, y, 24, 24);
    GImage needle;
    needle.setSize(24, 24);
    needle.blit(0, 0, &haystack, &r);
    GRect whole(0, 0, 24, 24);
    int fx = -1, fy = -1;
    finder.findSubImage(&fx, &fy, &needle, &whole);
    found.push_back({fx, fy});
  }
  EXPECT_EQ(found, (std::vector<std::pair<int, int>>{{5, 5}, {60, 35}, {90, 60}, {20, 70}}));
}
