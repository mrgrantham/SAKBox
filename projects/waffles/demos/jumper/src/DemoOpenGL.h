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

// SAKBox: altered from Bullet's demo GlutStuff.h. The window and input come
// from SDL2 (SdlDemoApplication) instead of GLUT; this keeps the OpenGL
// includes and the definitions the drawing code uses.

#ifndef DEMO_OPENGL_H
#define DEMO_OPENGL_H

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

// Modifier keys held, as bits of DemoApplication::m_modifierKeys. (In the
// GLUT version BT_ACTIVE_CTRL was GLUT_ACTIVE_ALT, the same bit as Alt.)
#define BT_ACTIVE_ALT 1
#define BT_ACTIVE_CTRL 2
#define BT_ACTIVE_SHIFT 4

#if defined(BT_USE_DOUBLE_PRECISION)
#define btglLoadMatrix glLoadMatrixd
#define btglMultMatrix glMultMatrixd
#define btglColor3 glColor3d
#define btglVertex3 glVertex3d
#else
#define btglLoadMatrix glLoadMatrixf
#define btglMultMatrix glMultMatrixf
#define btglColor3 glColor3f
#define btglVertex3 glVertex3d
#endif

#endif // DEMO_OPENGL_H
