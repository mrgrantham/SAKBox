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

// SAKBox: altered from Bullet's demo GlutDemoApplication.cpp and GlutStuff.cpp
// (GLUT replaced with SDL2; see SdlDemoApplication.h).

#include "SdlDemoApplication.h"

#include "btBulletDynamicsCommon.h"

#include <SDL2/SDL.h>
#include <WClasses/GError.h>

void SdlDemoApplication::run(int width, int height, const char* title)
{
	if(SDL_Init(SDL_INIT_VIDEO) != 0)
		throw GClasses::Ex("Couldn't start SDL: ", SDL_GetError());
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	m_pWindow = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if(!m_pWindow)
		throw GClasses::Ex("Couldn't open a window: ", SDL_GetError());
	SDL_GLContext context = SDL_GL_CreateContext(m_pWindow);
	if(!context)
		throw GClasses::Ex("Couldn't create an OpenGL context: ", SDL_GetError());
	SDL_GL_SetSwapInterval(1); // sync to the display, so it runs at its refresh rate

	myinit();
	reshape(width, height);
	while(!quitRequested())
	{
		SDL_Event event;
		while(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
				case SDL_QUIT:
					requestQuit();
					break;
				case SDL_WINDOWEVENT:
					if(event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
						reshape(event.window.data1, event.window.data2);
					break;
				case SDL_KEYDOWN:
				case SDL_KEYUP:
				{
					bool down = event.type == SDL_KEYDOWN;
					SDL_Keycode key = event.key.keysym.sym;
					int x, y;
					SDL_GetMouseState(&x, &y);
					updateModifierKeys();
					if(key >= 0 && key < 128) // a character
					{
						if(down)
							keyboardCallback((unsigned char)key, x, y);
						else
							keyboardUpCallback((unsigned char)key, x, y);
					}
					else if(down)
						specialKeyboard(key, x, y);
					else
						specialKeyboardUp(key, x, y);
					break;
				}
				case SDL_MOUSEBUTTONDOWN:
				case SDL_MOUSEBUTTONUP:
					// GLUT's numbering: buttons 0 (left), 1 (middle), 2 (right);
					// state 0 for pressed, 1 for released
					updateModifierKeys();
					mouseFunc(event.button.button - SDL_BUTTON_LEFT, event.type == SDL_MOUSEBUTTONDOWN ? 0 : 1, event.button.x, event.button.y);
					break;
				case SDL_MOUSEMOTION:
					mouseMotionFunc(event.motion.x, event.motion.y);
					break;
			}
		}
		moveAndDisplay();
	}

	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(m_pWindow);
	m_pWindow = nullptr;
	SDL_Quit();
}

void SdlDemoApplication::updateModifierKeys()
{
	SDL_Keymod mod = SDL_GetModState();
	m_modifierKeys = 0;
	if(mod & KMOD_ALT)
		m_modifierKeys |= BT_ACTIVE_ALT;
	if(mod & KMOD_CTRL)
		m_modifierKeys |= BT_ACTIVE_CTRL;
	if(mod & KMOD_SHIFT)
		m_modifierKeys |= BT_ACTIVE_SHIFT;
}

void SdlDemoApplication::specialKeyboard(int key, int x, int y)
{
	(void)x;
	(void)y;

	switch(key)
	{
		case SDLK_END:
		{
			int numObj = getDynamicsWorld()->getNumCollisionObjects();
			if(numObj)
			{
				btCollisionObject* obj = getDynamicsWorld()->getCollisionObjectArray()[numObj - 1];

				getDynamicsWorld()->removeCollisionObject(obj);
				btRigidBody* body = btRigidBody::upcast(obj);
				if(body && body->getMotionState())
				{
					delete body->getMotionState();
				}
				delete obj;
			}
			break;
		}
		case SDLK_LEFT: stepLeft(); break;
		case SDLK_RIGHT: stepRight(); break;
		case SDLK_UP: stepFront(); break;
		case SDLK_DOWN: stepBack(); break;
		case SDLK_PAGEUP: zoomIn(); break;
		case SDLK_PAGEDOWN: zoomOut(); break;
		case SDLK_HOME: toggleIdle(); break;
		default:
			break;
	}
}

void SdlDemoApplication::swapBuffers()
{
	SDL_GL_SwapWindow(m_pWindow);
}
