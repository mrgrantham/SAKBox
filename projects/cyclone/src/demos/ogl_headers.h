/*
 * OpenGL headers for the demos.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */
#ifndef _OPENGL_HEADERS_
#define _OPENGL_HEADERS_

// SAKBox: the demos run in a GLFW window (they used GLUT). GLFW includes the
// platform's OpenGL and GLU headers; the demos use legacy fixed-function
// OpenGL, which GLFW's default context provides on every platform.
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#define GLFW_INCLUDE_GLU
#include <GLFW/glfw3.h>

#include "shapes.h"

// Mouse buttons and states passed to Application::mouse (GLUT's values)
enum { MOUSE_LEFT_BUTTON = 0, MOUSE_MIDDLE_BUTTON = 1, MOUSE_RIGHT_BUTTON = 2 };
enum { MOUSE_DOWN = 0, MOUSE_UP = 1 };

#endif // _OPENGL_HEADERS_
