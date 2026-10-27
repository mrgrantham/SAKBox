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
/// stands on the ground (y = 0).
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

	/// Moves every part on by `duration` seconds (before its contacts are
	/// found and resolved)
	void integrate(cyclone::real duration);

	/// Writes the contacts between the parts and the ground, and between
	/// parts that aren't joined, into `data`, which has room for
	/// `maxContacts`
	void generateContacts(cyclone::CollisionData& data, unsigned maxContacts) const;

	/// The joints, as the contact resolver takes them
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
	// Which pairs of parts can collide: all but the joined ones
	bool m_collide[partCount][partCount];

	const cyclone::CollisionPrimitive& shape(unsigned part) const;
};

#endif // JUMPER_ROBOT_H
