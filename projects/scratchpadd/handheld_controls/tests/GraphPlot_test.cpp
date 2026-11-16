// Which pixels draw a graph: bars from the bottom with gaps between, a line
// joining each value to the next, spacing by the graph's length (filling in
// from the left), the newest values when there are more than columns, the
// scale fitted when there's no range, and nothing drawn outside the box.
#include <ScratchPadd/HandheldControls/GraphPlot.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

using ScratchPadd::ControlType::Graph;
using ScratchPadd::HandheldControls::plotGraph;

namespace {

// The graph plotted in a box `width` x `height`, a string a row ('#' for a
// pixel set), failing the test for any pixel outside the box
std::vector<std::string> picture(const Graph &graph, int width, int height) {
  std::vector<std::string> rows(static_cast<std::size_t>(height),
                                std::string(static_cast<std::size_t>(width), '.'));
  plotGraph(graph, width, height, [&](int x, int y) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
      ADD_FAILURE() << "pixel outside the box: " << x << ", " << y;
      return;
    }
    rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = '#';
  });
  return rows;
}

Graph withValues(Graph graph, const std::vector<float> &values) {
  for (float value : values)
    graph.add(value);
  return graph;
}

} // namespace

TEST(GraphPlotTest, BarsFromTheBottomWithGaps) {
  const Graph bars = withValues(Graph(4, Graph::Style::Bars, {{0.0f, 3.0f}}), {0, 1, 2, 3});
  EXPECT_EQ(picture(bars, 8, 4), (std::vector<std::string>{"......#.", //
                                                           "....#.#.", //
                                                           "..#.#.#.", //
                                                           "#.#.#.#."}));
}

TEST(GraphPlotTest, ALineJoinsEachValueToTheNext) {
  const Graph line = withValues(Graph(4, Graph::Style::Line, {{0.0f, 3.0f}}), {0, 3, 1, 1});
  EXPECT_EQ(picture(line, 8, 4), (std::vector<std::string>{"..###...", //
                                                           "..#.#...", //
                                                           "..#.####", //
                                                           "###....."}));
}

// Spaced by the graph's length: two values of a graph keeping four take
// the left half
TEST(GraphPlotTest, ANewGraphFillsInFromTheLeft) {
  const Graph bars = withValues(Graph(4, Graph::Style::Bars, {{0.0f, 1.0f}}), {1, 1});
  EXPECT_EQ(picture(bars, 8, 2), (std::vector<std::string>{"#.#.....", //
                                                           "#.#....."}));
}

// More values than columns: the newest, a column each
TEST(GraphPlotTest, TheNewestWhenThereAreMoreValuesThanColumns) {
  const Graph line =
      withValues(Graph(10, Graph::Style::Line, {{0.0f, 1.0f}}), {1, 1, 1, 1, 1, 1, 0, 0, 0, 1});
  EXPECT_EQ(picture(line, 4, 2), (std::vector<std::string>{"...#", //
                                                           "####"}));
}

// No range: from the lowest value shown to the highest; all the same: the
// middle
TEST(GraphPlotTest, FittedToTheValuesWithoutARange) {
  const Graph line = withValues(Graph(3, Graph::Style::Line), {10, 20, 30});
  EXPECT_EQ(picture(line, 3, 3), (std::vector<std::string>{"..#", //
                                                           ".##", //
                                                           "##."}));
  const Graph flat = withValues(Graph(3, Graph::Style::Line), {5, 5, 5});
  EXPECT_EQ(picture(flat, 3, 3), (std::vector<std::string>{"...", //
                                                           "###", //
                                                           "..."}));
}

// Values beyond the range are drawn at its edge; nothing at all for an
// empty graph or box
TEST(GraphPlotTest, StaysInsideTheBox) {
  const Graph wild = withValues(Graph(2, Graph::Style::Bars, {{0.0f, 1.0f}}), {-5, 9});
  EXPECT_EQ(picture(wild, 4, 2), (std::vector<std::string>{"..#.", //
                                                           "#.#."}));
  EXPECT_EQ(picture(Graph(4), 4, 2), (std::vector<std::string>{"....", "...."}));
  int pixels = 0;
  plotGraph(wild, 0, 5, [&](int, int) { ++pixels; });
  EXPECT_EQ(pixels, 0);
}
