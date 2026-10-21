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
#include <WClasses/WRand.h>
#include <WClasses/WImage.h>
#include <WClasses/WRayTrace.h>

class RayTraceController;
class RayTraceDialog;


class RayTraceView : public ViewBase
{
	friend class RayTraceController;
protected:
	RayTraceDialog* m_pDialog;
	WClasses::WImage* m_pImage;

public:
	RayTraceView(RayTraceController* pController);
	~RayTraceView() override;

	void onChar(char c) override;
	void onMouseDown(int nButton, int x, int y) override;
	void onMouseUp(int nButton, int x, int y) override;
	bool onMousePos(int x, int y) override;
	void SetImage(WClasses::WImage* pImage) { m_pImage = pImage; }

protected:
	void draw(int* pixels, int pitch) override;
};










class RayTraceController : public ControllerBase
{
protected:
	WClasses::WRayTraceScene* m_pScene;
	bool m_bRendering;
	WClasses::WRand m_prng;

public:
	RayTraceController();
	~RayTraceController() override;

	void RunModal();
	void OnSelectFile(const char* szFilename);
	void SetHighQuality(bool b);
};

#endif // __RAYTRACE_H__

