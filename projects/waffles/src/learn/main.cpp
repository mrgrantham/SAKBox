/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
    Eric Moyer,
    Michael R. Smith,
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

#include "WClasses/WLearnerLib.h"

using namespace WClasses;

int main(int argc, char* argv[])
{
#ifdef _DEBUG
	WApp::enableFloatingPointExceptions();
#endif
	int nRet = 0;
	PathData pd;
	WFile::parsePath(argv[0], &pd);
	const char* appName = argv[0] + pd.fileStart;
	WArgReader args(argc, argv);
	try
	{
		args.pop_string(); // advance past the name of this app
		if(args.size() >= 1)
		{
			if(args.if_pop("usage"))
				WLearnerLib::ShowUsage(appName);
			else if(args.if_pop("autotune"))
				WLearnerLib::autoTune(args);
			else if(args.if_pop("train"))
				WLearnerLib::Train(args);
			else if(args.if_pop("test"))
				WLearnerLib::Test(args);
			else if(args.if_pop("predict"))
				WLearnerLib::predict(args);
			else if(args.if_pop("predictdistribution"))
				WLearnerLib::predictDistribution(args);
			else if(args.if_pop("transduce"))
				WLearnerLib::Transduce(args);
			else if(args.if_pop("transacc"))
				WLearnerLib::TransductiveAccuracy(args);
			else if(args.if_pop("splittest"))
				WLearnerLib::SplitTest(args);
			else if(args.if_pop("crossvalidate"))
				WLearnerLib::CrossValidate(args);
			else if(args.if_pop("precisionrecall"))
				WLearnerLib::PrecisionRecall(args);
			else if(args.if_pop("sterilize"))
				WLearnerLib::sterilize(args);
			//			else if(args.if_pop("trainrecurrent"))
			//				WLearnerLib::trainRecurrent(args);
			else if(args.if_pop("regress"))
				WLearnerLib::regress(args);
			else if(args.if_pop("metadata"))
				WLearnerLib::metaData(args);
			else
			{
				nRet = 1;
				string s = args.peek();
				s += " is not a recognized command.";
				WLearnerLib::showError(args, appName, s.c_str());
			}
		}
		else
		{
			nRet = 1;
			WLearnerLib::showError(args, appName, "Brief Usage Information:");
		}
	}
	catch(const std::exception& e)
	{
		nRet = 1;
		if(strcmp(e.what(), "nevermind") != 0) // if an error message was not already displayed...
			WLearnerLib::showError(args, appName, e.what());
	}
	return nRet;
}
