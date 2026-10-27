/*
 * The main entry point for all demos.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */

#include "app.h"
#include "window.h"

// Forward declaration of the function that will return the
// application object for this particular demo. This should be
// implemented in the demo's .cpp file.
extern Application *getApplication();

int main(int argc, char **argv) { return runInWindow(getApplication, argc, argv); }
