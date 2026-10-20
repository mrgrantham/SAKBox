#include "SeriesPlot.h"

#include <WClasses/GPlot.h>
#include <WClasses/GVec.h>

#include <algorithm>
#include <fstream>

namespace GClasses
{

void plotSeries(const std::string& path, GNeuralNet& nn, GMatrix& trainFeat, GMatrix& trainLab, GMatrix& testFeat, GMatrix& testLab)
{
	GSVG svg(1000, 500);
	double xmin = trainFeat[0][0];
	double xmax = testFeat[testFeat.rows() - 1][0];
	svg.newChart(xmin, std::min(trainLab.columnMin(0), testLab.columnMin(0)), xmax, std::max(trainLab.columnMax(0), testLab.columnMax(0)));
	svg.horizMarks(20);
	svg.vertMarks(20);
	double prevx = xmin;
	double prevy = 0.0;
	double step = (xmax - xmin) / 500.0;
	GVec x(1);
	for(x[0] = prevx; x[0] < xmax; x[0] += step)
	{
		double y = nn.forwardProp(x)[0];
		if(prevx != x[0])
			svg.line(prevx, prevy, x[0], y, 0.3);
		prevx = x[0];
		prevy = y;
	}
	for(size_t i = 0; i < trainLab.rows(); i++)
		svg.dot(trainFeat[i][0], trainLab[i][0], 0.4, 0xff000080);
	for(size_t i = 0; i < testLab.rows(); i++)
		svg.dot(testFeat[i][0], testLab[i][0], 0.4, 0xff800000);

	std::ofstream ofs;
	ofs.open(path);
	svg.print(ofs);
}

} // namespace GClasses
