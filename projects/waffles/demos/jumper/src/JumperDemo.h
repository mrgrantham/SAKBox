// The jumper in a window, on the Cyclone demos' framework (projects/cyclone):
// the same camera, keys and fixed steps as Cyclone's rigid-body demos.
#ifndef JUMPER_DEMO_H
#define JUMPER_DEMO_H

class Application;
namespace WClasses
{
class WNeuralNetLearner;
}

/// A window showing the robot jump under `pPolicy` (manualPolicy if null),
/// for runInWindow (Cyclone's demos/window.h)
Application* MakeJumperDemo(WClasses::WNeuralNetLearner* pPolicy);

#endif // JUMPER_DEMO_H
