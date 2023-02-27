/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
    anonymous contributors,

  to the public domain (http://creativecommons.org/publicdomain/zero/1.0/).

  Note that some moral obligations still exist in the absence of legal ones.
  For example, it would still be dishonest to deliberately misrepresent the
  origin of a work. Although we impose no legal requirements to obtain a
  license, it is beseeming for those who build on the works of others to
  give back useful improvements, or pay it forward in their own field. If
  you would like to cite us, a published paper about Waffles can be found
  at http://jmlr.org/papers/volume12/gashler11a/gashler11a.pdf. If you find
  our code to be useful, the Waffles team would love to hear how you use it.
*/

#include <stdio.h>
#include <stdlib.h>
#include <WClasses/GApp.h>
#include <WClasses/GError.h>
#include <WClasses/GRand.h>
#include <WClasses/GTime.h>
#include <WClasses/GWidgets.h>
#include <WClasses/GThread.h>
#include "gui/Gui.h"
#include <exception>
#include <iostream>

using namespace GClasses;
using std::cerr;

class HelloModel;


class HelloController : public ControllerBase
{
protected:
	HelloModel* m_pModel;

public:
	HelloController();
	virtual ~HelloController();

	HelloModel* model() { return m_pModel; }
	void RunModal();
};


class HelloModel
{
protected:
	int m_x, m_y;
	GRand m_rand;

public:
	HelloModel()
	: m_x(100), m_y(250), m_rand(0)
	{
	}
	
	~HelloModel()
	{
	}

	void update()
	{
		m_x = std::max(0, std::min(200, m_x + (int)m_rand.next(15) - 7));
		m_y = std::max(0, std::min(500, m_y + (int)m_rand.next(15) - 7));
	}

	int x() { return m_x; }
	int y() { return m_y; }
};

const char* g_szMessage = "Hello GUI!";
#define TEXT_SIZE 9.0f

class HelloDialog : public GWidgetDialog
{
protected:
	HelloController* m_pController;
	GImage* m_pImage;
	GWidgetCanvas* m_pCanvas;
	GWidgetTextButton* m_pQuitButton;

public:
	HelloDialog(HelloController* pController, int w, int h)
	: GWidgetDialog(w, h, 0xff90d0f0) // a=ff, r=90, g=d0, b=f0
	{
		m_pController = pController;
		m_pImage = new GImage();
		m_pImage->setSize(800, 600);
		m_pCanvas = new GWidgetCanvas(this, 10, 30, m_pImage->width(), m_pImage->height(), m_pImage);
		m_pQuitButton = new GWidgetTextButton(this, 850, 300, 80, 24, "Quit");
	}

	virtual ~HelloDialog()
	{
		delete(m_pImage);
	}

	void update()
	{
		m_pImage->clear(0xff000000); // a=ff, r=00, g=00, b=00
		HelloModel* pModel = m_pController->model();
		m_pImage->text(g_szMessage, pModel->x(), pModel->y(), TEXT_SIZE, 0xffffff30); // a=ff, r=ff, g=ff, b=30
		m_pCanvas->setImage(m_pImage);
	}

	virtual void onReleaseTextButton(GWidgetTextButton* pButton)
	{
		if(pButton == m_pQuitButton)
			m_pController->quit();
		else
			GAssert(false); // unknown button
	}
};


// -------------------------------------------------------------------------------

class HelloView : public ViewBase
{
protected:
	HelloDialog* m_pDialog;

public:
	HelloView(HelloController* pController)
	: ViewBase()
	{
		m_pDialog = new HelloDialog(pController, m_screenRect.w, m_screenRect.h);
	}

	virtual ~HelloView()
	{
		delete(m_pDialog);
	}

	virtual void onChar(char c)
	{
		m_pDialog->handleChar(c);
	}

	virtual void onMouseDown(int nButton, int x, int y)
	{
		m_pDialog->pressButton(nButton, x - m_screenRect.x, y - m_screenRect.y);
	}

	virtual void onMouseUp(int nButton, int x, int y)
	{
		m_pDialog->releaseButton(nButton);
	}

	virtual bool onMousePos(int x, int y)
	{
		return m_pDialog->handleMousePos(x - m_screenRect.x, y - m_screenRect.y);
	}

protected:
	virtual void draw(int* pixels, int pitch)
	{
		m_pDialog->update();

		// Clear the screen
		SDL_SetRenderDrawColor(sdlRenderer,0,0,0,0);
		SDL_Rect windowRect{m_screenRect.x,m_screenRect.y,m_screenRect.w,m_screenRect.h};
		SDL_RenderFillRect(sdlRenderer,&windowRect);
		Uint32 format; // SDL_PixelFormatEnum
		SDL_QueryTexture(sdlTexture,&format, NULL,NULL,NULL);
		// Draw the dialog
		blitImage(pixels,pitch,format, m_screenRect.x, m_screenRect.y, m_pDialog->image());
	}
};


// -------------------------------------------------------------------------------


HelloController::HelloController()
: ControllerBase()
{
	m_pModel = new HelloModel();
	m_pView = new HelloView(this);
}

HelloController::~HelloController()
{
	delete(m_pView);
	delete(m_pModel);
}

void HelloController::RunModal()
{
	double timeOld = GTime::seconds();
	double time;
	m_pView->update();
	while(m_bKeepRunning)
	{
		time = GTime::seconds();
		if(!handleEvents(time - timeOld))
			GThread::sleep(50); // slow things down
		m_pModel->update();
		m_pView->update();
		timeOld = time;
	}
}





int main(int argc, char *argv[])
{
	int nRet = 0;
	try
	{
		HelloController c;
		c.RunModal();
	}
	catch(const std::exception& e)
	{
		cerr << e.what() << "\n";
		nRet = 1;
	}

	return nRet;
}

