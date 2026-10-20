#ifndef SERIES_PLOT_H
#define SERIES_PLOT_H

#include <WClasses/GMatrix.h>
#include <WClasses/GNeuralNet.h>

#include <string>

namespace GClasses
{

/// Draws, as an SVG at `path`, the curve nn makes over a time series (one
/// input, time; one output, the value) from the first training time to the
/// last test time, with the training points as blue dots and the test points
/// as red ones. (The fourier_net and time_series demos draw their progress
/// with it.)
void plotSeries(const std::string& path, GNeuralNet& nn, GMatrix& trainFeat, GMatrix& trainLab, GMatrix& testFeat, GMatrix& testLab);

} // namespace GClasses

#endif // SERIES_PLOT_H
