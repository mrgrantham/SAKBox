#ifndef LOADER_H
#define LOADER_H

#include <WClasses/WMatrix.h>
using namespace WClasses;

class Loader
{
public:
	static void loadStockData(WMatrix& trainFeat, WMatrix& trainLab, WMatrix& testFeat, WMatrix& testLab);
	static void loadPrecipitationData(WMatrix& trainFeat, WMatrix& trainLab, WMatrix& testFeat, WMatrix& testLab);
	static void loadTemperatureData(WMatrix& trainFeat, WMatrix& trainLab, WMatrix& testFeat, WMatrix& testLab);
	static void loadLaborData(WMatrix& trainFeat, WMatrix& trainLab, WMatrix& testFeat, WMatrix& testLab);
	static void loadLaborData2(WMatrix& trainFeat, WMatrix& trainLab, WMatrix& testFeat, WMatrix& testLab);
	static void loadMackeyGlassData(WMatrix& trainFeat, WMatrix& trainLab, WMatrix& testFeat, WMatrix& testLab);
	static void loadSunSpotData(WMatrix& trainFeat, WMatrix& trainLab, WMatrix& testFeat, WMatrix& testLab);
	static void loadToyData(WMatrix& trainFeat, WMatrix& trainLab, WMatrix& testFeat, WMatrix& testLab);
	static void loadToyData2(WMatrix& trainFeat, WMatrix& trainLab, WMatrix& testFeat, WMatrix& testLab);

	static void loadTest01(WMatrix& trainFeat, WMatrix& trainLab, WMatrix& testFeat, WMatrix& testLab);
	static void loadTest02(WMatrix& trainFeat, WMatrix& trainLab, WMatrix& testFeat, WMatrix& testLab);
	static void loadTest04(WMatrix& trainFeat, WMatrix& trainLab, WMatrix& testFeat, WMatrix& testLab);

	static void loadNASDAQ(WMatrix& trainFeat, WMatrix& trainLab, WMatrix& testFeat, WMatrix& testLab);
	static void loadAirPassengerData(WMatrix& trainFeat, WMatrix& trainLab, WMatrix& testFeat, WMatrix& testLab);
	static void loadOzoneData(WMatrix& trainFeat, WMatrix& trainLab, WMatrix& testFeat, WMatrix& testLab);
};

#endif
