#include "JumperDemo.h"
#include "PolicyLearner.h"
#include <WClasses/WError.h>
#include <WClasses/WNeuralNet.h>
#include <demos/window.h>
#include <iostream>
#include <memory>

using namespace WClasses;
using std::cerr;
using std::cout;

int DoIt(int argc, char* argv[])
{
	if(argc < 2)
	{
		cout << "Jumper is a demo of a robot that learns to jump. It uses\n";
		cout << "a neural network to represent a policy that tells it how\n";
		cout << "to move its joints based on the time, and the orientation\n";
		cout << "of its spine. It improves the policy over time by\n";
		cout << "using an evolutionary optimizer to adjust the weights, and\n";
		cout << "computes fitness by the height it obtains.\n\n";

		cout << "Usage:\n";
		cout << "	jumper				see this info\n";
		cout << "	jumper train			generate policies\n";
		cout << "	jumper [policy_file]		see it jump\n";
		cout << "	jumper manual			see the hand-made seed policy jump\n";
		cout << "	jumper height [policy_file]	print how high it jumps (the seed\n";
		cout << "					policy's if no file is given)\n\n";
		cout << "Seeing it jump takes the Cyclone demos' options after the\n";
		cout << "policy: --run, and --keys \"SECONDS:INPUT,...\" (R jumps again).\n";
		return 0;
	}
	else if(_stricmp(argv[1], "train") == 0)
	{
		cout << "Training the jumper. This takes some time, so let it run\n";
		cout << "until you have several policy files. When you've got enough,\n";
		cout << "press ctrl-c to stop. Then, run\n\n";

		cout << "	jumper [policy_file]\n\n";

		cout << "to see it jump with one of the policies. The longer you let\n";
		cout << "it train, the better they should get. You might want to\n";
		cout << "open another shell, so you can let this one train while you\n";
		cout << "view results in the other one.\n";

		Train();

		cout << "Well, that's enough. I think I'll stop now.\n";
		return 0;
	}
	else if(_stricmp(argv[1], "height") == 0)
	{
		std::unique_ptr<WNeuralNetLearner> pPolicy;
		if(argc > 2)
			pPolicy = LoadPolicy(argv[2]);
		cout << JumpHeight(pPolicy.get()) << "\n";
		return 0;
	}

	// See it jump, with the hand-made policy or one from a file. (The window
	// reads its own options from what follows.)
	std::unique_ptr<WNeuralNetLearner> pPolicy;
	if(_stricmp(argv[1], "manual") != 0)
		pPolicy = LoadPolicy(argv[1]);
	auto makeDemo = [&]
	{ return MakeJumperDemo(pPolicy.get()); };
	return runInWindow(makeDemo, argc - 1, argv + 1);
}

int main(int argc, char* argv[])
{
	try
	{
		return DoIt(argc, argv);
	}
	catch(const std::exception& e)
	{
		cerr << e.what() << "\n";
		return 1;
	}
}
