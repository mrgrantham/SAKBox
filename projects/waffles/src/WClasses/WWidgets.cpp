/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
    anonymous contributors,

  to the public domain (http://creativecommons.org/publicdomain/zero/1.0/).

  Note that some moral obligations still exist in the absence of legal ones.
  For example, it would still be dishonest to deliberately misrepresent the
  origin of a work. Although we impose no legal requirements to obtain a
  license, it is beseeming for those who build on the works of others to
  give back useful improvements, or find a way to pay it forward. If
  you would like to cite us, a published paper about Waffles can be found
  at http://jmlr.org/papers/volume12/gashler11a/gashler11a.pdf. If you find
  our code to be useful, the Waffles team would love to hear how you use it.
*/

#include "WHolders.h"
#include "WWidgets.h"
#include "WImage.h"
#include "WDirList.h"
#ifdef WINDOWS
#include <direct.h>
#else
#include <unistd.h>
#endif
#include "WFile.h"
#include "WHeap.h"
#include <math.h>
#include "WTime.h"
#include "WBits.h"
#include "WWave.h"
#include <algorithm>
#include <sstream>

namespace WClasses
{

using std::string;
using std::vector;

WWidgetCommon::WWidgetCommon()
{
}

WWidgetCommon::~WWidgetCommon()
{
}

void WWidgetCommon::drawButtonText(WImage* pImage, int x, int y, int w, int h, const char* text, bool pressed)
{
	float fontSize = std::max(1.0f, (float)h / 24);
	int wid = pImage->measureTextWidth(text, fontSize);
	int xx = x + (w - wid) / 2;
	if(xx < x)
		xx = x;
	int yy = y + (int)((h - fontSize * 12) / 2);
	if(yy < y)
		yy = y;
	pImage->text(text, xx, yy, fontSize, pressed ? 0xffffffff : 0xff000000, x + w - xx, y + h - yy);
}

void WWidgetCommon::drawLabelText(WImage* pImage, int x, int y, int w, int h, const char* text, float fontSize, bool alignLeft, unsigned int col)
{
	int xx;
	if(alignLeft)
		xx = x;
	else
	{
		int wid = pImage->measureTextWidth(text, fontSize);
		xx = x + w - wid - 2;
		if(xx < x)
			xx = x;
	}
	int yy = y;
	if(yy < y)
		yy = y;
	pImage->text(text, xx, yy, fontSize, col, x + w - xx, y + h - yy);
}

void WWidgetCommon::drawHorizCurvedOutSurface(WImage* pImage, int x, int y, int w, int h, unsigned int col)
{
	if(w <= 0 || h <= 0)
		return;
	float fac = (float)1.1 / w;
	int n;
	for(n = 0; n < w; n++)
	{
		float t = (float)n * fac - (float).1;
		int shade = (int)(255 * (1 - (t * t)));
		pImage->lineNoChecks(x, y, x, y + h - 1, MixColors(0xffffffff, col, shade));
		x++;
	}
}

void WWidgetCommon::drawHorizCurvedInSurface(WImage* pImage, int x, int y, int w, int h, unsigned int col)
{
	if(w <= 0 || h <= 0)
		return;
	int n;
	for(n = 0; n < w; n++)
	{
		float t = (float)(n + n) / w - 1;
		int shade = (int)(64 + 191 * ((t * t)));
		pImage->lineNoChecks(x, y, x, y + h - 1, MixColors(col, 0xff000000, shade));
		x++;
	}
}

void WWidgetCommon::drawVertCurvedOutSurface(WImage* pImage, int x, int y, int w, int h, unsigned int col)
{
	float fac = (float)1.1 / h;
	int yStart = 0;
	if(y < 0)
		yStart = -y;
	if(y + h > (int)pImage->height())
		h = pImage->height() - y;
	int n;
	for(n = yStart; n < h; n++)
	{
		float t = (float)n * fac - (float).1;
		int shade = (int)(255 * (1 - (t * t)));
		pImage->lineNoChecks(x, y, x + w - 1, y, MixColors(0xffffffff, col, shade));
		y++;
	}
}

void WWidgetCommon::drawVertCurvedInSurface(WImage* pImage, int x, int y, int w, int h, unsigned int col)
{
	int n;
	for(n = 0; n < h; n++)
	{
		float t = (float)(n + n) / h - 1;
		int shade = (int)(64 + 191 * ((t * t)));
		pImage->lineNoChecks(x, y, x + w - 1, y, MixColors(col, 0xff000000, shade));
		y++;
	}
}

void WWidgetCommon::drawCursor(WImage* pImage, int x, int y, int w, int h)
{
	pImage->box(x, y, x + w, y + h, 0xffffffff);
}

void WWidgetCommon::drawClipped(WImage* pCanvas, int x, int y, WWidget* pWidget, WRect* pClipRect)
{
	// See if we can do it without clipping
	WRect* pWidgetRect = pWidget->rect();
	if(x >= pClipRect->x && y >= pClipRect->y && x + pWidgetRect->w <= pClipRect->x + pClipRect->w && y + pWidgetRect->h <= pClipRect->y + pClipRect->h)
	{
		pWidget->draw(pCanvas, x, y);
		return;
	}

	// Draw onto the buffer image, then copy the clipped part
	if((int)m_bufferImage.width() < pWidgetRect->w || (int)m_bufferImage.height() < pWidgetRect->h)
		m_bufferImage.setSize(std::max((int)m_bufferImage.width(), pWidgetRect->w), std::max((int)m_bufferImage.height(), pWidgetRect->h));

	int xx = std::max(x, pClipRect->x);
	int yy = std::max(y, pClipRect->y);
	WRect r(xx - x, yy - y, std::min(x + pWidgetRect->w, pClipRect->x + pClipRect->w) - xx, std::min(y + pWidgetRect->h, pClipRect->y + pClipRect->h) - yy);
	m_bufferImage.boxFill(r.x, r.y, r.w, r.h, 0);
	pWidget->draw(&m_bufferImage, 0, 0);
	pCanvas->blitAlpha(xx, yy, &m_bufferImage, &r);
}

// ----------------------------------------------------------------------

WWidget::WWidget(WWidgetGroup* pParent, int x, int y, int w, int h)
{
	m_pParent = pParent;
	m_common = pParent ? pParent->m_common : nullptr;
	m_rect.set(x, y, w, h);
	if(pParent)
		pParent->addWidget(this);
	else
		m_nID = -1;
}

/*virtual*/ WWidget::~WWidget()
{
	//WAssert(m_pParent || type() == Dialog); // Unexpected root widget
	if(m_pParent)
		m_pParent->onDestroyWidget(this);
}

void WWidget::setPos(int x, int y)
{
	m_rect.x = x;
	m_rect.y = y;
	// todo: dirty the parent?
}

// ----------------------------------------------------------------------

WWidgetAtomic::WWidgetAtomic(WWidgetGroup* pParent, int x, int y, int w, int h)
: WWidget(pParent, x, y, w, h)
{
	WAssert(pParent); // atomic widgets require a parent
}

/*virtual*/ WWidgetAtomic::~WWidgetAtomic()
{
}

/*virtual*/ void WWidgetAtomic::onChar(char c)
{
	if(m_pParent)
		m_pParent->onChar(c);
}

/*virtual*/ void WWidgetAtomic::onSpecialKey(int key)
{
	if(m_pParent)
		m_pParent->onSpecialKey(key);
}

/*virtual*/ void WWidgetAtomic::onMouseMove(int dx, int dy)
{
}

// ----------------------------------------------------------------------

WWidgetGroup::WWidgetGroup(WWidgetGroup* pParent, int x, int y, int w, int h)
: WWidget(pParent, x, y, w, h)
{
	m_dirtyBits.push_back(0);
}

/*virtual*/ WWidgetGroup::~WWidgetGroup()
{
	while(m_widgets.size() > 0)
		delete(m_widgets[0]);
}

bool WWidgetGroup::getDirtyBit(int nBit)
{
	unsigned int n = m_dirtyBits[nBit / 32];
	return (((1 << (nBit % 32)) & n) != 0);
}

void WWidgetGroup::setDirtyBit(int nBit, bool bValue)
{
	int index = nBit / 32;
	int offset = nBit % 32;
	unsigned int n = m_dirtyBits[index];
	if(bValue)
		n = (n | (1 << offset));
	else
		n = (n & (~(1 << offset)));
	m_dirtyBits[index] = n;
}

void WWidgetGroup::tattle(WWidget* pChild)
{
	if(pChild)
	{
		WAssert(childWidget(pChild->m_nID) == pChild); // that's not my child
		if(getDirtyBit(pChild->m_nID))
			return; // we already know this child is dirty
		m_dirtyChildren.push_back(pChild);
		setDirtyBit(pChild->m_nID, true);
	}
	else
		setDirtyBit((int)m_widgets.size(), true);
	if(m_pParent)
		m_pParent->tattle(this);
}

void WWidgetGroup::addWidget(WWidget* pWidget)
{
	pWidget->m_nID = (int)m_widgets.size();
	m_widgets.push_back(pWidget);
	if((m_widgets.size() + 33) / 32 > m_dirtyBits.size())
		m_dirtyBits.push_back(0);
	setDirtyBit((int)m_widgets.size(), getDirtyBit((int)m_widgets.size() - 1));
	tattle(pWidget);
}

// todo: use a divide-and-conquer technique to improve performance
/*virtual*/ WWidgetAtomic* WWidgetGroup::findAtomicWidget(int x, int y)
{
	int n;
	int nCount = (int)m_widgets.size();
	WWidget* pWidget;
	for(n = 0; n < nCount; n++)
	{
		pWidget = m_widgets[n];
		if(pWidget->rect()->doesInclude(x, y))
		{
			if(pWidget->isAtomic())
			{
				if(((WWidgetAtomic*)pWidget)->isClickable())
					return (WWidgetAtomic*)pWidget;
			}
			else
			{
				WRect* pRect = pWidget->rect();
				return ((WWidgetGroup*)pWidget)->findAtomicWidget(x - pRect->x, y - pRect->y);
			}
		}
	}
	return nullptr;
}

int WWidgetGroup::childWidgetCount()
{
	return (int)m_widgets.size();
}

WWidget* WWidgetGroup::childWidget(int n)
{
	return m_widgets[n];
}

/*virtual*/ void WWidgetGroup::onDestroyWidget(WWidget* pWidget)
{
	if(pWidget->m_nID >= 0)
	{
		// Remove the widget from my list
		WAssert(childWidget(pWidget->m_nID) == pWidget); // bad id
		int nLast = (int)m_widgets.size() - 1;
		WWidget* pLast = m_widgets[nLast];
		pLast->m_nID = pWidget->m_nID;
		m_widgets[pWidget->m_nID] = pLast;
		m_widgets.pop_back();
		setDirtyBit((int)m_widgets.size(), getDirtyBit((int)m_widgets.size() + 1));
		pWidget->m_nID = -1;
	}
	if(m_pParent)
		m_pParent->onDestroyWidget(pWidget);
}

void WWidgetGroup::setClean()
{
	m_dirtyChildren.clear();
	int nCount = (int)m_dirtyBits.size();
	for(int i = 0; i < nCount; i++)
		m_dirtyBits[i] = 0;
}

// ----------------------------------------------------------------------

WWidgetDialog::WWidgetDialog(int w, int h, unsigned int cBackground)
: WWidgetGroup(nullptr, 0, 0, w, h)
{
	m_cBackground = cBackground;
	m_image.setSize(w, h);
	m_image.clear(cBackground);
	m_common = new WWidgetCommon();
	m_pGrabbedWidget = nullptr;
	m_pFocusWidget = nullptr;
	m_prevMouseX = 0;
	m_prevMouseY = 0;
	m_bRunning = true;
}

/*virtual*/ WWidgetDialog::~WWidgetDialog()
{
	releaseButton(1);
	delete(m_common);
}

void WWidgetDialog::setBackgroundImage(WImage* pImage)
{
	WRect r(0, 0, std::min((unsigned int)m_rect.w, pImage->width()), std::min((unsigned int)m_rect.h, pImage->height()));
	m_image.blit(0, 0, pImage, &r);
}

// virtual
void WWidgetDialog::draw(WImage* pCanvas, int x, int y)
{
	vector<WWidget*>* pDirtyChildren = &m_dirtyChildren;
	if(getDirtyBit((int)m_widgets.size()))
	{
		setDirtyBit((int)m_widgets.size(), false);
		pDirtyChildren = &m_widgets;
		//m_image.Clear(m_cBackground);
	}
	int nCount = (int)pDirtyChildren->size();
	WRect* pRect;
	WWidget* pChild;
	for(int n = 0; n < nCount; n++)
	{
		pChild = (*pDirtyChildren)[n];
		pRect = pChild->rect();
		WAssert(x + pRect->x >= 0 && y + pRect->y >= 0 && x + pRect->x + pRect->w <= (int)pCanvas->width() && y + pRect->y + pRect->h <= (int)pCanvas->height()); // out of range
		pChild->draw(pCanvas, x + pRect->x, y + pRect->y);
	}
	setClean();
}

void WWidgetDialog::setFocusWidget(WWidgetAtomic* pWidget)
{
	if(m_pFocusWidget != pWidget)
	{
		if(m_pFocusWidget)
			m_pFocusWidget->onLoseFocus();
		m_pFocusWidget = pWidget;
		if(pWidget)
			pWidget->onGetFocus();
	}
}

void WWidgetDialog::grabWidget(WWidgetAtomic* pWidget, int button, int mouseX, int mouseY)
{
	releaseButton(button);
	m_pGrabbedWidget = pWidget;
	setFocusWidget(pWidget);
	if(pWidget)
	{
		WWidget* pTmp;
		WRect* pRect;
		for(pTmp = pWidget; pTmp; pTmp = pTmp->parent())
		{
			pRect = pTmp->rect();
			mouseX -= pRect->x;
			mouseY -= pRect->y;
		}
		pWidget->grab(button, mouseX, mouseY);
	}
}

// virtual
void WWidgetDialog::pressButton(int button, int x, int y)
{
	WWidgetAtomic* pPressWidget = findAtomicWidget(x, y);
	grabWidget(pPressWidget, button, x, y);
}

// virtual
void WWidgetDialog::releaseButton(int button)
{
	if(!m_pGrabbedWidget)
		return;

	// Use a local var in case the handler destroys this dialog
	WWidgetAtomic* pGrabbedWidget = m_pGrabbedWidget;
	m_pGrabbedWidget = nullptr;
	pGrabbedWidget->release(button);
}

// virtual
void WWidgetDialog::onDestroyWidget(WWidget* pWidget)
{
	if(pWidget->parent() == this)
	{
		WRect* pRect = pWidget->rect();
		m_image.boxFill(pRect->x, pRect->y, pRect->w, pRect->h, m_cBackground);
		m_dirtyChildren.clear();
		tattle(nullptr);
	}
	if(pWidget == m_pGrabbedWidget)
		m_pGrabbedWidget = nullptr;
	if(pWidget == m_pFocusWidget)
		m_pFocusWidget = nullptr;
	WWidgetGroup::onDestroyWidget(pWidget);
}

// virtual
void WWidgetDialog::handleChar(char c)
{
	if(!m_pFocusWidget)
		return;
	m_pFocusWidget->onChar(c);
}

// virtual
void WWidgetDialog::handleSpecialKey(int key)
{
	if(!m_pFocusWidget)
		return;
	m_pFocusWidget->onSpecialKey(key);
}

// virtual
bool WWidgetDialog::handleMousePos(int x, int y)
{
	x -= m_prevMouseX;
	y -= m_prevMouseY;
	if(x == 0 && y == 0)
		return false;
	m_prevMouseX += x;
	m_prevMouseY += y;
	if(!m_pGrabbedWidget)
		return false;
	m_pGrabbedWidget->onMouseMove(x, y);
	return true;
}

WImage* WWidgetDialog::image()
{
	draw(&m_image, 0, 0);
	return &m_image;
}

// ----------------------------------------------------------------------

WWidgetTextButton::WWidgetTextButton(WWidgetGroup* pParent, int x, int y, int w, int h, const char* szText)
: WWidgetAtomic(pParent, x, y, w, h)
{
	m_text = szText;
	m_pressed = false;
	m_holding = false;
	m_color = 0xff0000ff;
}

/*virtual*/ WWidgetTextButton::~WWidgetTextButton()
{
}

/*virtual*/ void WWidgetTextButton::grab(int button, int x, int y)
{
	m_pressed = true;
	m_holding = true;
	m_pParent->tattle(this);
	m_pressedX = x;
	m_pressedY = y;
	m_pParent->onPushTextButton(this);
}

/*virtual*/ void WWidgetTextButton::release(int button)
{
	if(m_pressed)
	{
		m_pressed = false;
		m_pParent->tattle(this);
		m_pParent->onReleaseTextButton(this);
	}
	m_holding = false;
}

// virtual
void WWidgetTextButton::draw(WImage* pCanvas, int x, int y)
{
	int w = m_rect.w;
	int h = m_rect.h;
	if(m_pressed)
	{
		int nHorizOfs = (int)(w * (float).05);
		int nVertOfs = (int)(h * (float).15);
		m_common->drawVertCurvedInSurface(pCanvas, x, y, w, h, m_color);
		m_common->drawButtonText(pCanvas, x + nHorizOfs, y + nVertOfs, w - nHorizOfs, h - nVertOfs, m_text.c_str(), true);
		pCanvas->box(x, y, x + w - 1, y + h - 1, gRGB(255, 255, 255));
	}
	else
	{
		m_common->drawVertCurvedOutSurface(pCanvas, x, y, w, h, m_color);
		m_common->drawButtonText(pCanvas, x, y, w, h, m_text.c_str(), false);
		pCanvas->box(x, y, x + w - 1, y + h - 1, gRGB(64, 64, 64));
	}
}

void WWidgetTextButton::setText(const char* szText)
{
	m_text = szText;
	m_pParent->tattle(this);
}

void WWidgetTextButton::setColor(unsigned int c)
{
	m_color = c;
	m_pParent->tattle(this);
}

/*virtual*/ void WWidgetTextButton::onMouseMove(int dx, int dy)
{
	if(!m_holding)
		return;
	m_pressedX += dx;
	m_pressedY += dy;
	if(m_pressed)
	{
		if(m_pressedX < 0 || m_pressedX >= m_rect.w || m_pressedY < 0 || m_pressedY >= m_rect.h)
		{
			m_pressed = false;
			m_pParent->tattle(this);
		}
	}
	else
	{
		if(m_pressedX >= 0 && m_pressedX < m_rect.w && m_pressedY >= 0 && m_pressedY < m_rect.h)
		{
			m_pressed = true;
			m_pParent->tattle(this);
		}
	}
}


// ----------------------------------------------------------------------

WWidgetTextTab::WWidgetTextTab(WWidgetGroup* pParent, int x, int y, int w, int h, const char* szText, unsigned int cBackground)
: WWidgetAtomic(pParent, x, y, w, h)
{
	m_text = szText;
	m_cBackground = cBackground;
	m_selected = false;
}

/*virtual*/ WWidgetTextTab::~WWidgetTextTab()
{
}

/*virtual*/ void WWidgetTextTab::grab(int button, int x, int y)
{
	m_pParent->onSelectTextTab(this);
}

/*virtual*/ void WWidgetTextTab::release(int button)
{
}

// virtual
void WWidgetTextTab::draw(WImage* pCanvas, int x, int y)
{
	// Draw the non-pressed image
	int w = m_rect.w;
	int h = m_rect.h;
	if(m_selected)
	{
		int nHorizOfs = (int)(w * (float).05);
		int nVertOfs = (int)(h * (float).15);
		m_common->drawHorizCurvedInSurface(pCanvas, x, y, w, h, m_cBackground);
		m_common->drawButtonText(pCanvas, x + nHorizOfs, y + nVertOfs, w - nHorizOfs, h - nVertOfs, m_text.c_str(), true);
		pCanvas->box(x, y, x + w - 1, y + h - 1, gRGB(255, 255, 255));
	}
	else
	{
		m_common->drawHorizCurvedOutSurface(pCanvas, x, y, w, h, m_cBackground);
		m_common->drawButtonText(pCanvas, x, y, w, h, m_text.c_str(), false);
		pCanvas->box(x, y, x + w - 1, y + h - 1, gRGB(64, 64, 64));
	}
}

void WWidgetTextTab::setText(const char* szText)
{
	m_text = szText;
	m_pParent->tattle(this);
}

void WWidgetTextTab::setSelected(bool selected)
{
	m_selected = selected;
	m_pParent->tattle(this);
}

// ----------------------------------------------------------------------

WWidgetImageButton::WWidgetImageButton(WWidgetGroup* pParent, int x, int y, WImage* pImage)
: WWidgetAtomic(pParent, x, y, pImage->width() / 2, pImage->height())
{
	m_image.copy(pImage);
	m_pressed = false;
	m_holding = false;
}

/*virtual*/ WWidgetImageButton::~WWidgetImageButton()
{
}

/*virtual*/ void WWidgetImageButton::grab(int button, int x, int y)
{
	m_pressed = true;
	m_holding = true;
	m_pParent->tattle(this);
	m_pressedX = x;
	m_pressedY = y;
}

/*virtual*/ void WWidgetImageButton::release(int button)
{
	m_holding = false;
	if(m_pressed)
	{
		m_pressed = false;
		m_pParent->tattle(this);
		m_pParent->onReleaseImageButton(this);
	}
}

/*virtual*/ void WWidgetImageButton::onMouseMove(int dx, int dy)
{
	if(!m_holding)
		return;
	m_pressedX += dx;
	m_pressedY += dy;
	if(m_pressed)
	{
		if(m_pressedX < 0 || m_pressedX >= m_rect.w || m_pressedY < 0 || m_pressedY >= m_rect.h)
		{
			m_pressed = false;
			m_pParent->tattle(this);
		}
	}
	else
	{
		if(m_pressedX >= 0 && m_pressedX < m_rect.w && m_pressedY >= 0 && m_pressedY < m_rect.h)
		{
			m_pressed = true;
			m_pParent->tattle(this);
		}
	}
}

// virtual
void WWidgetImageButton::draw(WImage* pCanvas, int x, int y)
{
	WRect r((m_pressed ? m_rect.w : 0), 0, m_rect.w, m_rect.h);
	pCanvas->blitAlpha(x, y, &m_image, &r);
}

// ----------------------------------------------------------------------

WWidgetAnimation::WWidgetAnimation(WWidgetGroup* pParent, int x, int y, WImage* pImage, int nFrames)
: WWidgetAtomic(pParent, x, y, pImage->width(), pImage->height() / nFrames)
{
	m_image.copy(pImage);
	m_nFrames = nFrames;
	m_nFrame = 0;
}

/*virtual*/ WWidgetAnimation::~WWidgetAnimation()
{
}

// virtual
void WWidgetAnimation::draw(WImage* pCanvas, int x, int y)
{
	WRect r(0, m_nFrame * m_rect.h, m_rect.w, m_rect.h);
	pCanvas->blitAlpha(x, y, &m_image, &r);
}

void WWidgetAnimation::setFrame(int nFrame)
{
	m_nFrame = nFrame;
	m_pParent->tattle(this);
}

// ----------------------------------------------------------------------

WWidgetTextLabel::WWidgetTextLabel(WWidgetGroup* pParent, int x, int y, int w, int h, const char* szText, unsigned int c, unsigned int back, float fontSize)
: WWidgetAtomic(pParent, x, y, w, h)
{
	m_text = szText;
	m_alignLeft = true;
	m_fontSize = fontSize;
	m_cForeground = c;
	m_cBackground = back;
	m_bGrabbed = false;
}

/*virtual*/ WWidgetTextLabel::~WWidgetTextLabel()
{
}

/*virtual*/ void WWidgetTextLabel::grab(int button, int x, int y)
{
	m_pParent->tattle(this);
	m_bGrabbed = true;
	if(button == 1)
		m_pParent->onClickTextLabel(this);
}

/*virtual*/ void WWidgetTextLabel::release(int button)
{
	m_bGrabbed = false;
	m_pParent->tattle(this);
}

// virtual
void WWidgetTextLabel::draw(WImage* pCanvas, int x, int y)
{
	if(m_bGrabbed)
		pCanvas->boxFill(x, y, m_rect.w, m_rect.h, 0xffffff00);
	else if(gAlpha(m_cBackground) != 0)
		pCanvas->boxFill(x, y, m_rect.w, m_rect.h, m_cBackground);
	size_t beg = 0;
	int yy = y;
	while(true)
	{
		size_t end;
		for(end = beg; end < m_text.length() && m_text[end] != '\n'; end++)
		{
		}
		if(end <= beg)
			break;
		string s;
		s.assign(m_text, beg, end - beg);
		m_common->drawLabelText(pCanvas, x, yy, m_rect.w, m_rect.h, s.c_str(), m_fontSize, m_alignLeft, m_cForeground);
		yy += (int)(14 * m_fontSize);
		if(yy >= y + m_rect.h)
			break;
		beg = end + 1;
	}
}

void WWidgetTextLabel::setText(const char* szText)
{
	m_text = szText;
	m_pParent->tattle(this);
}

void WWidgetTextLabel::setForegroundColor(unsigned int c)
{
	m_cForeground = c;
	m_pParent->tattle(this);
}

void WWidgetTextLabel::setBackgroundColor(unsigned int c)
{
	m_cBackground = c;
	m_pParent->tattle(this);
}

void WWidgetTextLabel::setAlignLeft(bool bAlignLeft)
{
	m_alignLeft = bAlignLeft;
	m_pParent->tattle(this);
}

void WWidgetTextLabel::wrap()
{
	size_t start = 0;
	while(true)
	{
		size_t pos = start;
		size_t lastSpace = start;
		int wid = 0;
		while(pos < m_text.length() && wid <= m_rect.w)
		{
			char c = m_text[pos];
			if(c == ' ')
				lastSpace = pos;
			wid += WImage::measureCharWidth(c, m_fontSize);
			pos++;
		}
		if(wid <= m_rect.w)
			break;
		if(lastSpace > start)
		{
			m_text[lastSpace] = '\n';
			start = lastSpace + 1;
		}
		else if(pos > 1)
		{
			m_text.insert(pos - 1, 1, '\n');
			start = pos;
		}
		else
			start++;
	}
}

// ----------------------------------------------------------------------
/*
WWidgetImageLabel::WWidgetImageLabel(WWidgetGroup* pParent, int x, int y, const char* szHexPngFile)
: WWidgetAtomic(pParent, x, y, 0, 0)
{
	m_image.loadPngFromHex(szHexPngFile);
	rect()->w = m_image.width();
	rect()->h = m_image.height();
}

// virtual
WWidgetImageLabel::~WWidgetImageLabel()
{
}

// virtual
void WWidgetImageLabel::draw(WImage* pCanvas, int x, int y)
{
	WRect r(0, 0, m_rect.w, m_rect.h);
	pCanvas->blitAlpha(x, y, &m_image, &r);
}
*/
// ----------------------------------------------------------------------

WWidgetGroupBox::WWidgetGroupBox(WWidgetGroup* pParent, int x, int y, int w, int h, unsigned int cLight, unsigned int cShadow)
: WWidgetAtomic(pParent, x, y, w, h)
{
	m_cLight = cLight;
	m_cShadow = cShadow;
}

/*virtual*/ WWidgetGroupBox::~WWidgetGroupBox()
{
}

// virtual
void WWidgetGroupBox::draw(WImage* pCanvas, int x, int y)
{
	pCanvas->box(x, y, x + m_rect.w - 2, y + m_rect.h - 2, m_cShadow);
	pCanvas->box(x + 1, y + 1, x + m_rect.w - 1, y + m_rect.h - 1, m_cLight);
	pCanvas->setPixel(x + m_rect.w - 1, y, m_cShadow);
	pCanvas->setPixel(x, y + m_rect.h - 1, m_cShadow);
}

void WWidgetGroupBox::setLightColor(unsigned int c)
{
	m_cLight = c;
	m_pParent->tattle(this);
}

void WWidgetGroupBox::setShadowColor(unsigned int c)
{
	m_cShadow = c;
	m_pParent->tattle(this);
}

// ----------------------------------------------------------------------

WWidgetVCRButton::WWidgetVCRButton(WWidgetGroup* pParent, int x, int y, int w, int h, VCR_Type eType)
: WWidgetAtomic(pParent, x, y, w, h)
{
	m_eType = eType;
	m_pressed = false;
}

/*virtual*/ WWidgetVCRButton::~WWidgetVCRButton()
{
}

/*virtual*/ void WWidgetVCRButton::grab(int button, int x, int y)
{
	m_pressed = true;
	m_pParent->tattle(this);
	m_pParent->onPushVCRButton(this);
}

/*virtual*/ void WWidgetVCRButton::release(int button)
{
	m_pressed = false;
	m_pParent->tattle(this);
}

void WWidgetVCRButton::drawIcon(WImage* pCanvas, int nHorizOfs, int nVertOfs)
{
	int nMinSize = m_rect.w;
	if(nMinSize > m_rect.h)
		nMinSize = m_rect.h;
	int nArrowSize = nMinSize / 3;
	int hh = m_rect.h / 2;
	int n;
	if(m_eType == ArrowRight)
	{
		for(n = 0; n < nArrowSize; n++)
			pCanvas->lineNoChecks(nHorizOfs + hh - nArrowSize / 2 + n, nVertOfs + hh - nArrowSize + n + 1, nHorizOfs + hh - nArrowSize / 2 + n, nVertOfs + hh + nArrowSize - n - 1, 0);
	}
	else if(m_eType == ArrowLeft)
	{
		for(n = 0; n < nArrowSize; n++)
			pCanvas->lineNoChecks(nHorizOfs + hh + nArrowSize / 2 - n, nVertOfs + hh - nArrowSize + n + 1, nHorizOfs + hh + nArrowSize / 2 - n, nVertOfs + hh + nArrowSize - n - 1, 0xff000000);
	}
	if(m_eType == ArrowDown)
	{
		for(n = 0; n < nArrowSize; n++)
			pCanvas->lineNoChecks(nHorizOfs + hh - nArrowSize + n + 1, nVertOfs + hh - nArrowSize / 2 + n, nHorizOfs + hh + nArrowSize - n - 1, nVertOfs + hh - nArrowSize / 2 + n, 0xff000000);
	}
	else if(m_eType == ArrowUp)
	{
		for(n = 0; n < nArrowSize; n++)
			pCanvas->lineNoChecks(nHorizOfs + hh - nArrowSize + n + 1, nVertOfs + hh + nArrowSize / 2 - n, nHorizOfs + hh + nArrowSize - n - 1, nVertOfs + hh + nArrowSize / 2 - n, 0xff000000);
	}
	else if(m_eType == Square)
	{
		pCanvas->box(nHorizOfs + hh - nArrowSize, nVertOfs + hh - nArrowSize, nArrowSize * 2, nArrowSize * 2, 0xff000000);
	}
}

// virtual
void WWidgetVCRButton::draw(WImage* pCanvas, int x, int y)
{
	int w = m_rect.w;
	int h = m_rect.h;
	if(m_pressed)
	{
		int nHorizOfs = (int)(w * (float).05);
		int nVertOfs = (int)(h * (float).15);
		m_common->drawVertCurvedInSurface(pCanvas, x, y, w, h);
		drawIcon(pCanvas, x + nHorizOfs, y + nVertOfs);
		pCanvas->box(x, y, x + w - 1, y + h - 1, gRGB(255, 255, 255));
	}
	else
	{
		m_common->drawVertCurvedOutSurface(pCanvas, x, y, w, h);
		drawIcon(pCanvas, x, y);
		pCanvas->box(x, y, x + w - 1, y + h - 1, gRGB(64, 64, 64));
	}
}

void WWidgetVCRButton::setType(VCR_Type eType)
{
	m_eType = eType;
	m_pParent->tattle(this);
}

// ----------------------------------------------------------------------

WWidgetProgressBar::WWidgetProgressBar(WWidgetGroup* pParent, int x, int y, int w, int h)
: WWidgetAtomic(pParent, x, y, w, h)
{
	m_fProgress = 0;
}

/*virtual*/ WWidgetProgressBar::~WWidgetProgressBar()
{
}

// virtual
void WWidgetProgressBar::draw(WImage* pCanvas, int x, int y)
{
	int w = m_rect.w;
	int h = m_rect.h;
	int pos;
	if(w >= h)
	{
		if(m_fProgress >= 0)
		{
			pos = (int)(m_fProgress * (w - 2));
			m_common->drawVertCurvedOutSurface(pCanvas, x + 1, y + 1, pos, h - 2);
			pCanvas->boxFill(x + 1 + pos, y + 1, w - 3 - pos, h - 2, 0xff000000);
		}
		else
		{
			pos = (int)((m_fProgress + 1) * (w - 2));
			m_common->drawVertCurvedOutSurface(pCanvas, x + 1 + pos, y + 1, w - 3 - pos, h - 2);
			pCanvas->boxFill(x + 1, y + 1, pos, h - 2, 0xff000000);
		}
	}
	else
	{
		if(m_fProgress >= 0)
		{
			pos = (int)(m_fProgress * (h - 2));
			m_common->drawHorizCurvedOutSurface(pCanvas, x + 1, y + h - 1 - pos, w - 2, pos);
			pCanvas->boxFill(x + 1, y + 1, w - 2, h - 2 - pos, 0xff000000);
		}
		else
		{
			pos = (int)((m_fProgress + 1) * (h - 2));
			m_common->drawHorizCurvedOutSurface(pCanvas, x + 1, y + 1, w - 2, h - 2 - pos);
			pCanvas->boxFill(x + 1, y + h - 1 - pos, w - 2, pos, 0xff000000);
		}
	}
	pCanvas->box(x, y, x + w - 1, y + h - 1, gRGB(64, 64, 64));
}

void WWidgetProgressBar::setProgress(float fProgress)
{
	m_fProgress = fProgress;
	m_pParent->tattle(this);
}

// ----------------------------------------------------------------------

WWidgetCheckBox::WWidgetCheckBox(WWidgetGroup* pParent, int x, int y, int w, int h)
: WWidgetAtomic(pParent, x, y, w, h)
{
	m_checked = false;
}

/*virtual*/ WWidgetCheckBox::~WWidgetCheckBox()
{
}

/*virtual*/ void WWidgetCheckBox::grab(int button, int x, int y)
{
	// todo: gray the box?
}

/*virtual*/ void WWidgetCheckBox::release(int button)
{
	setChecked(!m_checked);
}

// virtual
void WWidgetCheckBox::draw(WImage* pCanvas, int x, int y)
{
	int w = m_rect.w;
	int h = m_rect.h;
	pCanvas->box(x, y, x + w - 1, y + h - 1, 0xff000000);
	pCanvas->box(x + 1, y + 1, x + w - 2, y + h - 2, gRGB(64, 128, 128));
	pCanvas->boxFill(x + 2, y + 2, w - 4, h - 4, 0xffffffff);
	if(m_checked)
	{
		pCanvas->lineNoChecks(x + 4, y + 4, x + w - 5, y + h - 5, 0xff000000);
		pCanvas->lineNoChecks(x + 5, y + 4, x + w - 5, y + h - 6, 0xff000000);
		pCanvas->lineNoChecks(x + 4, y + 5, x + w - 6, y + h - 5, 0xff000000);

		pCanvas->lineNoChecks(x + w - 5, y + 4, x + 4, y + h - 5, 0xff000000);
		pCanvas->lineNoChecks(x + w - 6, y + 4, x + 4, y + h - 6, 0xff000000);
		pCanvas->lineNoChecks(x + w - 5, y + 5, x + 5, y + h - 5, 0xff000000);
	}
}

void WWidgetCheckBox::setChecked(bool checked)
{
	m_checked = checked;
	m_pParent->onChangeCheckBox(this);
	m_pParent->tattle(this);
}

// ----------------------------------------------------------------------

WWidgetBulletHole::WWidgetBulletHole(WWidgetGroup* pParent, int x, int y, int w, int h)
: WWidgetAtomic(pParent, x, y, w, h)
{
	m_checked = false;
}

/*virtual*/ WWidgetBulletHole::~WWidgetBulletHole()
{
}

/*virtual*/ void WWidgetBulletHole::grab(int button, int x, int y)
{
	m_checked = true;
	m_pParent->tattle(this);
	m_pParent->onCheckBulletHole(this);
}

/*virtual*/ void WWidgetBulletHole::release(int button)
{
}

// virtual
void WWidgetBulletHole::draw(WImage* pCanvas, int x, int y)
{
	int hw = m_rect.w / 2;
	int hh = m_rect.h / 2;
	float r = (float)std::min(hw, hh);
	pCanvas->circleFill(x + hw, y + hh, r, 0xffffffff);
	pCanvas->circle(x + hw, y + hh, r, 0xff808080);
	if(m_checked)
		pCanvas->circleFill(x + hw, y + hh, (r >= 4 ? r - 3 : r), 0xff000000);
}

void WWidgetBulletHole::setChecked(bool checked)
{
	m_checked = checked;
	m_pParent->tattle(this);
	if(checked)
		m_pParent->onCheckBulletHole(this);
}

// ----------------------------------------------------------------------

WWidgetBulletGroup::WWidgetBulletGroup(WWidgetGroup* pParent, int x, int y, int w, int h, int count, int interval, bool vertical)
: WWidgetGroup(pParent, x, y, (vertical ? w : (count - 1) * interval + w), (vertical ? (count - 1) * interval + h : h))
{
	m_nSelection = -1;
	int i;
	int xx = 0;
	int yy = 0;
	for(i = 0; i < count; i++)
	{
		new WWidgetBulletHole(this, xx, yy, w, h);
		if(vertical)
			yy += interval;
		else
			xx += interval;
	}
	if(count > 0)
	{
		WWidgetBulletHole* pFirst = (WWidgetBulletHole*)childWidget(0);
		pFirst->setChecked(true);
	}
}

// virtual
WWidgetBulletGroup::~WWidgetBulletGroup()
{
}

// virtual
void WWidgetBulletGroup::draw(WImage* pCanvas, int x, int y)
{
	int nCount = (int)m_widgets.size();
	WWidget* pChild;
	WRect* pRect;
	for(int n = 0; n < nCount; n++)
	{
		pChild = childWidget(n);
		pRect = pChild->rect();
		pChild->draw(pCanvas, x + pRect->x, y + pRect->y);
	}
	setClean();
}

// virtual
void WWidgetBulletGroup::onCheckBulletHole(WWidgetBulletHole* pBullet)
{
	m_nSelection = -1;
	int nCount = (int)m_widgets.size();
	WWidgetBulletHole* pChild;
	for(int n = 0; n < nCount; n++)
	{
		pChild = (WWidgetBulletHole*)childWidget(n);
		if(pChild == pBullet)
			m_nSelection = n;
		else
			pChild->setChecked(false);
	}
	WAssert(m_nSelection >= 0 || nCount == 0); // not one of my bullet holes
	m_pParent->tattle(this);
}

// virtual
void WWidgetBulletGroup::tattle(WWidget* pChild)
{
}

void WWidgetBulletGroup::setSelection(int n)
{
	WWidgetBulletHole* pChild = (WWidgetBulletHole*)childWidget(n);
	pChild->setChecked(true);
}

// ----------------------------------------------------------------------

WWidgetSliderTab::WWidgetSliderTab(WWidgetGroup* pParent, int x, int y, int w, int h, bool vertical, Style eStyle)
: WWidgetAtomic(pParent, x, y, w, h)
{
	m_vertical = vertical;
	m_eStyle = eStyle;
}

/*virtual*/ WWidgetSliderTab::~WWidgetSliderTab()
{
}

/*virtual*/ void WWidgetSliderTab::grab(int button, int x, int y)
{
	m_pParent->onClickTab(this);
}

/*virtual*/ void WWidgetSliderTab::release(int button)
{
}

/*virtual*/ void WWidgetSliderTab::onMouseMove(int dx, int dy)
{
	m_pParent->onSlideTab(this, dx, dy);
}

// virtual
void WWidgetSliderTab::draw(WImage* pCanvas, int x, int y)
{
	int i, j;
	if(m_rect.w <= 0 || m_rect.h <= 0)
		return;
	if(m_vertical)
	{
		switch(m_eStyle)
		{
			case ScrollBarTab:
				m_common->drawHorizCurvedOutSurface(pCanvas, x, y, m_rect.w, m_rect.h);
				break;
			case ScrollBarArea:
				m_common->drawHorizCurvedInSurface(pCanvas, x, y, m_rect.w, m_rect.h);
				break;
			case SliderNub:
				m_common->drawHorizCurvedOutSurface(pCanvas, x, y, m_rect.w, m_rect.h);
				for(i = m_rect.h / 2; i >= 0; i--)
				{
					for(j = m_rect.h / 2 - i; j >= 0; j--)
					{
						pCanvas->setPixel(x + m_rect.w - 1 - j, y + i, 0xff000000);
						pCanvas->setPixel(x + m_rect.w - 1 - j, y + m_rect.h - 1 - i, 0xff000000);
					}
				}
				break;
			case SliderArea:
				m_common->drawHorizCurvedInSurface(pCanvas, x + m_rect.w / 4, y, m_rect.w / 2, m_rect.h);
				break;
			default:
				WAssert(false); // Unexpected case
		}
	}
	else
	{
		switch(m_eStyle)
		{
			case ScrollBarTab:
				m_common->drawVertCurvedOutSurface(pCanvas, x, y, m_rect.w, m_rect.h);
				break;
			case ScrollBarArea:
				m_common->drawVertCurvedInSurface(pCanvas, x, y, m_rect.w, m_rect.h);
				break;
			case SliderNub:
				m_common->drawVertCurvedOutSurface(pCanvas, x, y, m_rect.w, m_rect.h);
				for(i = m_rect.w / 2; i >= 0; i--)
				{
					for(j = m_rect.w / 2 - i; j >= 0; j--)
					{
						pCanvas->setPixel(x + i, y + j, 0xff000000);
						pCanvas->setPixel(x + m_rect.w - 1 - i, y + j, 0xff000000);
					}
				}
				break;
			case SliderArea:
				m_common->drawVertCurvedInSurface(pCanvas, x, y + m_rect.h / 4, m_rect.w, m_rect.h / 2);
				break;
			default:
				WAssert(false); // Unexpected case
		}
	}
}

void WWidgetSliderTab::setSize(int w, int h)
{
	m_rect.w = w;
	m_rect.h = h;
	m_pParent->tattle(this);
}

// ----------------------------------------------------------------------

WWidgetHorizScrollBar::WWidgetHorizScrollBar(WWidgetGroup* pParent, int x, int y, int w, int h, int nViewSize, int nModelSize)
: WWidgetGroup(pParent, x, y, w, h)
{
	m_nViewSize = nViewSize;
	m_nModelSize = nModelSize;
	m_nPos = 0;
	int wid = buttonWidth();
	m_pLeftButton = new WWidgetVCRButton(this, 0, 0, wid, h, WWidgetVCRButton::ArrowLeft);
	m_pRightButton = new WWidgetVCRButton(this, m_rect.w - wid, 0, wid, h, WWidgetVCRButton::ArrowRight);
	m_pLeftTab = new WWidgetSliderTab(this, 0, 0, w, 0, false, WWidgetSliderTab::ScrollBarArea);
	m_pTab = new WWidgetSliderTab(this, 0, 0, w, 0, false, WWidgetSliderTab::ScrollBarTab);
	m_pRightTab = new WWidgetSliderTab(this, 0, 0, w, 0, false, WWidgetSliderTab::ScrollBarArea);
}

/*virtual*/ WWidgetHorizScrollBar::~WWidgetHorizScrollBar()
{
}

int WWidgetHorizScrollBar::buttonWidth()
{
	if((m_rect.w >> 2) < m_rect.h)
		return (m_rect.w >> 2);
	else
		return m_rect.h;
}

// virtual
void WWidgetHorizScrollBar::draw(WImage* pCanvas, int x, int y)
{
	// Calculations
	if(m_nModelSize < m_nViewSize)
		m_nModelSize = m_nViewSize;
	int wid = m_rect.w;
	int hgt = m_rect.h;
	int nButtonSize = buttonWidth();
	int nSlideAreaSize = wid - nButtonSize - nButtonSize;
	int nTabSize = nSlideAreaSize * m_nViewSize / m_nModelSize;
	if(nTabSize < hgt)
		nTabSize = hgt;
	if(nTabSize > nSlideAreaSize)
		nTabSize = nSlideAreaSize;
	if(m_nPos < 0)
		m_nPos = 0;
	int nTabPos = m_nPos * nSlideAreaSize / m_nModelSize;
	if(nTabPos > nSlideAreaSize - nTabSize)
		nTabPos = nSlideAreaSize - nTabSize;
	nTabPos += nButtonSize;

	// Draw the three tab areas
	m_pLeftTab->setPos(nButtonSize, 0);
	m_pLeftTab->setSize(nTabPos - nButtonSize, m_rect.h);
	m_pTab->setPos(nTabPos, 0);
	m_pTab->setSize(nTabSize, m_rect.h);
	m_pRightTab->setPos(nTabPos + nTabSize, 0);
	m_pRightTab->setSize(nSlideAreaSize + nButtonSize - (nTabPos + nTabSize), m_rect.h);
	WRect* pRect;
	pRect = m_pLeftTab->rect();
	m_pLeftTab->draw(pCanvas, x + pRect->x, y + pRect->y);
	pRect = m_pTab->rect();
	m_pTab->draw(pCanvas, x + pRect->x, y + pRect->y);
	pRect = m_pRightTab->rect();
	m_pRightTab->draw(pCanvas, x + pRect->x, y + pRect->y);

	// Draw the buttons
	pRect = m_pLeftButton->rect();
	m_pLeftButton->draw(pCanvas, x + pRect->x, y + pRect->y);
	pRect = m_pRightButton->rect();
	m_pRightButton->draw(pCanvas, x + pRect->x, y + pRect->y);
	setClean();
}

/*virtual*/ void WWidgetHorizScrollBar::onPushVCRButton(WWidgetVCRButton* pButton)
{
	if(pButton == m_pLeftButton)
	{
		m_nPos -= m_nViewSize / 5;
		if(m_nPos < 0)
			m_nPos = 0;
	}
	else
	{
		WAssert(pButton == m_pRightButton); // unexpected button
		m_nPos += m_nViewSize / 5;
		if(m_nPos > m_nModelSize - m_nViewSize)
			m_nPos = m_nModelSize - m_nViewSize;
	}
	m_pParent->tattle(this);
	m_pParent->onHorizScroll(this);
}

/*virtual*/ void WWidgetHorizScrollBar::onClickTab(WWidgetSliderTab* pTab)
{
	if(pTab == m_pLeftTab)
	{
		m_nPos -= m_nViewSize;
		if(m_nPos < 0)
			m_nPos = 0;
		m_pParent->tattle(this);
		m_pParent->onHorizScroll(this);
	}
	else if(pTab == m_pRightTab)
	{
		m_nPos += m_nViewSize;
		if(m_nPos > m_nModelSize - m_nViewSize)
			m_nPos = m_nModelSize - m_nViewSize;
		m_pParent->tattle(this);
		m_pParent->onHorizScroll(this);
	}
}

/*virtual*/ void WWidgetHorizScrollBar::onSlideTab(WWidgetSliderTab* pTab, int dx, int dy)
{
	if(pTab != m_pTab)
		return;
	int wid = m_rect.w - 2 * buttonWidth();
	int nDelta = dx * m_nModelSize / wid;
	if(dx != 0 && nDelta == 0)
		nDelta = WBits::sign(dx);
	m_nPos += nDelta;
	if(m_nPos < 0)
		m_nPos = 0;
	else if(m_nPos > m_nModelSize - m_nViewSize)
		m_nPos = m_nModelSize - m_nViewSize;
	m_pParent->tattle(this);
	m_pParent->onHorizScroll(this);
}

void WWidgetHorizScrollBar::setViewSize(int n)
{
	m_nViewSize = n;
	m_pParent->tattle(this);
}

void WWidgetHorizScrollBar::setModelSize(int n)
{
	m_nModelSize = n;
	m_pParent->tattle(this);
}

void WWidgetHorizScrollBar::setPos(int n)
{
	m_nPos = n;
	m_pParent->tattle(this);
	m_pParent->onHorizScroll(this);
}

// ----------------------------------------------------------------------

WWidgetVertScrollBar::WWidgetVertScrollBar(WWidgetGroup* pParent, int x, int y, int w, int h, int nViewSize, int nModelSize)
: WWidgetGroup(pParent, x, y, w, h)
{
	m_nViewSize = nViewSize;
	m_nModelSize = nModelSize;
	m_nPos = 0;
	int hgt = buttonHeight();
	m_pUpButton = new WWidgetVCRButton(this, 0, 0, w, hgt, WWidgetVCRButton::ArrowUp);
	m_pDownButton = new WWidgetVCRButton(this, 0, m_rect.h - hgt, w, hgt, WWidgetVCRButton::ArrowDown);
	m_pAboveTab = new WWidgetSliderTab(this, 0, 0, w, 0, true, WWidgetSliderTab::ScrollBarArea);
	m_pTab = new WWidgetSliderTab(this, 0, 0, w, 0, true, WWidgetSliderTab::ScrollBarTab);
	m_pBelowTab = new WWidgetSliderTab(this, 0, 0, w, 0, true, WWidgetSliderTab::ScrollBarArea);
}

/*virtual*/ WWidgetVertScrollBar::~WWidgetVertScrollBar()
{
}

int WWidgetVertScrollBar::buttonHeight()
{
	if((m_rect.h >> 2) < m_rect.w)
		return (m_rect.h >> 2);
	else
		return m_rect.w;
}

// virtual
void WWidgetVertScrollBar::draw(WImage* pCanvas, int x, int y)
{
	// Calculations
	if(m_nModelSize < m_nViewSize)
		m_nModelSize = m_nViewSize;
	int wid = m_rect.w;
	int hgt = m_rect.h;
	int nButtonSize = buttonHeight();
	int nSlideAreaSize = hgt - nButtonSize - nButtonSize;
	int nTabSize = nSlideAreaSize * m_nViewSize / m_nModelSize;
	if(nTabSize < wid)
		nTabSize = wid;
	if(nTabSize > nSlideAreaSize)
		nTabSize = nSlideAreaSize;
	if(m_nPos < 0)
		m_nPos = 0;
	int nTabPos = m_nPos * nSlideAreaSize / m_nModelSize;
	if(nTabPos > nSlideAreaSize - nTabSize)
		nTabPos = nSlideAreaSize - nTabSize;
	nTabPos += nButtonSize;

	// Draw the three tab areas
	m_pAboveTab->setPos(0, nButtonSize);
	m_pAboveTab->setSize(m_rect.w, nTabPos - nButtonSize);
	m_pTab->setPos(0, nTabPos);
	m_pTab->setSize(m_rect.w, nTabSize);
	m_pBelowTab->setPos(0, nTabPos + nTabSize);
	m_pBelowTab->setSize(m_rect.w, nSlideAreaSize + nButtonSize - (nTabPos + nTabSize));
	WRect* pRect;
	pRect = m_pAboveTab->rect();
	m_pAboveTab->draw(pCanvas, x + pRect->x, y + pRect->y);
	pRect = m_pTab->rect();
	m_pTab->draw(pCanvas, x + pRect->x, y + pRect->y);
	pRect = m_pBelowTab->rect();
	m_pBelowTab->draw(pCanvas, x + pRect->x, y + pRect->y);

	// Draw the buttons
	pRect = m_pUpButton->rect();
	m_pUpButton->draw(pCanvas, x + pRect->x, y + pRect->y);
	pRect = m_pDownButton->rect();
	m_pDownButton->draw(pCanvas, x + pRect->x, y + pRect->y);
	setClean();
}

/*virtual*/ void WWidgetVertScrollBar::onPushVCRButton(WWidgetVCRButton* pButton)
{
	if(pButton == m_pUpButton)
	{
		m_nPos -= m_nViewSize / 5;
		if(m_nPos < 0)
			m_nPos = 0;
	}
	else
	{
		WAssert(pButton == m_pDownButton); // unexpected button
		m_nPos += m_nViewSize / 5;
		if(m_nPos > m_nModelSize - m_nViewSize)
			m_nPos = m_nModelSize - m_nViewSize;
	}
	m_pParent->tattle(this);
	m_pParent->onVertScroll(this);
}

/*virtual*/ void WWidgetVertScrollBar::onClickTab(WWidgetSliderTab* pTab)
{
	if(pTab == m_pAboveTab)
	{
		m_nPos -= m_nViewSize;
		if(m_nPos < 0)
			m_nPos = 0;
		m_pParent->tattle(this);
		m_pParent->onVertScroll(this);
	}
	else if(pTab == m_pBelowTab)
	{
		m_nPos += m_nViewSize;
		if(m_nPos > m_nModelSize - m_nViewSize)
			m_nPos = m_nModelSize - m_nViewSize;
		m_pParent->tattle(this);
		m_pParent->onVertScroll(this);
	}
}

/*virtual*/ void WWidgetVertScrollBar::onSlideTab(WWidgetSliderTab* pTab, int dx, int dy)
{
	if(pTab != m_pTab)
		return;
	int hgt = m_rect.h - 2 * buttonHeight();
	int nDelta = dy * m_nModelSize / hgt;
	if(dy != 0 && nDelta == 0)
		nDelta = WBits::sign(dy);
	m_nPos += nDelta;
	if(m_nPos < 0)
		m_nPos = 0;
	else if(m_nPos > m_nModelSize - m_nViewSize)
		m_nPos = m_nModelSize - m_nViewSize;
	m_pParent->tattle(this);
	m_pParent->onVertScroll(this);
}

void WWidgetVertScrollBar::setViewSize(int n)
{
	m_nViewSize = n;
	m_pParent->tattle(this);
}

void WWidgetVertScrollBar::setModelSize(int n)
{
	m_nModelSize = n;
	m_pParent->tattle(this);
}

void WWidgetVertScrollBar::setPos(int n)
{
	m_nPos = n;
	m_pParent->tattle(this);
	m_pParent->onVertScroll(this);
}

// ----------------------------------------------------------------------

WWidgetTextBox::WWidgetTextBox(WWidgetGroup* pParent, int x, int y, int w, int h)
: WWidgetAtomic(pParent, x, y, w, h)
{
	m_bGotFocus = false;
	m_bPassword = false;
	m_nAnchorPos = 0;
	m_nCursorPos = 0;
	m_nMouseDelta = 0;
	m_cBackground = 0xff0000ff;
}

/*virtual*/ WWidgetTextBox::~WWidgetTextBox()
{
}

void WWidgetTextBox::setText(const char* szText)
{
	m_text = szText;
	m_nAnchorPos = (int)m_text.length();
	m_nCursorPos = m_nAnchorPos;
	m_pParent->tattle(this);
	m_pParent->onTextBoxTextChanged(this);
}

void WWidgetTextBox::setText(int nValue)
{
	std::ostringstream os;
	os << nValue;
	m_text = os.str();
}

void WWidgetTextBox::setText(double dValue)
{
	std::ostringstream os;
	os.precision(14);
	os << dValue;
	m_text = os.str();
}

// virtual
void WWidgetTextBox::draw(WImage* pCanvas, int x, int y)
{
	// Draw the background area
	int w = m_rect.w;
	int h = m_rect.h;
	m_common->drawVertCurvedInSurface(pCanvas, x, y, w, h, m_cBackground);
	pCanvas->box(x, y, x + w - 1, y + h - 1, m_bGotFocus ? 0xffffffff : 0xff000000);

	// Draw the text
	int nTempBufLen = (int)m_text.length() + 1;
	WTEMPBUF(char, szText, nTempBufLen);
	strcpy(szText, m_text.c_str());
	float fontSize = (float)(h - 4) / 12;
	int xx = x + 1;
	int yy = y + 3;
	if(m_bPassword)
	{
		int i;
		for(i = 0; szText[i] != '\0'; i++)
			szText[i] = '#';
	}
	pCanvas->text(szText, xx, yy, fontSize, 0xffffffff, x + w - xx, y + h - yy);

	// Draw the cursor or selection
	if(!m_bGotFocus)
		return; // don't waste time drawing the cursor for inactive text boxes
	int nSelStart = m_nAnchorPos;
	int nSelEnd = m_nCursorPos;
	if(nSelEnd < nSelStart)
	{
		int nTmp = nSelEnd;
		nSelEnd = nSelStart;
		nSelStart = nTmp;
	}
	szText[nSelEnd] = '\0';
	int nSelEndPos = pCanvas->measureTextWidth(szText, fontSize);
	if(nSelEndPos > w - 3)
		nSelEndPos = w - 3;
	if(nSelStart == nSelEnd)
		m_common->drawCursor(pCanvas, x + nSelEndPos, y + 2, 2, h - 5);
	else
	{
		szText[nSelStart] = '\0';
		int nSelStartPos = pCanvas->measureTextWidth(szText, fontSize);
		WRect r;
		r.x = x + nSelStartPos;
		r.y = yy;
		r.w = nSelEndPos - nSelStartPos;
		r.h = h - 4;
		pCanvas->invertRect(&r);
	}
}

void WWidgetTextBox::setColor(unsigned int c)
{
	m_cBackground = c;
	m_pParent->tattle(this);
}

/*virtual*/ void WWidgetTextBox::onChar(char c)
{
	if(c == '\b')
	{
		if(m_nAnchorPos == m_nCursorPos)
		{
			if(m_nAnchorPos <= 0)
				return;
			m_nAnchorPos--;
		}
		if(m_nCursorPos < m_nAnchorPos)
		{
			int nTmp = m_nCursorPos;
			m_nCursorPos = m_nAnchorPos;
			m_nAnchorPos = nTmp;
		}
		m_text.erase(m_nAnchorPos, m_nCursorPos - m_nAnchorPos);
		m_nCursorPos = m_nAnchorPos;
	}
	else if(c == '\r')
	{
		m_pParent->onTextBoxPressEnter(this);
		return;
	}
	else if(c == 127) // delete key
	{
		if(m_nAnchorPos == m_nCursorPos)
		{
			if(m_nCursorPos >= (int)m_text.length())
				return;
			m_nCursorPos++;
		}
		if(m_nCursorPos < m_nAnchorPos)
		{
			int nTmp = m_nCursorPos;
			m_nCursorPos = m_nAnchorPos;
			m_nAnchorPos = nTmp;
		}
		m_text.erase(m_nAnchorPos, m_nCursorPos - m_nAnchorPos);
		m_nCursorPos = m_nAnchorPos;
	}
	else
	{
		if(m_nAnchorPos != m_nCursorPos)
		{
			if(m_nCursorPos < m_nAnchorPos)
			{
				int nTmp = m_nCursorPos;
				m_nCursorPos = m_nAnchorPos;
				m_nAnchorPos = nTmp;
			}
			m_text.erase(m_nAnchorPos, m_nCursorPos - m_nAnchorPos);
			m_nCursorPos = m_nAnchorPos;
		}
		if(m_nCursorPos >= (int)m_text.length())
			m_text += c;
		else
			m_text.insert(m_nCursorPos, 1, c);
		m_nCursorPos++;
		m_nAnchorPos++;
	}
	m_pParent->tattle(this);
	m_pParent->onTextBoxTextChanged(this);
}

/*virtual*/ void WWidgetTextBox::onSpecialKey(int key)
{
	switch(key)
	{
		case 276: // left
			if(m_nCursorPos > 0)
			{
				m_nCursorPos--;
				m_nAnchorPos = m_nCursorPos;
			}
			break;
		case 275: // right
			if(m_nCursorPos < (int)m_text.length())
			{
				m_nCursorPos++;
				m_nAnchorPos = m_nCursorPos;
			}
			break;
		case 278: // home
			m_nCursorPos = 0;
			m_nAnchorPos = m_nCursorPos;
			break;
		case 279: // end
			m_nCursorPos = (int)m_text.length();
			m_nAnchorPos = m_nCursorPos;
			break;
	}
	m_pParent->tattle(this);
}

void WWidgetTextBox::SetSelection(int anchorPos, int cursorPos)
{
	if(anchorPos > cursorPos)
		std::swap(anchorPos, cursorPos);
	m_nAnchorPos = std::max(0, std::min((int)m_text.length(), anchorPos));
	m_nCursorPos = std::max(0, std::min((int)m_text.length(), cursorPos));
}

/*virtual*/ void WWidgetTextBox::grab(int button, int x, int y)
{
	m_nMouseDelta = 0;
	float fontSize = (float)(m_rect.h - 4) / 12;
	m_nAnchorPos = WImage::countTextChars(x - 1 + 3, m_text.c_str(), fontSize);
	m_nCursorPos = m_nAnchorPos;
	m_pParent->tattle(this);
}

/*virtual*/ void WWidgetTextBox::release(int button)
{
}

/*virtual*/ void WWidgetTextBox::onMouseMove(int dx, int dy)
{
	m_nMouseDelta += dx;
	int nNewCursorPos = m_nAnchorPos + m_nMouseDelta / 6;
	if(nNewCursorPos < 0)
		nNewCursorPos = 0;
	if(nNewCursorPos > (int)m_text.length())
		nNewCursorPos = (int)m_text.length();
	if(nNewCursorPos != m_nCursorPos)
	{
		m_nCursorPos = nNewCursorPos;
		m_pParent->tattle(this);
	}
}

/*virtual*/ void WWidgetTextBox::onGetFocus()
{
	m_bGotFocus = true;
	m_pParent->tattle(this);
}

/*virtual*/ void WWidgetTextBox::onLoseFocus()
{
	m_bGotFocus = false;
	m_pParent->tattle(this);
}

// ----------------------------------------------------------------------

WWidgetGrid::WWidgetGrid(WWidgetGroup* pParent, int nColumns, int x, int y, int w, int h, unsigned int cBackground)
: WWidgetGroup(pParent, x, y, w, h)
{
	m_cBackground = cBackground;
	m_nColumns = 0;
	m_pColumnHeaders = nullptr;
	m_nColumnWidths = nullptr;
	m_nRowHeight = 12;
	m_nHeaderHeight = 12;
	int nScrollBarSize = 16;
	m_pVertScrollBar = nullptr;
	m_pHorizScrollBar = nullptr;
	m_pVertScrollBar = new WWidgetVertScrollBar(this, w - nScrollBarSize, 0, nScrollBarSize, h, h - m_nHeaderHeight - nScrollBarSize, h);
	m_pHorizScrollBar = new WWidgetHorizScrollBar(this, 0, h - nScrollBarSize, w - nScrollBarSize, nScrollBarSize, w - nScrollBarSize, 80 * nColumns);
	setColumnCount(nColumns);
}

/*virtual*/ WWidgetGrid::~WWidgetGrid()
{
	delete[] m_pColumnHeaders;
	delete[] m_nColumnWidths;
	for(size_t i = 0; i < m_rows.size(); i++)
		delete[] m_rows[i];
}

int WWidgetGrid::rowCount()
{
	return (int)m_rows.size();
}

void WWidgetGrid::setRowHeight(int n)
{
	m_nRowHeight = n;
	m_pParent->tattle(this);
}

void WWidgetGrid::setHeaderHeight(int n)
{
	m_nHeaderHeight = n;
	m_pVertScrollBar->setViewSize(m_rect.h - 16 - n);
	m_pParent->tattle(this);
}

void WWidgetGrid::setHScrollPos(int n)
{
	m_pHorizScrollBar->setPos(n);
	m_pParent->tattle(this);
}

void WWidgetGrid::setVScrollPos(int n)
{
	m_pVertScrollBar->setPos(n);
	m_pParent->tattle(this);
}

void WWidgetGrid::addBlankRow()
{
	WWidget** pNewRow = new WWidget*[m_nColumns];
	int n;
	for(n = 0; n < m_nColumns; n++)
		pNewRow[n] = nullptr;
	m_rows.push_back(pNewRow);
	m_pParent->tattle(this);
}

WWidget* WWidgetGrid::widget(int col, int row)
{
	WAssert(col >= 0 && col < m_nColumns); // out of range
	WWidget** pRow = m_rows[row];
	return pRow[col];
}

void WWidgetGrid::setWidget(int col, int row, WWidget* pWidget)
{
	if(pWidget->m_pParent == nullptr)
	{
		pWidget->m_pParent = this;
		addWidget(pWidget);
	}
	else if(pWidget->m_pParent != this)
		throw Ex("That widget is already a child of another group widget");
	if(col < 0 || col >= m_nColumns)
		throw Ex("out of range");
	while(row >= (int)m_rows.size())
		addBlankRow();
	WWidget** pRow = m_rows[row];
	pRow[col] = pWidget;
	int nColPos = 0;
	int n;
	for(n = 0; n < col; n++)
		nColPos += m_nColumnWidths[n];
	pWidget->setPos(nColPos, m_nHeaderHeight + row * m_nRowHeight);
	m_pParent->tattle(this);
}

void WWidgetGrid::setColumnCount(int n)
{
	// Replace the column headers
	int nScrollBarSize = 16;
	int nDefaultColumnWidth = std::max((m_rect.w - nScrollBarSize) / n, 50);
	WWidget** pNewColumnHeaders = new WWidget*[n];
	int* pNewColumnWidths = new int[n];
	int i;
	for(i = 0; i < n && i < m_nColumns; i++)
	{
		pNewColumnHeaders[i] = m_pColumnHeaders[i];
		pNewColumnWidths[i] = m_nColumnWidths[i];
	}
	for(; i < n; i++)
	{
		pNewColumnHeaders[i] = nullptr;
		pNewColumnWidths[i] = nDefaultColumnWidth;
	}
	delete[] m_pColumnHeaders;
	delete[] m_nColumnWidths;
	m_pColumnHeaders = pNewColumnHeaders;
	m_nColumnWidths = pNewColumnWidths;

	// Replace the rows
	for(size_t j = 0; j < m_rows.size(); j++)
	{
		WWidget** pOldRow = m_rows[j];
		WWidget** pNewRow = new WWidget*[n];
		for(i = 0; i < n && i < m_nColumns; i++)
			pNewRow[i] = pOldRow[i];
		for(; i < n; i++)
			pNewRow[i] = nullptr;
		delete[] pOldRow;
		m_rows[j] = pNewRow;
	}

	// Change the column count and notify the parent
	m_nColumns = n;
	m_pParent->tattle(this);
}

WWidget* WWidgetGrid::columnHeader(int col)
{
	WAssert(col >= 0 && col < m_nColumns); // out of range
	return m_pColumnHeaders[col];
}

void WWidgetGrid::setColumnHeader(int col, WWidget* pWidget)
{
	WAssert(col >= 0 && col < m_nColumns); // out of range
	// todo: do we need to do anything with the widget that used to be here?
	m_pColumnHeaders[col] = pWidget;
	int nColPos = 0;
	int n;
	for(n = 0; n < col; n++)
		nColPos += m_nColumnWidths[n];
	pWidget->setPos(nColPos, 0);
	tattle(pWidget);
}

int WWidgetGrid::columnWidth(int col)
{
	WAssert(col >= 0 && col < m_nColumns); // out of range
	return m_nColumnWidths[col];
}

void WWidgetGrid::setColumnWidth(int col, int nWidth)
{
	WAssert(col >= 0 && col < m_nColumns); // out of range
	m_nColumnWidths[col] = nWidth;
	m_pParent->tattle(this);
}

// virtual
void WWidgetGrid::tattle(WWidget* pChild)
{
	// Swallow tattling from scroll bars since WWidgetGrid::Draw
	// triggers them to change
	if(pChild == m_pVertScrollBar || pChild == m_pHorizScrollBar)
		return;
	WWidgetGroup::tattle(pChild);
}

// virtual
void WWidgetGrid::draw(WImage* pCanvas, int x, int y)
{
	// Clear
	int w = m_rect.w - 16;
	int h = m_rect.h - 16;
	pCanvas->boxFill(x, y, w, h, m_cBackground);
	WRect rClipHeader(x, y, w, h);
	WRect rClipBody(x, y + m_nHeaderHeight, w, h - m_nHeaderHeight);

	// Draw columns
	int nColumnPos, nColumnWidth, nVertPos;
	WWidget* pWidget;
	WRect r;
	int nHScrollPos = m_pHorizScrollBar->pos();
	int nVScrollPos = m_pVertScrollBar->pos();
	int col, row;
	nColumnPos = -nHScrollPos;
	for(col = 0; col < m_nColumns && nColumnPos < w; col++)
	{
		nColumnWidth = m_nColumnWidths[col];
		if(nColumnPos + nColumnWidth > 0)
		{
			// Draw the header widget
			pWidget = m_pColumnHeaders[col];
			if(pWidget)
				m_common->drawClipped(pCanvas, x + nColumnPos, y, pWidget, &rClipHeader);

			// Draw all the widgets in the column
			row = nVScrollPos / m_nRowHeight;
			nVertPos = m_nHeaderHeight - (nVScrollPos % m_nRowHeight);
			while(row < (int)m_rows.size() && nVertPos < h)
			{
				pWidget = (m_rows[row])[col];
				if(pWidget)
					m_common->drawClipped(pCanvas, x + nColumnPos, y + nVertPos, pWidget, &rClipBody);
				nVertPos += m_nRowHeight;
				row++;
			}
		}
		nColumnPos += nColumnWidth;
	}
	while(col < m_nColumns)
	{
		nColumnPos += m_nColumnWidths[col];
		col++;
	}

	// Draw the scroll bars
	m_pVertScrollBar->setModelSize((int)m_rows.size() * m_nRowHeight);
	m_pVertScrollBar->draw(pCanvas, x + w, y);
	m_pHorizScrollBar->setModelSize(nColumnPos + nHScrollPos);
	m_pHorizScrollBar->draw(pCanvas, x, y + h);
	setClean();
}

void WWidgetGrid::flushItems(bool deleteWidgets)
{
	bool bCol;
	for(size_t n = 0; n < m_widgets.size(); n++)
	{
		WWidget* pWidget = m_widgets[n];
		if(pWidget == m_pVertScrollBar)
			continue;
		if(pWidget == m_pHorizScrollBar)
			continue;
		bCol = false;
		for(int i = 0; i < m_nColumns; i++)
		{
			if(m_pColumnHeaders[i] == pWidget)
			{
				bCol = true;
				break;
			}
		}
		if(bCol)
			continue;
		if(deleteWidgets)
			delete(pWidget);
		else
		{
			onDestroyWidget(pWidget);
			pWidget->m_pParent = nullptr;
		}
		n--;
	}
	int nCount = (int)m_rows.size();
	for(int n = 0; n < nCount; n++)
		delete[] m_rows[n];
	m_rows.clear();

	m_pParent->tattle(this);
}

/*virtual*/ void WWidgetGrid::onVertScroll(WWidgetVertScrollBar* pScrollBar)
{
	m_pParent->tattle(this);
}

/*virtual*/ void WWidgetGrid::onHorizScroll(WWidgetHorizScrollBar* pScrollBar)
{
	m_pParent->tattle(this);
}

/*virtual*/ WWidgetAtomic* WWidgetGrid::findAtomicWidget(int x, int y)
{
	WRect* pRect = m_pVertScrollBar->rect();
	if(x >= pRect->x)
		return m_pVertScrollBar->findAtomicWidget(x - pRect->x, y - pRect->y);
	pRect = m_pHorizScrollBar->rect();
	if(y >= pRect->y)
		return m_pHorizScrollBar->findAtomicWidget(x - pRect->x, y - pRect->y);
	int xOrig = x;
	int yOrig = y;
	x += m_pHorizScrollBar->pos();
	WWidget** pRow;
	if(y < m_nHeaderHeight)
		pRow = m_pColumnHeaders;
	else
	{
		y += m_pVertScrollBar->pos();
		y -= m_nHeaderHeight;
		y /= m_nRowHeight;
		if(y >= 0 && y < (int)m_rows.size())
			pRow = m_rows[y];
		else
			return nullptr;
	}
	WWidget* pWidget = nullptr;
	int nColLeft = 0;
	int n;
	for(n = 0; n < m_nColumns; n++)
	{
		nColLeft += m_nColumnWidths[n];
		if(nColLeft > x)
		{
			pWidget = pRow[n];
			break;
		}
	}
	if(pWidget)
	{
		if(pWidget->isAtomic())
			return (WWidgetAtomic*)pWidget;
		else
		{
			WRect* pRectangle = pWidget->rect();
			return ((WWidgetGroup*)pWidget)->findAtomicWidget(xOrig - pRectangle->x, yOrig - pRectangle->y);
		}
	}
	return nullptr;
}

// ----------------------------------------------------------------------

WWidgetFileSystemBrowser::WWidgetFileSystemBrowser(WWidgetGroup* pParent, int x, int y, int w, int h, const char* szExtensions)
: WWidgetGroup(pParent, x, y, w, h)
{
	int nPathHeight = 12;
	m_pPath = new WWidgetTextLabel(this, 0, 0, w, nPathHeight, "", 0xff8888ff);
	m_pPath->setBackgroundColor(0xff000000);
	m_pFiles = new WWidgetGrid(this, 3, 0, nPathHeight, w, h - nPathHeight);

	// Column Headers
	m_pFiles->setColumnWidth(0, 100);
	m_pFiles->setColumnWidth(1, 100);
	m_pFiles->setColumnWidth(2, 100);
	/*
	WWidgetTextButton* pButton;
	pButton = new WWidgetTextButton(m_pFiles, 0, 0, 300, 20, "");
	m_pFiles->SetColumnHeader(0, pButton);
	pButton = new WWidgetTextButton(m_pFiles, 0, 0, 50, 20, "");
	m_pFiles->SetColumnHeader(1, pButton);
	pButton = new WWidgetTextButton(m_pFiles, 0, 0, 50, 20, "");
	m_pFiles->SetColumnHeader(2, pButton);
*/
	// Extension
	int nExtLen = 0;
	if(szExtensions)
		nExtLen = (int)strlen(szExtensions);
	if(nExtLen > 0)
	{
		m_szExtensions = new char[nExtLen + 1];
		strcpy(m_szExtensions, szExtensions);
	}
	else
		m_szExtensions = nullptr;

	m_bFileListDirty = true;
}

/*virtual*/ WWidgetFileSystemBrowser::~WWidgetFileSystemBrowser()
{
	delete[] m_szExtensions;
}

bool CheckExtensionList(const char* szExt, const char* szList)
{
	int nExtLen = (int)strlen(szExt);
	while(true)
	{
		if(_strnicmp(szExt, szList, nExtLen) == 0)
		{
			if(szList[nExtLen] == '\0' || szList[nExtLen] == ',' || szList[nExtLen] == ';' || szList[nExtLen] == ':')
				return true;
		}
		szList++;
		while(*szList != '\0' && *szList != ',' && *szList != ';' && *szList != ':')
			szList++;
		if(*szList == '\0')
			break;
		szList++;
	}
	return false;
}

bool WSortByCaseInsensitiveStringComparer(const string& a, const string& b)
{
	return _stricmp(a.c_str(), b.c_str()) < 0;
}

#define MIN_FILES_BROWSER_COL_WIDTH 150

// virtual
void WWidgetFileSystemBrowser::dirFoldersAndFiles(string* pOutDir, vector<string>* pOutFolders, vector<string>* pOutFiles)
{
	// Get the path
	char szPath[300];
	if(!getcwd(szPath, 300))
		throw Ex("error getting cwd");
	*pOutDir = szPath;
	(*pOutDir) += '/';

	// Get the folders
	if(pOutDir->length() >
#ifdef WINDOWS
	   4)
#else
	   2)
#endif
		pOutFolders->push_back("..");
	WFile::folderList(*pOutFolders);

	// Get the files
	{
		vector<string> files;
		WFile::fileList(files);
		for(vector<string>::iterator it = files.begin(); it != files.end(); it++)
		{
			const char* szFilename = it->c_str();
			if(m_szExtensions)
			{
				PathData pd;
				WFile::parsePath(szFilename, &pd);
				WAssert(pd.extStart >= 0); // out of range
				if(!CheckExtensionList(&szFilename[pd.extStart], m_szExtensions))
					continue;
			}
			pOutFiles->push_back(szFilename);
		}
	}
}

void WWidgetFileSystemBrowser::reloadFileList()
{
	m_bFileListDirty = false;
	int nScrollBarSize = 16;
	m_pFiles->flushItems();
	vector<string> folders;
	vector<string> files;
	dirFoldersAndFiles(&m_path, &folders, &files);
	m_pPath->setText(m_path.c_str());
	std::sort(folders.begin(), folders.end(), WSortByCaseInsensitiveStringComparer);
	std::sort(files.begin(), files.end(), WSortByCaseInsensitiveStringComparer);

	// Make the columns
	int totalSize = (int)folders.size() + (int)files.size();
	int displayrows = (m_rect.h - nScrollBarSize) / m_pFiles->rowHeight() - 1;
	int cols = std::max(1, std::min((m_rect.w - nScrollBarSize) / MIN_FILES_BROWSER_COL_WIDTH, (totalSize + displayrows - 1) / displayrows));
	int colwid = (m_rect.w - nScrollBarSize) / cols;
	int rows = totalSize / cols;
	if((rows * cols) < totalSize)
		rows++;

	// Make the widgets
	m_pFiles->setColumnCount(cols);
	int c, r;
	size_t i = 0;
	unsigned int col;
	const char* szFilename;
	for(c = 0; c < cols; c++)
	{
		m_pFiles->setColumnWidth(c, colwid);
		for(r = 0; r < rows; r++)
		{
			if((int)i >= totalSize)
				break;
			if(m_pFiles->rowCount() <= r)
				m_pFiles->addBlankRow();
			if(i < folders.size())
				szFilename = folders[i].c_str();
			else
				szFilename = files[i - folders.size()].c_str();
			if(szFilename[0] == '.')
			{
				if(i < folders.size())
					col = 0xff4488aa;
				else
					col = 0xffaaaaaa;
			}
			else
			{
				if(i < folders.size())
					col = 0xff44aaff;
				else
					col = 0xffffffff;
			}
			WWidgetTextLabel* pLabel = new WWidgetTextLabel(m_pFiles, 0, 0, colwid, m_pFiles->rowHeight(), szFilename, col, 0xff003000);
			m_pFiles->setWidget(c, r, pLabel);
			i++;
		}
	}
}

/*virtual*/ void WWidgetFileSystemBrowser::onClickTextLabel(WWidgetTextLabel* pLabel)
{
	int nPathLen = (int)m_path.length();
	const string& text = pLabel->text();
	int nTempBufLen = nPathLen + (int)text.length() + 1;
	WTEMPBUF(char, szFilename, nTempBufLen);
	strcpy(szFilename, m_path.c_str());
	strcpy(szFilename + nPathLen, text.c_str());
	if(chdir(szFilename) == 0)
	{
		m_pFiles->setVScrollPos(0);
		m_bFileListDirty = true;
		m_pParent->tattle(this);
	}
	else
	{
		if(m_pParent)
			m_pParent->onSelectFilename(this, szFilename);
	}
}

// virtual
void WWidgetFileSystemBrowser::draw(WImage* pCanvas, int x, int y)
{
	if(m_bFileListDirty)
		reloadFileList();
	vector<WWidget*>* pDirtyChildren = &m_dirtyChildren;
	if(getDirtyBit((int)m_widgets.size()))
	{
		setDirtyBit((int)m_widgets.size(), false);
		pDirtyChildren = &m_widgets;
		pCanvas->boxFill(x, y, m_rect.w, m_rect.h, 0);
	}
	int n;
	int nCount = (int)pDirtyChildren->size();
	WWidget* pWidget;
	WRect* pRect;
	for(n = 0; n < nCount; n++)
	{
		pWidget = (*pDirtyChildren)[n];
		pRect = pWidget->rect();
		pWidget->draw(pCanvas, x + pRect->x, y + pRect->y);
	}
	m_dirtyChildren.clear();
	nCount = (int)m_dirtyBits.size();
	for(n = 0; n < nCount; n++)
		m_dirtyBits[n] = 0;
	setClean();
}

// ----------------------------------------------------------------------

WWidgetHorizSlider::WWidgetHorizSlider(WWidgetGroup* pParent, int x, int y, int w, int h)
: WWidgetGroup(pParent, x, y, w, h)
{
	m_dirty = true;
	m_fPos = .5;
	m_pLeftTab = new WWidgetSliderTab(this, 0, 0, 0, h, false, WWidgetSliderTab::SliderArea);
	m_pTab = new WWidgetSliderTab(this, 0, 0, 0, h, false, WWidgetSliderTab::SliderNub);
	m_pRightTab = new WWidgetSliderTab(this, 0, 0, 0, h, false, WWidgetSliderTab::SliderArea);
}

/*virtual*/ WWidgetHorizSlider::~WWidgetHorizSlider()
{
}

void WWidgetHorizSlider::setPos(float f)
{
	m_fPos = f;
	m_dirty = true;
	m_pParent->tattle(this);
	m_pParent->onHorizSliderMove(this);
}

// virtual
void WWidgetHorizSlider::draw(WImage* pCanvas, int x, int y)
{
	// Calculations
	int wid = m_rect.w;
	int hgt = m_rect.h;
	WAssert(wid > hgt); // disproportioned horizontal slider
	int nTabSize = hgt / 2;
	if(m_fPos < 0)
		m_fPos = 0;
	else if(m_fPos > 1)
		m_fPos = 1;
	int nTabPos = (int)(m_fPos * (wid - nTabSize));

	// Position the three tab areas
	m_pLeftTab->setPos(0, 0);
	m_pLeftTab->setSize(nTabPos, m_rect.h);
	m_pTab->setPos(nTabPos, 0);
	m_pTab->setSize(nTabSize, m_rect.h);
	m_pRightTab->setPos(nTabPos + nTabSize, 0);
	m_pRightTab->setSize(wid - (nTabPos + nTabSize), m_rect.h);

	// Draw everything
	pCanvas->boxFill(x, y, m_rect.w, m_rect.h, 0x00000000);
	WRect* pRect;
	pRect = m_pLeftTab->rect();
	m_pLeftTab->draw(pCanvas, x + pRect->x, y + pRect->y);
	pRect = m_pTab->rect();
	m_pTab->draw(pCanvas, x + pRect->x, y + pRect->y);
	pRect = m_pRightTab->rect();
	m_pRightTab->draw(pCanvas, x + pRect->x, y + pRect->y);
	setClean();
}

// virtual
void WWidgetHorizSlider::onClickTab(WWidgetSliderTab* pTab)
{
	if(pTab == m_pRightTab)
	{
		m_fPos += (float).1;
		if(m_fPos > 1)
			m_fPos = 1;
		m_dirty = true;
		m_pParent->tattle(this);
		m_pParent->onHorizSliderMove(this);
	}
	else if(pTab == m_pLeftTab)
	{
		m_fPos -= (float).1;
		if(m_fPos < 0)
			m_fPos = 0;
		m_dirty = true;
		m_pParent->tattle(this);
		m_pParent->onHorizSliderMove(this);
	}
}

/*virtual*/ void WWidgetHorizSlider::onSlideTab(WWidgetSliderTab* pTab, int dx, int dy)
{
	if(pTab != m_pTab)
		return;
	int wid = m_rect.w;
	int hgt = m_rect.h;
	int nTabSize = hgt / 2;
	m_fPos += (float)dx / (wid - nTabSize);
	if(m_fPos < 0)
		m_fPos = 0;
	else if(m_fPos > 1)
		m_fPos = 1;
	m_dirty = true;
	m_pParent->tattle(this);
	m_pParent->onHorizSliderMove(this);
}

// ----------------------------------------------------------------------

WWidgetVertSlider::WWidgetVertSlider(WWidgetGroup* pParent, int x, int y, int w, int h)
: WWidgetGroup(pParent, x, y, w, h)
{
	m_dirty = true;
	m_fPos = .5;
	m_pAboveTab = new WWidgetSliderTab(this, 0, 0, w, 0, true, WWidgetSliderTab::SliderArea);
	m_pTab = new WWidgetSliderTab(this, 0, 0, w, 0, true, WWidgetSliderTab::SliderNub);
	m_pBelowTab = new WWidgetSliderTab(this, 0, 0, w, 0, true, WWidgetSliderTab::SliderArea);
}

/*virtual*/ WWidgetVertSlider::~WWidgetVertSlider()
{
}

void WWidgetVertSlider::setPos(float f)
{
	m_fPos = f;
	m_dirty = true;
	m_pParent->tattle(this);
	m_pParent->onVertSliderMove(this);
}

// virtual
void WWidgetVertSlider::draw(WImage* pCanvas, int x, int y)
{
	// Calculations
	int wid = m_rect.w;
	int hgt = m_rect.h;
	WAssert(hgt > wid); // disproportioned vertical slider
	int nTabSize = wid / 2;
	if(m_fPos < 0)
		m_fPos = 0;
	else if(m_fPos > 1)
		m_fPos = 1;
	int nTabPos = hgt - nTabSize - (int)(m_fPos * (hgt - nTabSize));

	// Position the three tab areas
	m_pAboveTab->setPos(0, 0);
	m_pAboveTab->setSize(m_rect.w, nTabPos);
	m_pTab->setPos(0, nTabPos);
	m_pTab->setSize(m_rect.w, nTabSize);
	m_pBelowTab->setPos(0, nTabPos + nTabSize);
	m_pBelowTab->setSize(m_rect.w, hgt - (nTabPos + nTabSize));

	// Draw everything
	pCanvas->boxFill(x, y, m_rect.w, m_rect.h, 0x00000000);
	WRect* pRect;
	pRect = m_pAboveTab->rect();
	m_pAboveTab->draw(pCanvas, x + pRect->x, y + pRect->y);
	pRect = m_pTab->rect();
	m_pTab->draw(pCanvas, x + pRect->x, y + pRect->y);
	pRect = m_pBelowTab->rect();
	m_pBelowTab->draw(pCanvas, x + pRect->x, y + pRect->y);
	setClean();
}

// virtual
void WWidgetVertSlider::onClickTab(WWidgetSliderTab* pTab)
{
	if(pTab == m_pBelowTab)
	{
		m_fPos -= (float).2;
		if(m_fPos < 0)
			m_fPos = 0;
		m_dirty = true;
		m_pParent->tattle(this);
		m_pParent->onVertSliderMove(this);
	}
	else if(pTab == m_pAboveTab)
	{
		m_fPos += (float).2;
		if(m_fPos > 1)
			m_fPos = 1;
		m_dirty = true;
		m_pParent->tattle(this);
		m_pParent->onVertSliderMove(this);
	}
}

/*virtual*/ void WWidgetVertSlider::onSlideTab(WWidgetSliderTab* pTab, int dx, int dy)
{
	if(pTab != m_pTab)
		return;
	int wid = m_rect.w;
	int hgt = m_rect.h;
	int nTabSize = wid / 2;
	m_fPos -= (float)dy / (hgt - nTabSize);
	if(m_fPos < 0)
		m_fPos = 0;
	else if(m_fPos > 1)
		m_fPos = 1;
	m_dirty = true;
	m_pParent->tattle(this);
	m_pParent->onVertSliderMove(this);
}

// ----------------------------------------------------------------------

WWidgetCanvas::WWidgetCanvas(WWidgetGroup* pParent, int x, int y, int w, int h, WImage* pImage)
: WWidgetAtomic(pParent, x, y, w, h)
{
	m_fZoom = 1;
	m_nHScroll = 0;
	m_nVScroll = 0;
	m_pImageIn = pImage;
	m_pSelectionMask = nullptr;
	m_pressed = false;
	m_nMouseX = 0;
	m_nMouseY = 0;
}

/*virtual*/ WWidgetCanvas::~WWidgetCanvas()
{
}

void WWidgetCanvas::setDirty()
{
	m_pParent->tattle(this);
}

void WWidgetCanvas::setImage(WImage* pImage)
{
	m_pImageIn = pImage;
	m_pParent->tattle(this);
}

void WWidgetCanvas::setSelectionMask(WImage* pMask)
{
	m_pSelectionMask = pMask;
	m_pParent->tattle(this);
}

// virtual
void WWidgetCanvas::draw(WImage* pCanvas, int xx, int yy)
{
	if(!m_pImageIn)
		return;

	// Blit the source image onto the destination canvas
	// todo: why not just use the WImage::Blit method?
	int x, y, xIn, yIn;
	int widIn = m_pImageIn->width();
	int hgtIn = m_pImageIn->height();
	for(y = 0; y < m_rect.h; y++)
	{
		yIn = (int)((y + m_nVScroll) / m_fZoom);
		if(yIn >= hgtIn)
			break;
		for(x = 0; x < m_rect.w; x++)
		{
			xIn = (int)((x + m_nHScroll) / m_fZoom);
			if(xIn >= widIn)
				break;
			pCanvas->setPixel(xx + x, yy + y, m_pImageIn->pixel(xIn, yIn));
		}
	}
	if(m_pSelectionMask)
	{
		double dTime = WTime::seconds() * 32; // 32 is the speed at which the diagonal bars move
		int nTime = (int)(dTime / 65536);
		nTime = (int)(dTime - (double)65536 * nTime);
		unsigned int c;
		for(y = 0; y < m_rect.h; y++)
		{
			yIn = (int)((y + m_nVScroll) / m_fZoom);
			if(yIn >= hgtIn)
				break;
			for(x = 0; x < m_rect.w; x++)
			{
				xIn = (int)((x + m_nHScroll) / m_fZoom);
				if(xIn >= widIn)
					break;
				c = m_pSelectionMask->pixel(xIn, yIn);
				if((x + y + nTime) % 40 < (int)gAlpha(c) / 8) // 32/40 of the area will be filled with diagonal bars when the area is fully (255) selected
					pCanvas->setPixel(xx + x, yy + y, c | 0xff000000);
			}
		}
	}
}

/*virtual*/ void WWidgetCanvas::grab(int button, int x, int y)
{
	m_nMouseX = x;
	m_nMouseY = y;
	m_pressed = true;
	m_pParent->onCanvasMouseDown(this, button, (int)((x + m_nHScroll) / m_fZoom), (int)((y + m_nVScroll) / m_fZoom));
}

/*virtual*/ void WWidgetCanvas::release(int button)
{
	m_pressed = false;
	m_pParent->onCanvasMouseUp(this, button, (int)((m_nMouseX + m_nHScroll) / m_fZoom), (int)((m_nMouseY + m_nVScroll) / m_fZoom));
}

/*virtual*/ void WWidgetCanvas::onMouseMove(int dx, int dy)
{
	m_nMouseX += dx;
	m_nMouseY += dy;
	m_pParent->onCanvasMouseMove(this, (int)((m_nMouseX + m_nHScroll) / m_fZoom), (int)((m_nMouseY + m_nVScroll) / m_fZoom), m_pressed);
}

void WWidgetCanvas::setZoom(float f)
{
	m_fZoom = f;
	if(m_pImageIn)
	{
		if(m_nHScroll > (int)(m_fZoom * m_pImageIn->width()) - m_rect.w)
			m_nHScroll = std::max(0, (int)(m_fZoom * m_pImageIn->width()) - m_rect.w);
		if(m_nVScroll > (int)(m_fZoom * m_pImageIn->height()) - m_rect.h)
			m_nVScroll = std::max(0, (int)(m_fZoom * m_pImageIn->height()) - m_rect.h);
	}
	m_pParent->tattle(this);
}

void WWidgetCanvas::zoomToFit()
{
	setZoom(std::min((float)m_rect.w / m_pImageIn->width(), (float)m_rect.h / m_pImageIn->height()));
}

void WWidgetCanvas::setHorizScroll(int x)
{
	WAssert(x >= 0 && x < (int)(m_pImageIn->width() * m_fZoom)); // out of range
	m_nHScroll = x;
	m_pParent->tattle(this);
}

void WWidgetCanvas::setVertScroll(int y)
{
	WAssert(y >= 0 && y < (int)(m_pImageIn->height() * m_fZoom)); // out of range
	m_nVScroll = y;
	m_pParent->tattle(this);
}

// ----------------------------------------------------------------------

WWidgetWave::WWidgetWave(WWidgetGroup* pParent, int x, int y, int w, int h)
: WWidgetAtomic(pParent, x, y, w, h)
{
	m_pWave = nullptr;
	m_pos = 0;
	m_width = w;
	m_selectionPos = -1;
	m_selectionWidth = 0;
}

/*virtual*/ WWidgetWave::~WWidgetWave()
{
}

void WWidgetWave::setDirty()
{
	m_pParent->tattle(this);
}

void WWidgetWave::setWave(WWave* pWave)
{
	m_pWave = pWave;
	m_pParent->tattle(this);
}

void WWidgetWave::setRange(int position, int wid)
{
	m_pos = position;
	m_width = wid;
	m_pParent->tattle(this);
}

// virtual
void WWidgetWave::draw(WImage* pCanvas, int xx, int yy)
{
	pCanvas->boxFill(xx, yy, m_rect.w, m_rect.h, 0xff000000);
	if(!m_pWave)
		return;

	if(m_pWave->bitsPerSample() == 32)
	{
		int* pData = (int*)m_pWave->data();
		if(m_rect.w * 2 >= m_width)
		{
			int i, val, x, y;
			int xprev = -1;
			int yprev = -1;
			for(i = m_pos; i < (int)m_pWave->sampleCount(); i++)
			{
				x = (i - m_pos) * m_rect.w / m_width;
				if(x >= m_rect.w)
					break;
				val = pData[i] / 65536 + 32768;
				y = val * m_rect.h / 65536;
				if(xprev >= 0)
					pCanvas->lineNoChecks(xx + xprev, yy + yprev, xx + x, yy + y, 0xff0080c0);
				xprev = x;
				yprev = y;
			}
		}
		else
		{
			int i, j, index;
			int val;
			for(i = 0; i < m_rect.w; i++)
			{
				int min = 0x7fffffff;
				int max = -0x7fffffff;
				for(j = 0; j < 8; j++)
				{
					index = (i * 8 + j) * m_width / (m_rect.w * 8) + m_pos;
					if(index >= 0 && index < (int)m_pWave->sampleCount())
					{
						val = pData[index];
						if(val > max)
							max = val;
						if(val < min)
							min = val;
					}
				}
				min /= 65536;
				max /= 65536;
				min += 32768;
				max += 32768;
				min *= m_rect.h;
				max *= m_rect.h;
				min /= 65536;
				max /= 65536;
				WAssert(max < m_rect.h && min < m_rect.h); // math error
				pCanvas->lineNoChecks(xx + i, yy + min, xx + i, yy + max, 0xff0080c0);
			}
		}
	}
	else if(m_pWave->bitsPerSample() == 16)
	{
		short* pData = (short*)m_pWave->data();
		if(m_rect.w * 2 >= m_width)
		{
			int i, val, x, y;
			int xprev = -1;
			int yprev = -1;
			for(i = m_pos; i < (int)m_pWave->sampleCount(); i++)
			{
				x = (i - m_pos) * m_rect.w / m_width;
				if(x >= m_rect.w)
					break;
				val = pData[i] + 32768;
				y = val * m_rect.h / 65536;
				if(xprev >= 0)
					pCanvas->lineNoChecks(xx + xprev, yy + yprev, xx + x, yy + y, 0xff0080c0);
				xprev = x;
				yprev = y;
			}
		}
		else
		{
			int i, j, index;
			int val;
			for(i = 0; i < m_rect.w; i++)
			{
				int min = 0x7fffffff;
				int max = -0x7fffffff;
				for(j = 0; j < 8; j++)
				{
					index = (i * 8 + j) * m_width / (m_rect.w * 8) + m_pos;
					if(index >= 0 && index < (int)m_pWave->sampleCount())
					{
						val = pData[index];
						if(val > max)
							max = val;
						if(val < min)
							min = val;
					}
				}
				min += 32768;
				max += 32768;
				min *= m_rect.h;
				max *= m_rect.h;
				min /= 65536;
				max /= 65536;
				WAssert(max < m_rect.h && min < m_rect.h); // math error
				pCanvas->lineNoChecks(xx + i, yy + min, xx + i, yy + max, 0xff0080c0);
			}
		}
	}
	else if(m_pWave->bitsPerSample() == 8)
	{
		unsigned char* pData = (unsigned char*)m_pWave->data();
		if(m_rect.w * 2 >= m_width)
		{
			int i, val, x, y;
			int xprev = -1;
			int yprev = -1;
			for(i = m_pos; i < (int)m_pWave->sampleCount(); i++)
			{
				x = (i - m_pos) * m_rect.w / m_width;
				if(x >= m_rect.w)
					break;
				val = pData[i] * 256; // + 32768;
				y = val * m_rect.h / 65536;
				if(xprev >= 0)
					pCanvas->lineNoChecks(xx + xprev, yy + yprev, xx + x, yy + y, 0xff0080c0);
				xprev = x;
				yprev = y;
			}
		}
		else
		{
			int i, j, index;
			int val;
			for(i = 0; i < m_rect.w; i++)
			{
				int min = 0x7fffffff;
				int max = -0x7fffffff;
				for(j = 0; j < 8; j++)
				{
					index = (i * 8 + j) * m_width / (m_rect.w * 8) + m_pos;
					if(index >= 0 && index < (int)m_pWave->sampleCount())
					{
						val = pData[index];
						if(val > max)
							max = val;
						if(val < min)
							min = val;
					}
				}
				min *= 256;
				max *= 256;
				//min += 32768;
				//max += 32768;
				min *= m_rect.h;
				max *= m_rect.h;
				min /= 65536;
				max /= 65536;
				WAssert(max < m_rect.h && min < m_rect.h); // math error
				pCanvas->lineNoChecks(xx + i, yy + min, xx + i, yy + max, 0xff0080c0);
			}
		}
	}
	else
		throw Ex("Sorry, WWidgetWave doesn't support ", to_str(m_pWave->bitsPerSample()), " bit samples. It only supports 16 and 32 bit samples");
}

/*virtual*/ void WWidgetWave::grab(int button, int x, int y)
{
}

/*virtual*/ void WWidgetWave::release(int button)
{
}

/*virtual*/ void WWidgetWave::onMouseMove(int dx, int dy)
{
}

} // namespace WClasses

