// -------------------------------------------------------------
// The contents of this file may be distributed under the CC0
// license (http://creativecommons.org/publicdomain/zero/1.0/).
// Of course you may also distribute it under a more restrictive
// license, such as any of the OSI-approved licenses
// (http://www.opensource.org/licenses).
// -------------------------------------------------------------

#ifndef __RAYTRACE_H__
#define __RAYTRACE_H__

#include "gui/Gui.h"
#include <WClasses/GRand.h>
#include <WClasses/GImage.h>
#include <WClasses/GRayTrace.h>

class RayTraceController;
class RayTraceDialog;


class RayTraceView : public ViewBase
{
	friend class RayTraceController;
protected:
	RayTraceDialog* m_pDialog;
	GClasses::GImage* m_pImage;

public:
	RayTraceView(RayTraceController* pController);
	~RayTraceView() override;

	void onChar(char c) override;
	void onMouseDown(int nButton, int x, int y) override;
	void onMouseUp(int nButton, int x, int y) override;
	bool onMousePos(int x, int y) override;
	void SetImage(GClasses::GImage* pImage) { m_pImage = pImage; }

protected:
	void draw(int* pixels, int pitch) override;
};










class RayTraceController : public ControllerBase
{
protected:
	GClasses::GRayTraceScene* m_pScene;
	bool m_bRendering;
	GClasses::GRand m_prng;

public:
	RayTraceController();
	~RayTraceController() override;

	void RunModal();
	void OnSelectFile(const char* szFilename);
	void SetHighQuality(bool b);
};

#endif // __RAYTRACE_H__

