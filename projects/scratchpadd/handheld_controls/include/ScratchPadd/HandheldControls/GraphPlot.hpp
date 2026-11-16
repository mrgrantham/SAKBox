// GraphPlot.hpp - which pixels draw a graph (ControlType::Graph) in a box
// `width` x `height` pixels, for screens that draw graphs pixel by pixel
// (the DS's and 3DS's bottom screens: ConsolePanel.hpp's GraphPixels). It
// only says where; the screen sets the pixels, so this part is the same
// everywhere and tested on desktops.
//
//   plotGraph(graph, 300, 24, [&](int x, int y) { pixels[y * pitch + x] = colour; });
//
// The graph's length (how many values it keeps) sets the spacing, so a new
// graph fills in from the left and, once full, scrolls: each value gets
// width / length columns (at least 1; with more values than columns, the
// newest `width` are shown). Bars fill up from the bottom, at least a pixel
// tall so a zero shows as a baseline, with a column's gap between them when
// there's room; a line joins each value to the next. The scale is the
// graph's range, or with none, the lowest to the highest value shown.
#pragma once

#include <ScratchPadd/Messages/Controls.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace ScratchPadd::HandheldControls {

// Calls plot(x, y) for each pixel of `graph` drawn in a box `width` x
// `height` (x from the left, y from the top)
template <typename Plot>
void plotGraph(const ControlType::Graph &graph, int width, int height, Plot &&plot) {
  if (graph.values.empty() || width <= 0 || height <= 0)
    return;

  // The values shown, oldest first, and the columns each gets
  const auto columns = static_cast<std::size_t>(width);
  const std::size_t shown = std::min(graph.values.size(), columns);
  const auto first = graph.values.end() - static_cast<std::ptrdiff_t>(shown);
  const int columnsEach =
      std::max(1, width / static_cast<int>(std::max<std::size_t>(graph.length, 1)));

  // The scale: the graph's range, or the values' own
  float low = 0, high = 0;
  if (graph.range) {
    low = graph.range->first;
    high = graph.range->second;
  } else {
    const auto [least, most] = std::minmax_element(first, graph.values.end());
    low = *least;
    high = *most;
  }
  // A value's height in pixels above the bottom row, 0 to height - 1
  const auto heightOf = [&](float value) {
    const float share = high > low ? std::clamp((value - low) / (high - low), 0.0f, 1.0f) : 0.5f;
    return static_cast<int>(std::lround(share * static_cast<float>(height - 1)));
  };
  const int bottom = height - 1;

  int previousY = -1;
  for (std::size_t i = 0; i < shown; ++i) {
    const int left = static_cast<int>(i) * columnsEach;
    if (left >= width)
      break;
    const int right = std::min(left + columnsEach, width); // (one past)
    const int y = bottom - heightOf(*(first + static_cast<std::ptrdiff_t>(i)));
    if (graph.style == ControlType::Graph::Style::Bars) {
      // The last column left as a gap between bars, if there's room for one
      const int barRight = columnsEach >= 2 ? right - 1 : right;
      for (int x = left; x < barRight; ++x)
        for (int row = y; row <= bottom; ++row)
          plot(x, row);
    } else {
      // Up or down from the last value's height, then across
      if (previousY >= 0)
        for (int row = std::min(previousY, y); row <= std::max(previousY, y); ++row)
          plot(left, row);
      for (int x = left; x < right; ++x)
        plot(x, y);
      previousY = y;
    }
  }
}

} // namespace ScratchPadd::HandheldControls
