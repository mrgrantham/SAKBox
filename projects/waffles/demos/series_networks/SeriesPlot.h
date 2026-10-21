#ifndef SERIES_PLOT_H
#define SERIES_PLOT_H

#include <WClasses/WMatrix.h>
#include <WClasses/WNeuralNet.h>

#include <string>

namespace WClasses
{

/// Draws, as an SVG at `path`, the curve nn makes over a time series (one
/// input, time; one output, the value) from the first training time to the
/// last test time, with the training points as blue dots and the test points
/// as red ones. (The fourier_net and time_series demos draw their progress
/// with it.)
void plotSeries(const std::string& path, WNeuralNet& nn, WMatrix& trainFeat, WMatrix& trainLab, WMatrix& testFeat, WMatrix& testLab);

} // namespace WClasses

#endif // SERIES_PLOT_H
