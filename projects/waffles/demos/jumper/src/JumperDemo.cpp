#include "JumperDemo.h"
#include "PolicyLearner.h"
#include <demos/app.h>
#include <demos/ogl_headers.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
// Runs `draw` placed where `shape`'s body is, `fraction` of the way from its
// last step to now
template<typename Draw>
void drawOnBody(const cyclone::CollisionPrimitive& shape, cyclone::real fraction, Draw draw)
{
	GLfloat transform[16];
	shape.body->getBlendedGLTransform(fraction, transform);
	glPushMatrix();
	glMultMatrixf(transform);
	draw();
	glPopMatrix();
}

class JumperDemo : public RigidBodyApplication
{
protected:
	Jumper m_jumper;
	double m_highest = 0; // the highest its lowest point has been this jump

	// The robot's world, its policy acting before each sub-step
	cyclone::World& getWorld() override { return m_jumper.robot().world(); }

	// The height after each step, as JumpHeight measures it, so the highest
	// shown is the height the policy scored
	void afterStep() override { m_highest = std::max(m_highest, m_jumper.robot().lowestPoint()); }

	void reset() override
	{
		m_jumper.reset();
		m_highest = 0;
	}

public:
	explicit JumperDemo(WClasses::WNeuralNetLearner* pPolicy)
	: m_jumper(pPolicy)
	{
		// Simulated as JumpHeight scores it (PolicyLearner.h)
		physicsClock.setStepSeconds(kStepSeconds);
		physicsClock.setSubSteps(kSubSteps);
		// A robot 1.8 m tall: closer than the demos' 10 m scenes
		cameraDistance = 4;
		cameraTargetHeight = 0.9f;
		startRunning();
	}

	const char* getTitle() override { return "Waffles > Jumper"; }

	const char* getDescription() override
	{
		return "A robot that learns to jump: a neural network (the policy) drives\n"
		       "its 11 hinges from the time and how it's lying. Training\n"
		       "(jumper train) evolves the network to jump higher. Simulated by\n"
		       "Cyclone. R: jump again\n" RIGID_BODY_KEYS;
	}

	void initGraphics() override
	{
		GLfloat lightAmbient[] = {0.8f, 0.8f, 0.8f, 1.0f};
		GLfloat lightDiffuse[] = {0.9f, 0.95f, 1.0f, 1.0f};
		glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
		glEnable(GL_LIGHT0);
		Application::initGraphics();
	}

	void display() override
	{
		const static GLfloat lightPosition[] = {0.7f, -1, 0.4f, 0};
		RigidBodyApplication::display();

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_LIGHTING);
		glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
		glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
		glEnable(GL_COLOR_MATERIAL);
		glEnable(GL_NORMALIZE);
		const Robot& robot = m_jumper.robot();
		glColor3f(0.75f, 0.55f, 0.35f);
		for(unsigned part = 0; part < Robot::capsuleCount; part++)
		{
			const cyclone::CollisionCapsule& capsule = robot.capsule(Robot::Part(part));
			drawOnBody(capsule, drawFraction(), [&]
			{ solidCapsule(capsule.radius, capsule.halfLength, 16); });
		}
		glColor3f(0.3f, 0.3f, 0.35f);
		for(Robot::Part part : {Robot::leftFoot, Robot::rightFoot})
		{
			const cyclone::CollisionBox& foot = robot.foot(part);
			drawOnBody(foot, drawFraction(), [&]
			{
				glScaled(foot.halfSize.x * 2, foot.halfSize.y * 2, foot.halfSize.z * 2);
				solidCube(1);
			});
		}
		glDisable(GL_NORMALIZE);
		glDisable(GL_COLOR_MATERIAL);
		glDisable(GL_LIGHTING);

		// The ground: a grid of 25 cm squares
		glColor3f(0.75f, 0.75f, 0.75f);
		glBegin(GL_LINES);
		for(int i = -8; i <= 8; i++)
		{
			glVertex3f(i * 0.25f, 0, -2);
			glVertex3f(i * 0.25f, 0, 2);
			glVertex3f(-2, 0, i * 0.25f);
			glVertex3f(2, 0, i * 0.25f);
		}
		glEnd();

		RigidBodyApplication::drawDebug();

		char text[96];
		std::snprintf(text, sizeof text, "%.1f s   height %.2f m   highest %.2f m", m_jumper.time(), m_jumper.robot().lowestPoint(), m_highest);
		glColor3f(0.1f, 0.1f, 0.15f);
		renderText(10.0f, 10.0f, text);
	}
};
} // namespace

Application* MakeJumperDemo(WClasses::WNeuralNetLearner* pPolicy)
{
	return new JumperDemo(pPolicy);
}
