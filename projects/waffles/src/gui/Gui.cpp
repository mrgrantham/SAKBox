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

#include "Gui.h"
#include <WClasses/GSDL.h>
#include <WClasses/GImage.h>
#include <WClasses/GHolders.h>
#include <WClasses/GTime.h>
#include <WClasses/GFile.h>
#include <WClasses/GThread.h>
#include <WClasses/GBits.h>
#include <WClasses/GString.h>
#include <WClasses/GApp.h>
#include <stdarg.h>
#include <math.h>
#ifdef WINDOWS
#	include <windows.h>
#endif
#include <string>
#include <iostream>
#include <fmt/core.h>

using namespace GClasses;
using std::cout;
using std::string;


#ifndef NOGUI
SDL_Window* ViewBase::window_ = nullptr;

ViewBase::ViewBase()
{
	const int windowWidth = 1010;
	const int windowHeight = 690;
	if(!window_)
		window_ = makeScreen(windowWidth, windowHeight);
	m_pScreen = window_;
	sdlRenderer = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
	sdlTexture = SDL_CreateTexture(sdlRenderer,
										SDL_PIXELFORMAT_BGRA8888,
										SDL_TEXTUREACCESS_STREAMING,
										windowWidth, windowHeight);
	m_screenRect.x = 0;
	m_screenRect.y = 0;
	m_screenRect.w = windowWidth;
	m_screenRect.h = windowHeight;
}

ViewBase::~ViewBase()
{
	SDL_DestroyTexture(sdlTexture);
	SDL_DestroyRenderer(sdlRenderer);
	m_pScreen = nullptr;
	SDL_DestroyWindow(window_);
}

SDL_Window* ViewBase::makeScreen(int x, int y)
{
SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
SDL_Window *sdlWindow = SDL_CreateWindow("Car On Hill",
                             SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED,
                             x, y,
                             window_flags);
if(!sdlWindow)
	{
		// SDL_GetError();
		throw "failed to create SDL screen";
	}
	return sdlWindow;
}

void ViewBase::captureScreen(GImage* pImage)
{
	Uint32 format; // SDL_PixelFormatEnum
	SDL_QueryTexture(sdlTexture,&format, NULL,NULL,NULL);
	pImage->setSize(m_screenRect.w, m_screenRect.h);

	int *pixels = nullptr;
	int pitch = 0;
	const SDL_Rect* SDL_LOCK_ENTIRE_TEXTURE = nullptr;
	if ( SDL_LockTexture(sdlTexture,SDL_LOCK_ENTIRE_TEXTURE,(void **)&pixels,&pitch) < 0 )
	{
		GAssert(false); // SDL_GetError(); // failed to lock the surface
		return;
	}

	if(SDL_BYTESPERPIXEL(format) == 4)
	{
		unsigned int* pRGB = pImage->pixels();
		int y;
		for(y = 0; y < m_screenRect.h; y++)
		{
			Uint32* pPix = getPixMem32(pixels,pitch, m_screenRect.x, m_screenRect.y + y);
			memcpy(&pRGB[y * pImage->width()], pPix, pImage->width() * sizeof(unsigned int));
		}
	}
	else
	{
		unsigned int* pRGB = pImage->pixels();
		int x, y;
		Uint8 r, g, b;
		for(y = 0; y < m_screenRect.h; y++)
		{
			Uint16* pPix = getPixMem16(pixels,pitch, m_screenRect.x, m_screenRect.y + y);
			for(x = 0; x < m_screenRect.w; x++)
			{
				SDL_GetRGB(*pPix, window_surface->format, &r, &g, &b);
				*pRGB = gRGB(r, g, b);
				pPix++;
				pRGB++;
			}
		}
	}
}

/*static*/ void ViewBase::blitImage(int* pixels,int pitch, Uint32 format, int x, int y, GImage* pImage)
{
	if(SDL_BYTESPERPIXEL(format) == 4)
	{		
		// 32 bits per pixel
		unsigned int* pRGB = pImage->pixels();
		const int width = pImage->width();
		const int height = pImage->height();
		int yy;
		Uint32* pPix;
		for(yy = 0; yy < height; yy++)
		{
			pPix = getPixMem32(pixels,pitch, x, y);
#ifdef __APPLE__
			unsigned int* pRaw = &pRGB[yy * width];
			for(size_t i = 0; i < width; i++)
				*(pPix++) = ntohl(*(pRaw++));
#else
			memcpy(pPix, &pRGB[yy * w], w * sizeof(unsigned int));
#endif
			y++;
		}
	}
	else
	{
		// 16 bits per pixel
		GAssert(SDL_BYTESPERPIXEL(format) == 2); // Only 16 and 32 bit video modes are supported
		// fmt::println("16 bit pic blitImage");
		int w = pImage->width();
		int h = pImage->height();
		int xx, yy;
		unsigned int colIn;
		Uint16* pPix;
		for(yy = 0; yy < h; yy++)
		{
			pPix = (Uint16*)pixels + y * pitch / 2 + x;
			for(xx = 0; xx < w; xx++)
			{
				auto sdlFormat = SDL_AllocFormat(format);
				colIn = pImage->pixel(xx, yy);
				*pPix = (Uint16)SDL_MapRGB(sdlFormat, gRed(colIn), gGreen(colIn), gBlue(colIn));
				pPix++;
			}
			y++;
		}
	}
}

/*static*/ void ViewBase::stretchClipAndBlitImage(int* pixels,int pitch, Uint32 format, GRect* pDestRect, GRect* pClipRect, GImage* pImage)
{
	float fSourceDX =  (float)(pImage->width() - 1) / (float)(pDestRect->w - 1);
	GAssert((int)((pDestRect->w - 1) * fSourceDX) < (int)pImage->width()); // Extends past source image width
	float fSourceDY = (float)(pImage->height() - 1) / (float)(pDestRect->h - 1);
	GAssert((int)((pDestRect->h - 1) * fSourceDY) < (int)pImage->height()); // Extends past source image height
	float fSourceX = 0;
	float fSourceY = 0;
	int xStart = pDestRect->x;
	int xEnd = pDestRect->x + pDestRect->w;
	int yStart = pDestRect->y;
	int yEnd = pDestRect->y + pDestRect->h;

	// Clip
	if(pClipRect->x > xStart)
	{
		fSourceX = (pClipRect->x - xStart) * fSourceDX;
		xStart = pClipRect->x;
	}
	if(pClipRect->y > yStart)
	{
		fSourceY = (pClipRect->y - yStart) * fSourceDY;
		yStart = pClipRect->y;
	}
	if(xEnd > pClipRect->x + pClipRect->w)
		xEnd = pClipRect->x + pClipRect->w;
	if(yEnd > pClipRect->y + pClipRect->h)
		yEnd = pClipRect->y + pClipRect->h;

	// Blit
	int x, y;
	float fSX;
	if(SDL_BYTESPERPIXEL(format) == 4)
	{
		// 32 bits per pixel
		Uint32* pPix;
		for(y = yStart; y < yEnd; y++)
		{
			fSX = fSourceX;
			pPix = getPixMem32(pixels,pitch, xStart, y);
			for(x = xStart; x < xEnd; x++)
			{
				*pPix = pImage->pixel((int)fSX, (int)fSourceY);
				pPix++;
				fSX += fSourceDX;
			}
			fSourceY += fSourceDY;
		}
	}
	else
	{
		// 16 bits per pixel
		GAssert(pScreen->format->BytesPerPixel == 2); // Only 16 and 32 bit video modes are supported
		unsigned int colIn;
		Uint16* pPix;
		for(y = yStart; y < yEnd; y++)
		{
			fSX = fSourceX;
			pPix = getPixMem16(pixels,pitch, xStart, y);
			for(x = xStart; x < xEnd; x++)
			{
				colIn = pImage->pixel((int)fSX, (int)fSourceY);
				auto sdlFormat = SDL_AllocFormat(format);
				*pPix = (Uint16)SDL_MapRGB(sdlFormat, gRed(colIn), gGreen(colIn), gBlue(colIn));
				pPix++;
				fSX += fSourceDX;
			}
			fSourceY += fSourceDY;
		}
	}
}

void ViewBase::drawDot(int *pixels,int pitch,Uint32 format, int x, int y, unsigned int col, int nSize)
{
	int nWhiteAmount;
	int nColorAmount;
	int xx, yy;
	int nYMax = std::min(y + nSize, m_screenRect.y + m_screenRect.h);
	int nXMax = std::min(x + nSize, m_screenRect.x + m_screenRect.w);
	int nSizeSquared = nSize * nSize;
	int nDoubleSizeSquared = nSizeSquared + nSizeSquared;
	if(SDL_BYTESPERPIXEL(format) == 4)
	{
		// 32 bits per pixel
		for(yy = std::max(m_screenRect.y, y - nSize); yy < nYMax; yy++)
		{
			for(xx = std::max(m_screenRect.x, x - nSize); xx < nXMax; xx++)
			{
				nWhiteAmount = (x - xx) * (x - xx) + (y - yy) * (y - yy);
				if(nWhiteAmount > nSizeSquared)
					continue;
				nWhiteAmount += nWhiteAmount;
				nColorAmount = nDoubleSizeSquared - nWhiteAmount;
				*getPixMem32(pixels,pitch, xx, yy) =	gARGB(
						0xff,
						(nColorAmount * gRed(col)/* + nWhiteAmount * 0xff*/) / nDoubleSizeSquared,
						(nColorAmount * gGreen(col)/* + nWhiteAmount * 0xff*/) / nDoubleSizeSquared,
						(nColorAmount * gBlue(col)/* + nWhiteAmount * 0xff*/) / nDoubleSizeSquared
					);
			}
		}
	}
	else
	{
		// 16 bits per pixel
		GAssert(SDL_BYTESPERPIXEL(format) == 2); // Only 16 and 32 bit video modes are supported
		for(yy = std::max(m_screenRect.y, y - nSize); yy < nYMax; yy++)
		{
			for(xx = std::max(m_screenRect.x, x - nSize); xx < nXMax; xx++)
			{
				nWhiteAmount = (x - xx) * (x - xx) + (y - yy) * (y - yy);
				if(nWhiteAmount > nSizeSquared)
					continue;
				nWhiteAmount += nWhiteAmount;
				nColorAmount = nDoubleSizeSquared - nWhiteAmount;
				auto sdlFormat = SDL_AllocFormat(format);
				*getPixMem16(pixels,pitch, xx, yy) =
					(Uint16)SDL_MapRGB(sdlFormat,
						(nColorAmount * gRed(col)/* + nWhiteAmount * 0xff*/) / nDoubleSizeSquared,
						(nColorAmount * gGreen(col)/* + nWhiteAmount * 0xff*/) / nDoubleSizeSquared,
						(nColorAmount * gBlue(col)/* + nWhiteAmount * 0xff*/) / nDoubleSizeSquared
					);
			}
		}
	}
}

void ViewBase::update()
{
	// Lock the screen for direct access to the pixels
	// SDL_Surface *pScreen = window_surface;
	SDL_Texture *currentTexture = sdlTexture;
	// if ( SDL_MUSTLOCK(pScreen) )
	// {
		int *pixels = nullptr;
		int pitch = 0;
		const SDL_Rect* SDL_LOCK_ENTIRE_TEXTURE = nullptr;
		if ( SDL_LockTexture(currentTexture,SDL_LOCK_ENTIRE_TEXTURE,(void **)&pixels,&pitch) < 0 )
		{
			fmt::println("ERROR");
			GAssert(false); // SDL_GetError(); // failed to lock the surface
			return;
		}
	// }

	// fmt::println("start draw");	
	// Draw the screen
	draw(pixels,pitch);
	// fmt::println("after draw");	

	// Unlock the screen
	// if ( SDL_MUSTLOCK(pScreen) )
		SDL_UnlockTexture(currentTexture);

	SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
	// Update the whole screen
	 SDL_RenderPresent(sdlRenderer);
}


// -------------------------------------------------------------------

#define KEY_REPEAT_DELAY .3
#define KEY_REPEAT_RATE .01
#endif // !NOGUI

ControllerBase::ControllerBase()
{
	m_bKeepRunning = true;

#ifndef NOGUI
	m_pView = NULL;

	// Init the keyboard
	int n;
	for(n = 0; n < SDL_NUM_SCANCODES; n++)
		m_keyboard[n] = 0;

	m_mouse[1] = 0;
	m_mouse[2] = 0;
	m_mouse[3] = 0;
	m_mouseX = 0;
	m_mouseY = 0;
	m_eKeyState = Normal;
	m_lastPressedKey = SDLK_UNKNOWN;
	m_dKeyRepeatTimer = 0;
#endif
}

ControllerBase::~ControllerBase()
{
}

#ifndef NOGUI
bool ControllerBase::handleEvents(double dTimeDelta)
{
	if(!m_pView)
		return false;
	// Check for events
	bool bRet = false;
	SDL_Event event;
	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
			case SDL_KEYDOWN: {
				m_keyboard[event.key.keysym.sym] = 1;
				handleKeyPress(event.key.keysym.sym, (SDL_Keymod)event.key.keysym.mod);
				bRet = true;
				break;
			}

			case SDL_KEYUP:
				m_keyboard[event.key.keysym.sym] = 0;
				m_eKeyState = Normal;
				break;

			case SDL_MOUSEBUTTONDOWN:
				m_mouse[event.button.button] = 1;
				m_mouseX = event.button.x;
				m_mouseY = event.button.y;
				onMouseDown(event.button.button, m_mouseX, m_mouseY);
				bRet = true;
				break;

			case SDL_MOUSEBUTTONUP:
				m_mouse[event.button.button] = 0;
				onMouseUp(event.button.button, m_mouseX, m_mouseY);
				bRet = true;
				break;

			case SDL_MOUSEMOTION:
				m_mouseX = event.motion.x;
				m_mouseY = event.motion.y;
				break;

			case SDL_QUIT:
				m_bKeepRunning = false;
				break;
			default:
				break;
		}
	}
	if(bRet)
	{
	}
	else if(m_keyboard[m_lastPressedKey])
	{
		switch(m_eKeyState)
		{
			case Normal:
				m_eKeyState = Holding;
				m_dKeyRepeatTimer = 0;
				return false; // don't bother updating the display
			case Holding:
				m_dKeyRepeatTimer += dTimeDelta;
				if(m_dKeyRepeatTimer >= KEY_REPEAT_DELAY)
				{
					m_dKeyRepeatTimer = 0;
					m_eKeyState = Repeating;
				}
				return false; // don't bother updating the display
			case Repeating:
				m_dKeyRepeatTimer += dTimeDelta;
				if(m_dKeyRepeatTimer > KEY_REPEAT_RATE)
				{
					m_dKeyRepeatTimer -= KEY_REPEAT_RATE;
					handleKeyPress(m_lastPressedKey, (SDL_Keymod)event.key.keysym.mod);
				}
				break;
			default:
				GAssert(false); // unexpected case
		}
	}else if(onMousePos(m_mouseX, m_mouseY))
	{
	}
	else
	{
		m_eKeyState = Normal;
		return false; // false = don't bother updating the view
	}
	return true; // true = need to update the view
}

void ControllerBase::handleKeyPress(SDL_Keycode key, SDL_Keymod mod)
{
	char cKey = GSDL::filterKey(key);
	if(cKey == 0)
	{
		onSpecialKey(key);
		return;
	}
	else
	{
		if(cKey == 27)
		{
			m_bKeepRunning = false;
			return;
		}

		// Capitalize if shift is down
		if(mod & KMOD_SHIFT)
			cKey = GSDL::shiftKey(cKey);

		m_pView->onChar(cKey);
		m_lastPressedKey = key;
	}
}

void ControllerBase::mousePos(int* pX, int* pY)
{
	*pX = m_mouseX;
	*pY = m_mouseY;
}

// -----------------------------------------------------------------------------------------------

class PopupView : public ViewBase
{
protected:
	GImage* m_pBackgroundImage;
	GWidgetDialog* m_pDialog;
	int m_nLeft, m_nTop, m_x, m_y;

public:
	PopupView(GWidgetDialog* pDialog)
	: ViewBase()
	{
		m_nLeft = 0;
		m_nTop = 0;
		m_pBackgroundImage = new GImage();
		captureScreen(m_pBackgroundImage);
		m_pBackgroundImage->contrastAndBrightness(.5, -64);
		m_pDialog = pDialog;
		m_x = (m_pBackgroundImage->width() - m_pDialog->rect()->w) / 2;
		m_y = (m_pBackgroundImage->height() - m_pDialog->rect()->h) / 2;
	}

	virtual ~PopupView()
	{
		delete(m_pBackgroundImage);
	}

	virtual void onMouseDown(int button, int x, int y)
	{
		m_pDialog->pressButton(button, x - (m_x + m_screenRect.x), y - (m_y + m_screenRect.y));
	}

	virtual void onMouseUp(int button, int x, int y)
	{
		m_pDialog->releaseButton(button);
	}

	virtual bool onMousePos(int x, int y)
	{
		x -= (m_x + m_screenRect.x);
		y -= (m_y + m_screenRect.y);
		return m_pDialog->handleMousePos(x, y);
	}

	virtual void onChar(char c)
	{
		m_pDialog->handleChar(c);
	}

	virtual void onSpecialKey(int key)
	{
		m_pDialog->handleSpecialKey(key);
	}

protected:
	virtual void draw(int* pixels, int pitch)
	{
		fmt::println("draw");
		GImage* pDialogImage = m_pDialog->image();
		m_pBackgroundImage->blit(m_x, m_y, pDialogImage, m_pDialog->rect());
		Uint32 format; // SDL_PixelFormatEnum
		SDL_QueryTexture(sdlTexture,&format, NULL,NULL,NULL);
		blitImage(pixels,pitch,format, m_nLeft, m_nTop, m_pBackgroundImage);
	}
};


class PopupController : public ControllerBase
{
protected:
	bool* m_pKeepRunning;

public:
	PopupController(bool* pKeepRunning, GWidgetDialog* pDialog)
	: ControllerBase()
	{
		*pKeepRunning = true;
		m_pKeepRunning = pKeepRunning;
		m_pView = new PopupView(pDialog);
	}

	virtual ~PopupController()
	{
		delete(m_pView);
	}

	void runModal()
	{
		double timeOld = GTime::seconds();
		double time;
		m_pView->update();
		while(*m_pKeepRunning && m_bKeepRunning)
		{
			time = GTime::seconds();
			if(handleEvents(time - timeOld)) // HandleEvents returns true if it thinks the view needs to be updated
				m_pView->update();
			// else
			// 	GThread::sleep(10);
			timeOld = time;
		}
	}
};


void RunPopup(GWidgetDialog* pDialog)
{
	PopupController controller(pDialog->runningFlag(), pDialog);
	controller.runModal();
}
#endif // !NOGUI

// -----------------------------------------------------------------------------------------------
#ifdef WINDOWS
// Iterate the top-level windows. Encapsulates ::EnumWindows.
class CWindowIterator {
protected:
   HWND* m_hwnds;          // array of hwnds for this PID
   DWORD m_nAlloc;         // size of array
   DWORD m_count;          // number of HWNDs found
   DWORD m_current;        // current HWND
   static BOOL CALLBACK EnumProc(HWND hwnd, LPARAM lp);
   // virtual enumerator
   virtual BOOL OnEnumProc(HWND hwnd);
   // override to filter different kinds of windows
   virtual BOOL OnWindow(HWND hwnd) {return TRUE;}
public:
   CWindowIterator(DWORD nAlloc=1024);
   ~CWindowIterator();

   DWORD GetCount() { return m_count; }
   HWND First();
   HWND Next() {
      return m_hwnds && m_current < m_count ? m_hwnds[m_current++] : NULL;
   }
};

// Iterate the top-level windows in a process.
class CMainWindowIterator : public CWindowIterator  {
protected:
   DWORD m_pid;                     // process id
   virtual BOOL OnWindow(HWND hwnd);
public:
   CMainWindowIterator(DWORD pid, DWORD nAlloc=1024);
   ~CMainWindowIterator();
};

CWindowIterator::CWindowIterator(DWORD nAlloc)
{
   m_current = m_count = 0;
   m_hwnds = new HWND [nAlloc];
   m_nAlloc = nAlloc;
}

CWindowIterator::~CWindowIterator()
{
   delete [] m_hwnds;
}

HWND CWindowIterator::First()
{
   ::EnumWindows(EnumProc, (LPARAM)this);
   m_current = 0;
   return Next();
}

// Static proc passes to virtual fn.
BOOL CALLBACK CWindowIterator::EnumProc(HWND hwnd, LPARAM lp)
{
   return ((CWindowIterator*)lp)->OnEnumProc(hwnd);
}

// Virtual proc: add HWND to array if OnWindow says OK
BOOL CWindowIterator::OnEnumProc(HWND hwnd)
{
   if (OnWindow(hwnd)) {
      if (m_count < m_nAlloc)
         m_hwnds[m_count++] = hwnd;
   }
   return TRUE; // keep looking
}

CMainWindowIterator::CMainWindowIterator(DWORD pid, DWORD nAlloc)
   : CWindowIterator(nAlloc)
{
   m_pid = pid;
}

CMainWindowIterator::~CMainWindowIterator()
{
}

// virtual override: is this window a main window of my process?
BOOL CMainWindowIterator::OnWindow(HWND hwnd)
{
   if (GetWindowLong(hwnd,GWL_STYLE) & WS_VISIBLE) {
      DWORD pidwin;
      GetWindowThreadProcessId(hwnd, &pidwin);
      if (pidwin==m_pid)
         return TRUE;
   }
   return FALSE;
}

#endif // WINDOWS

void OpenFile(const char* szFilename)
{
	cout << "Opening file \"" << szFilename << "\"\n";
#ifdef WINDOWS
	SHELLEXECUTEINFO sei;
	memset(&sei, '\0', sizeof(SHELLEXECUTEINFO));
	sei.cbSize = sizeof(SHELLEXECUTEINFO);
	sei.fMask = SEE_MASK_NOCLOSEPROCESS/* | SEE_MASK_NOZONECHECKS*/;
	sei.hwnd = NULL;
	sei.lpVerb = NULL;
	sei.lpFile = szFilename;
	sei.lpParameters = NULL;
	sei.lpDirectory = NULL;
	sei.nShow = SW_SHOW;
	ShellExecuteEx(&sei);
	CMainWindowIterator itw((DWORD)sei.hProcess/*pid*/);
	SetForegroundWindow(itw.First());
	//ShellExecute(NULL, NULL, szFilename, NULL, NULL, SW_SHOW);
#else
#ifdef __APPLE__
	// Mac
	GTEMPBUF(char, pBuf, 32 + strlen(szFilename));
	strcpy(pBuf, "open ");
	strcat(pBuf, szFilename);
	strcat(pBuf, " &");
	system(pBuf);
#else // __APPLE__
	GTEMPBUF(char, pBuf, 32 + strlen(szFilename));

	// Gnome
	strcpy(pBuf, "gnome-open ");
	strcat(pBuf, szFilename);
	if(system(pBuf) != 0)
	{
		// KDE
		//strcpy(pBuf, "kfmclient exec ");
		strcpy(pBuf, "konqueror ");
		strcat(pBuf, szFilename);
		strcat(pBuf, " &");
		if(system(pBuf) != 0)
		{
			cout << "Failed to open " << szFilename << ". Please open it manually.\n";
			cout.flush();
		}
	}
#endif // !DARWIN
#endif // !WINDOWS
}


void OpenAppFile(const char* szRelativeFilename)
{
	char szFilename[256];
	int len = GApp::appPath(szFilename, 256, true);
	safe_strcpy(szFilename + len, szRelativeFilename, 256 - len);
	GFile::condensePath(szFilename);
	OpenFile(szFilename);
}

// -----------------------------------------------------------------------------------------------

#ifndef NOGUI

int MessageBoxDialog_CountLines(const char* szMessage)
{
	GStringChopper sc(szMessage, 40, 80, true);
	int nCount;
	for(nCount = 0; sc.next(); nCount++)
	{
	}
	return nCount;
}

MessageBoxDialog::MessageBoxDialog(const char* szMessage)
: GWidgetDialog(500, 100 + MessageBoxDialog_CountLines(szMessage) * 16, 0xffccbbaa)
{
	GStringChopper sc(szMessage, 40, 80, true);
	int x, y, w;
	for(y = 40; true; y += 16)
	{
		const char* szLine = sc.next();
		if(!szLine)
			break;
		w = GImage::measureTextWidth(szLine, 1.0f);
		x = (m_rect.w - 40 - w) / 2 + 20;
		new GWidgetTextLabel(this, x, y, m_rect.w - x, 16, szLine, 0xff442200);
	}
	m_pOK = new GWidgetTextButton(this, 200, m_rect.h - 35, 100, 24, "OK");
	new GWidgetGroupBox(this, 0, 0, m_rect.w, m_rect.h, 0xffddccbb, 0xffbbaa99);
}

// virtual
MessageBoxDialog::~MessageBoxDialog()
{
}

// virtual
void MessageBoxDialog::onReleaseTextButton(GWidgetTextButton* pButton)
{
	if(pButton == m_pOK)
		close();
	else
		GAssert(false); // unrecognized button
}

// -----------------------------------------------------------------------------------------------


GetStringDialog::GetStringDialog(const char* szMessage)
: GWidgetDialog(600, 90, 0xffccbbaa)
{
	new GWidgetTextLabel(this, 10, 5, m_rect.w - 20, 20, szMessage, 0xff442200);
	m_pTextBox = new GWidgetTextBox(this, 10, 30, m_rect.w - 20, 20);
	m_pOK = new GWidgetTextButton(this, 150, m_rect.h - 30, 100, 24, "OK");
	m_pCancel = new GWidgetTextButton(this, 350, m_rect.h - 30, 100, 24, "Cancel");
}

// virtual
GetStringDialog::~GetStringDialog()
{
}

// virtual
void GetStringDialog::onTextBoxPressEnter(GWidgetTextBox* pTextBox)
{
	close();
}

// virtual
void GetStringDialog::onReleaseTextButton(GWidgetTextButton* pButton)
{
	if(pButton == m_pOK)
		close();
	else if(pButton == m_pCancel)
	{
		m_pTextBox->setText("");
		close();
	}
	else
		GAssert(false); // unrecognized text button
}

// -----------------------------------------------------------------------------------------------


GetOpenFilenameDialog::GetOpenFilenameDialog(const char* szMessage, const char* szExt)
: GWidgetDialog(700, 500, 0xffcc9966)
{
	new GWidgetTextLabel(this, 10, 5, 300, 20, szMessage, 0xff442200);
	m_pFileBrowser = new GWidgetFileSystemBrowser(this, 10, 30, m_rect.w - 20, m_rect.h - 65, szExt);
	m_pCancel = new GWidgetTextButton(this, (m_rect.w - 100) / 2, m_rect.h - 30, 100, 24, "Cancel");
	m_szFilename = NULL;
}

// virtual
GetOpenFilenameDialog::~GetOpenFilenameDialog()
{
}

// virtual
void GetOpenFilenameDialog::onSelectFilename(GWidgetFileSystemBrowser* pBrowser, const char* szFilename)
{
	delete[] m_szFilename;
	m_szFilename = new char[strlen(szFilename) + 1];
	strcpy(m_szFilename, szFilename);
#ifdef WINDOWS
	int i;
	for(i = 0; m_szFilename[i] != '\0'; i++)
	{
		if(m_szFilename[i] == '\\')
			m_szFilename[i] = '/';
	}
#endif
	close();
}

// virtual
void GetOpenFilenameDialog::onReleaseTextButton(GWidgetTextButton* pButton)
{
	if(pButton == m_pCancel)
	{
		delete[] m_szFilename;
		m_szFilename = NULL;
		close();
	}
}

const char* GetOpenFilenameDialog::filename()
{
	return m_szFilename;
}

char* GetOpenFilenameDialog::releaseFilename()
{
	char* szFilename = m_szFilename;
	m_szFilename = NULL;
	return szFilename;
}

// -----------------------------------------------------------------------------------------------

GetSaveFilenameDialog::GetSaveFilenameDialog(const char* szMessage, const char* initialFilename, const char* szExt)
: GWidgetDialog(700, 500, 0xffcc6699)
{
	new GWidgetTextLabel(this, 10, 5, 300, 20, szMessage, 0xff442200);
	m_pFileBrowser = new GWidgetFileSystemBrowser(this, 10, 30, m_rect.w - 20, m_rect.h - 95, szExt);
	m_pFilename = new GWidgetTextBox(this, 20, m_rect.h - 60, m_rect.w - 40, 20);
	m_pFilename->setText(initialFilename);
	setFocusWidget(m_pFilename);
	m_pSave = new GWidgetTextButton(this, m_rect.w / 2 - 120, m_rect.h - 30, 100, 24, "Save");
	m_pCancel = new GWidgetTextButton(this, m_rect.w / 2 + 20, m_rect.h - 30, 100, 24, "Cancel");
	m_szFilename = NULL;
}

// virtual
GetSaveFilenameDialog::~GetSaveFilenameDialog()
{
}

// virtual
void GetSaveFilenameDialog::onSelectFilename(GWidgetFileSystemBrowser* pBrowser, const char* szFilename)
{
	m_pFilename->setText(szFilename);
}

// virtual
void GetSaveFilenameDialog::onReleaseTextButton(GWidgetTextButton* pButton)
{
	if(pButton == m_pCancel)
	{
		delete[] m_szFilename;
		m_szFilename = NULL;
		close();
	}
	else if(pButton == m_pSave)
	{
		const char* szFilename = m_pFilename->text().c_str();
		delete[] m_szFilename;
		m_szFilename = new char[strlen(szFilename) + 1];
		strcpy(m_szFilename, szFilename);
#ifdef WINDOWS
		int i;
		for(i = 0; m_szFilename[i] != '\0'; i++)
		{
			if(m_szFilename[i] == '\\')
				m_szFilename[i] = '/';
		}
#endif
		close();
	}
}

const char* GetSaveFilenameDialog::filename()
{
	return m_szFilename;
}

char* GetSaveFilenameDialog::releaseFilename()
{
	char* szFilename = m_szFilename;
	m_szFilename = NULL;
	return szFilename;
}

#endif // !NOGUI
