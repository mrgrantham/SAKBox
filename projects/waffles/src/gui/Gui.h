// -------------------------------------------------------------
// The contents of this file may be distributed under the CC0
// license (http://creativecommons.org/publicdomain/zero/1.0/).
// Of course you may also distribute it under a more restrictive
// license, such as any of the OSI-approved licenses
// (http://www.opensource.org/licenses).
// -------------------------------------------------------------

#ifndef __GUI_H__
#define __GUI_H__

#include <WClasses/WWidgets.h>

class ControllerBase;

#ifndef NOGUI
#include <SDL2/SDL.h>
#include <unordered_map>

inline Uint32* getPixMem32(int* pixels, int pitch, int x, int y)
{
	return (Uint32*)((Uint8*)pixels + y * pitch + (x << 2));
}

inline Uint16* getPixMem16(int* pixels, int pitch, int x, int y)
{
	return (Uint16*)pixels + y * pitch / 2 + x;
}


class ViewBase
{
protected:
	// SAKBox: every view (a popup too) draws to the one window, through one
	// renderer and texture (SDL allows a window one renderer). The last view
	// to go destroys them.
	static SDL_Window* window_;
	static SDL_Renderer* s_pRenderer;
	static SDL_Texture* s_pTexture;
	static int s_viewCount;
	SDL_Window* m_pScreen;
	WClasses::WRect m_screenRect;
	SDL_Renderer* sdlRenderer;
	SDL_Surface* window_surface; // software render
	SDL_Texture* sdlTexture; // hardware render
public:
	/// szTitle names the window. The first view makes it; a later one renames it.
	explicit ViewBase(const char* szTitle);
	virtual ~ViewBase();

	void update();

	virtual void onChar(char c) {}
	virtual void onSpecialKey(int key) {}
	virtual void onMouseDown(int nButton, int x, int y) {}
	virtual void onMouseUp(int nButton, int x, int y) {}
	virtual bool onMousePos(int x, int y) { return false; }
	WClasses::WRect* screenRect() { return &m_screenRect; }
	void captureScreen(WClasses::WImage* pImage);

protected:
	static void blitImage(int* pixels, int pitch, Uint32 format, int x, int y, WClasses::WImage* pImage);
	static void stretchClipAndBlitImage(int* pixels, int pitch, Uint32 format, WClasses::WRect* pDestRect, WClasses::WRect* pClipRect, WClasses::WImage* pImage);

	virtual void draw(int* pixels, int pitch) = 0;
	void drawDot(int* pixels, int pitch, Uint32 format, int x, int y, unsigned int col, int nSize);
	SDL_Window* makeScreen(int x, int y, const char* szTitle);
};

// when *pKeepRunning is false, the popup will close
// x, and y are the left and top margins. (Typically they will both be zero)
// pBackgroundImage is a screen shot to show in the background
// pDialog is the dialog to pop up in the middle
void RunPopup(WClasses::WWidgetDialog* pDialog);


void OpenFile(const char* szFilename);
void OpenAppFile(const char* szRelativeFilename);

#endif // !NOGUI

class MessageBoxDialog : public WClasses::WWidgetDialog
{
protected:
	WClasses::WWidgetTextButton* m_pOK;

public:
	MessageBoxDialog(const char* szMessage);
	~MessageBoxDialog() override;

	void onReleaseTextButton(WClasses::WWidgetTextButton* pButton) override;
};



class GetStringDialog : public WClasses::WWidgetDialog
{
public:
	WClasses::WWidgetTextBox* m_pTextBox;
	WClasses::WWidgetTextButton* m_pOK;
	WClasses::WWidgetTextButton* m_pCancel;

	GetStringDialog(const char* szMessage);
	~GetStringDialog() override;

	void onTextBoxPressEnter(WClasses::WWidgetTextBox* pTextBox) override;
	void onReleaseTextButton(WClasses::WWidgetTextButton* pButton) override;
};


class GetOpenFilenameDialog : public WClasses::WWidgetDialog
{
protected:
	WClasses::WWidgetFileSystemBrowser* m_pFileBrowser;
	WClasses::WWidgetTextButton* m_pCancel;
	char* m_szFilename;

public:
	GetOpenFilenameDialog(const char* szMessage, const char* szExt);
	~GetOpenFilenameDialog() override;

	void onSelectFilename(WClasses::WWidgetFileSystemBrowser* pBrowser, const char* szFilename) override;
	void onReleaseTextButton(WClasses::WWidgetTextButton* pButton) override;
	const char* filename();
	char* releaseFilename();
};


class GetSaveFilenameDialog : public WClasses::WWidgetDialog
{
protected:
	WClasses::WWidgetFileSystemBrowser* m_pFileBrowser;
	WClasses::WWidgetTextBox* m_pFilename;
	WClasses::WWidgetTextButton* m_pSave;
	WClasses::WWidgetTextButton* m_pCancel;
	char* m_szFilename;

public:
	GetSaveFilenameDialog(const char* szMessage, const char* initialFilename, const char* szExt);
	~GetSaveFilenameDialog() override;

	void onSelectFilename(WClasses::WWidgetFileSystemBrowser* pBrowser, const char* szFilename) override;
	void onReleaseTextButton(WClasses::WWidgetTextButton* pButton) override;
	const char* filename();
	char* releaseFilename();
};


class ControllerBase
{
protected:
	bool m_bKeepRunning;

	static WClasses::WImage* s_pManualImage;
	static WClasses::WImage* s_pCloseImage;

#ifndef NOGUI
	enum KeyState
	{
		Normal,
		Holding,
		Repeating,
	};

	std::unordered_map<int, int> m_keyboard;
	int m_mouse[4];
	int m_mouseX;
	int m_mouseY;
	KeyState m_eKeyState;
	SDL_Keycode m_lastPressedKey;
	double m_dKeyRepeatTimer;
	ViewBase* m_pView;
#endif // !NOGUI
public:
	ControllerBase();
	virtual ~ControllerBase();

	void quit() { m_bKeepRunning = false; }

#ifndef NOGUI
	virtual void onChar(char c) { m_pView->onChar(c); }
	virtual void onSpecialKey(int key) { m_pView->onSpecialKey(key); }
	virtual void onMouseDown(int nButton, int x, int y) { m_pView->onMouseDown(nButton, x, y); }
	virtual void onMouseUp(int nButton, int x, int y) { m_pView->onMouseUp(nButton, x, y); }
	virtual bool onMousePos(int x, int y) { return m_pView->onMousePos(x, y); }

	bool handleEvents(double dTimeDelta); // HandleEvents returns true if it thinks the view needs to be updated
	void mousePos(int* pX, int* pY);
#endif // !NOGUI

protected:
#ifndef NOGUI
	void handleKeyPress(SDL_Keycode eKey, SDL_Keymod mod);
	void handleMouseClick(int nButton);
#endif // !NOGUI
};


#endif // __GUI_H__
