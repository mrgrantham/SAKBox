// The jumping robot, simulated by the repo's Cyclone physics engine
// (projects/cyclone). It's the robot of Bullet's RagdollDemo (Marten
// Svanfeldt, Starbreeze Studios), which the jumper used until SAKBox moved
// it to Cyclone: the same parts, sizes and masses, the same joints and
// limits, and motors as strong.
#ifndef JUMPER_ROBOT_H
#define JUMPER_ROBOT_H

#include <cyclone/cyclone.h>
#include <span>

/// A robot of thirteen parts: capsules for the body, head, arms and legs,
/// and boxes for the feet, joined by eleven hinges (the policy's motors)
/// and a neck that tilts. It faces -z, with its left side towards -x, and
/// stands on the ground (y = 0), in a world of its own (world()), where
/// parts collide with the ground and each other but for those joined.
class Robot
{
public:
	enum Part
	{
		pelvis,
		spine,
		head,
		leftUpperLeg,
		leftLowerLeg,
		rightUpperLeg,
		rightLowerLeg,
		leftUpperArm,
		leftLowerArm,
		rightUpperArm,
		rightLowerArm,
		leftFoot,
		rightFoot,
		partCount,
		capsuleCount = leftFoot, // the parts before the feet are capsules
	};

	/// The hinges, in the order of the policy's outputs. Each one's angle
	/// is zero standing straight; a positive angle bends the spine forward,
	/// a hip or shoulder back, a knee back (its natural way), an elbow
	/// forward, an ankle's toes down.
	enum Hinge
	{
		spineHinge,
		leftHip,
		leftKnee,
		rightHip,
		rightKnee,
		leftShoulder,
		leftElbow,
		rightShoulder,
		rightElbow,
		leftAnkle,
		rightAnkle,
		hingeCount,
	};

	/// Stands the robot up, still, its feet just above the ground
	Robot();
	Robot(const Robot&) = delete;
	Robot& operator=(const Robot&) = delete;

	/// Stands it up again, as it was made
	void reset();

	/// Drives each hinge from one of the policy's outputs, as the jumper's
	/// Bullet robot did: towards bending one way or the other at 8 rad/s,
	/// as hard as the output's size, up to the joint's strength that way
	void setMotors(std::span<const double, hingeCount> outputs);

	/// The world its parts, shapes and joints are in. Code that acts as it
	/// moves (a policy setting its motors) does it in the world's
	/// beforeSubStep.
	cyclone::World& world() { return m_world; }

	/// One step of `seconds`, in the world's sub-steps
	void step(cyclone::real_duration seconds);

	/// The joints: the hinges (in Hinge's order), then the neck
	std::span<cyclone::Joint* const> joints() { return m_jointList; }

	/// How high the robot's lowest point is (m): its height off the ground
	double lowestPoint() const;

	/// 1 standing up, -1 on its head, 0 lying down (the spine's up
	/// direction, how far it points up)
	double uprightness() const;

	/// 1 lying on its front, -1 on its back, 0 on its side
	double faceDownness() const;

	/// A hinge's angle (rad, see Hinge)
	double angle(Hinge hinge) const;

	// Its shapes, to draw
	const cyclone::CollisionCapsule& capsule(Part part) const { return m_capsules[part]; }
	const cyclone::CollisionBox& foot(Part part) const { return m_feet[part - leftFoot]; }

protected:
	cyclone::RigidBody m_bodies[partCount];
	cyclone::CollisionCapsule m_capsules[capsuleCount];
	cyclone::CollisionBox m_feet[2];
	cyclone::HingeJoint m_hinges[hingeCount];
	cyclone::ConeJoint m_neck;
	cyclone::Joint* m_jointList[hingeCount + 1];
	cyclone::World m_world;
};

#endif // JUMPER_ROBOT_H
