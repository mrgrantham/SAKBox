/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

// SAKBox: altered from Bullet's demo GlutDemoApplication and GlutStuff: a
// DemoApplication in an SDL2 window (OpenGL 2.1, for the fixed-function
// drawing), with SDL's events passed on as GLUT's callbacks were.

#ifndef SDL_DEMO_APPLICATION_H
#define SDL_DEMO_APPLICATION_H

#include "DemoApplication.h"

struct SDL_Window;

class SdlDemoApplication : public DemoApplication
{
public:
	/// Opens a window and runs the demo in it until the window is closed or q
	/// is pressed. Each frame calls moveAndDisplay; the display is synced to
	/// the screen's refresh.
	void run(int width, int height, const char* title);

	void specialKeyboard(int key, int x, int y) override;
	void swapBuffers() override;
	void updateModifierKeys() override;

private:
	SDL_Window* m_pWindow = nullptr;
};

#endif // SDL_DEMO_APPLICATION_H
