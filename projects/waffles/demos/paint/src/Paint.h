// -------------------------------------------------------------
// The contents of this file may be distributed under the CC0
// license (http://creativecommons.org/publicdomain/zero/1.0/).
// Of course you may also distribute it under a more restrictive
// license, such as any of the OSI-approved licenses
// (http://www.opensource.org/licenses).
// -------------------------------------------------------------

#ifndef __PAINT_H__
#define __PAINT_H__

#include "gui/Gui.h"
#include <WClasses/WImage.h>

class PaintTool;


class PaintController : public ControllerBase
{
protected:
	WClasses::WImage* m_pImage;
	WClasses::WImage* m_pSelection;

public:
	PaintController();
	~PaintController() override;

	WClasses::WImage* GetCanvas() { return m_pImage; }
	WClasses::WImage* GetSelectionMask() { return m_pSelection; }
	void RunModal();
	void RedrawCanvas();
	void CleanCanvas();
	void SetCurrentTool(PaintTool* pTool);
	void OnSelectTab(int i);
	void OpenFile();
	void SaveFile();
	void ZoomIn();
	void ZoomOut();
};

#endif // __PAINT_H__

