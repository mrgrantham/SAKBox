// -------------------------------------------------------------
// The contents of this file may be distributed under the CC0
// license (http://creativecommons.org/publicdomain/zero/1.0/).
// Of course you may also distribute it under a more restrictive
// license, such as any of the OSI-approved licenses
// (http://www.opensource.org/licenses).
// -------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <WClasses/GApp.h>
#include <WClasses/GError.h>
#include "Paint.h"
#ifdef WINDOWS
#include <direct.h>
#endif
#include <exception>
#include <iostream>

using std::cerr;

int main(int argc, char* argv[])
{
	// SAKBox: open and save files where it was run from (`bazel run` starts
	// programs in their runfiles and says where it was run in this variable)
	const char* szWhereRun = getenv("BUILD_WORKING_DIRECTORY");
#ifdef WINDOWS
	if(szWhereRun && _chdir(szWhereRun) != 0)
#else
	if(szWhereRun && chdir(szWhereRun) != 0)
#endif
	{
	}
	int nRet = 0;
	try
	{
		PaintController c;
		c.RunModal();
	}
	catch(const std::exception& e)
	{
		cerr << e.what() << "\n";
		nRet = 1;
	}

	return nRet;
}

