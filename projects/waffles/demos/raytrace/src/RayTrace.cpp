// -------------------------------------------------------------
// The contents of this file may be distributed under the CC0
// license (http://creativecommons.org/publicdomain/zero/1.0/).
// Of course you may also distribute it under a more restrictive
// license, such as any of the OSI-approved licenses
// (http://www.opensource.org/licenses).
// -------------------------------------------------------------

#include "RayTrace.h"
#ifdef WINDOWS
#else
#include <unistd.h>
#endif
#include <WClasses/WTime.h>
#include <WClasses/WError.h>
#include <WClasses/WRayTrace.h>
#include <WClasses/WBits.h>
#include <WClasses/WThread.h>
#include "WRibParser.h"

using namespace WClasses;

class RayTraceDialog : public WWidgetDialog
{
	friend class RayTraceController;
protected:
	RayTraceController* m_pController;
	WWidgetFileSystemBrowser* m_pFileSystemBrowser;
	WWidgetCheckBox* m_pCheckBox;

public:
	RayTraceDialog(RayTraceController* pController, int w, int h)
	: WWidgetDialog(w, h, 0xff884466)
	{
		m_pController = pController;

		m_pFileSystemBrowser = new WWidgetFileSystemBrowser(this, 100, 400, 800, 280, ".rib");
		m_pCheckBox = new WWidgetCheckBox(this, 5, 400, 20, 20);
		new WWidgetTextLabel(this, 26, 405, 70, 12, "High Quality", 0xff002200);
	}

	~RayTraceDialog() override
	{
	}

	void onReleaseTextButton(WWidgetTextButton* pButton) override
	{
	}

	void onSelectFilename(WWidgetFileSystemBrowser* pBrowser, const char* szFilename) override
	{
		m_pController->OnSelectFile(szFilename);
	}

	void onChangeCheckBox(WWidgetCheckBox* pCheckBox) override
	{
		m_pController->SetHighQuality(pCheckBox->isChecked());
	}
};


// ----------------------------------------------------------------------------

RayTraceView::RayTraceView(RayTraceController* pController)
: ViewBase("Ray Trace")
{
	m_pDialog = new RayTraceDialog(pController, m_screenRect.w, m_screenRect.h);
	m_pImage = nullptr;
}

RayTraceView::~RayTraceView()
{
	delete(m_pDialog);
}

/*virtual*/ void RayTraceView::draw(int* pixels, int pitch)
{
	WImage* pCanvas = m_pDialog->image();
	if(m_pImage)
	{
		WRect r(0, 0, m_pImage->width(), m_pImage->height());
		pCanvas->blit(0, 0, m_pImage, &r);
	}
	Uint32 format; // SDL_PixelFormatEnum
	SDL_QueryTexture(sdlTexture, &format, nullptr, nullptr, nullptr);
	blitImage(pixels, pitch, format, m_screenRect.x, m_screenRect.y, pCanvas);
}

void RayTraceView::onChar(char c)
{
	m_pDialog->handleChar(c);
}

void RayTraceView::onMouseDown(int nButton, int x, int y)
{
	m_pDialog->pressButton(nButton, x - m_screenRect.x, y - m_screenRect.y);
}

void RayTraceView::onMouseUp(int nButton, int x, int y)
{
	m_pDialog->releaseButton(nButton);
}

bool RayTraceView::onMousePos(int x, int y)
{
	return m_pDialog->handleMousePos(x - m_screenRect.x, y - m_screenRect.y);
}


// -------------------------------------------------------------------------------

RayTraceController::RayTraceController()
: ControllerBase(), m_prng(0)
{
	m_pView = new RayTraceView(this);
	m_bRendering = false;
	m_pScene = nullptr;
}

RayTraceController::~RayTraceController()
{
	delete(m_pView);
}

void RayTraceController::SetHighQuality(bool b)
{
	if(!m_pScene)
		return;
	if(b)
		m_pScene->setRenderMode(WRayTraceScene::QUALITY_RAY_TRACE);
	else
		m_pScene->setRenderMode(WRayTraceScene::FAST_RAY_TRACE);
}

void RayTraceController::OnSelectFile(const char* szFilename)
{
	m_pScene = RibParser::LoadScene(szFilename, &m_prng);
	if(!m_pScene)
	{
		printf("Failed to create scene\n");
		m_bKeepRunning = false;
		return;
	}
	SetHighQuality(((RayTraceView*)m_pView)->m_pDialog->m_pCheckBox->isChecked());
	m_pScene->renderBegin();
	m_pScene->drawWireFrame();
	WImage* pImage = m_pScene->image();
	//m_pScene->RenderSinglePixel(pImage->GetWidth() / 2, pImage->GetHeight() / 2);
	((RayTraceView*)m_pView)->SetImage(pImage);
	m_bRendering = true;
}

void RayTraceController::RunModal()
{
	double timeOld = WTime::seconds();
	double time;
	double timeUpdate = 0;
	m_pView->update();
	while(m_bKeepRunning)
	{
		time = WTime::seconds();
		if(handleEvents(time - timeOld)) // HandleEvents returns true if it thinks the view needs to be updated
		{
			m_pView->update();
			timeUpdate = time;
		}
		else if(m_bRendering)
		{
			m_bRendering = m_pScene->renderLine();
			if(!m_bRendering || time - timeUpdate > .25)
			{
				m_pView->update();
				timeUpdate = time;
			}
		}
		else
		{
			WThread::sleep(10);
		}
		timeOld = time;
	}
}

