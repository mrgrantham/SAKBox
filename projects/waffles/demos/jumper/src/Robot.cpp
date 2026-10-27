#include "Robot.h"
#include <algorithm>
#include <cmath>

using cyclone::real;
using cyclone::Vector3;

namespace
{
const double pi = 3.14159265358979323846;

// Each capsule's radius, the length between its ends' centres (Bullet's
// btCapsuleShape height) and mass (kg), as Bullet's robot had them
struct CapsuleShape
{
	double radius, length, mass;
};
const CapsuleShape capsuleShapes[Robot::capsuleCount] = {
  {0.15, 0.20, 1.2}, // pelvis
  {0.15, 0.28, 1.4}, // spine
  {0.10, 0.05, 0.8}, // head
  {0.07, 0.45, 1.1}, // left upper leg
  {0.05, 0.37, 0.8}, // left lower leg
  {0.07, 0.45, 1.1}, // right upper leg
  {0.05, 0.37, 0.8}, // right lower leg
  {0.05, 0.33, 0.7}, // left upper arm
  {0.04, 0.25, 0.6}, // left lower arm
  {0.05, 0.33, 0.7}, // right upper arm
  {0.04, 0.25, 0.6}, // right lower arm
};
const Vector3 footHalfSize(0.06, 0.02, 0.14);
const double footMass = 0.3;

// How the arms hang: turned out from the body about z (the left arm
// clockwise seen from the front, the right anticlockwise), so their hinges
// tilt a little too. Bullet's robot tilted its shoulder hinges this way
// (pi/16), which turned the arms out in its first steps.
const double armTilt = pi / 16;

// Each hinge: the parts it joins, where (in each part's coordinates), its
// limits and its motor's strength each way (Bullet's motor impulse a step
// of 1/60 s), for bending towards a positive angle and towards a negative
// one. Its axis is the robot's -x: every hinge bends front to back.
struct HingeShape
{
	Robot::Part parent, child;
	Vector3 atParent, atChild;
	double lower, upper;
	double positiveStrength, negativeStrength;
};
const HingeShape hingeShapes[Robot::hingeCount] = {
  // spine: leaning back a little to sitting or bending forward
  {Robot::pelvis, Robot::spine, Vector3(0, 0.15, 0), Vector3(0, -0.15, 0), -pi / 8, pi / 4, 1.0, 0.8},
  // left hip: upper leg forward to down and back a little
  {Robot::pelvis, Robot::leftUpperLeg, Vector3(-0.16, -0.10, 0), Vector3(0, 0.225, 0), -pi / 2, pi / 8, 0.5, 1.0},
  // left knee: straight down to straight back
  {Robot::leftUpperLeg, Robot::leftLowerLeg, Vector3(0, -0.225, 0), Vector3(0, 0.185, 0), 0, pi / 2, 0.7, 1.0},
  {Robot::pelvis, Robot::rightUpperLeg, Vector3(0.16, -0.10, 0), Vector3(0, 0.225, 0), -pi / 2, pi / 8, 0.5, 1.0},
  {Robot::rightUpperLeg, Robot::rightLowerLeg, Vector3(0, -0.225, 0), Vector3(0, 0.185, 0), 0, pi / 2, 0.7, 1.0},
  // left shoulder: arm straight forward to back a little
  {Robot::spine, Robot::leftUpperArm, Vector3(-0.2, 0.15, 0), Vector3(0, 0.18, 0), -pi / 2, pi / 8, 1.0, 1.0},
  // left elbow: forearm up a little past level to straight down
  {Robot::leftUpperArm, Robot::leftLowerArm, Vector3(0, -0.18, 0), Vector3(0, 0.14, 0), -pi * 3 / 4, 0, 1.0, 1.0},
  {Robot::spine, Robot::rightUpperArm, Vector3(0.2, 0.15, 0), Vector3(0, 0.18, 0), -pi / 2, pi / 8, 1.0, 1.0},
  {Robot::rightUpperArm, Robot::rightLowerArm, Vector3(0, -0.18, 0), Vector3(0, 0.14, 0), -pi * 3 / 4, 0, 1.0, 1.0},
  // left ankle: toes up to toes pointed
  {Robot::leftLowerLeg, Robot::leftFoot, Vector3(0, -0.215, 0), Vector3(0, 0.02, 0.08), -1.0, 0.6, 1.0, 0.2},
  {Robot::rightLowerLeg, Robot::rightFoot, Vector3(0, -0.215, 0), Vector3(0, 0.02, 0.08), -1.0, 0.6, 1.0, 0.2},
};

// The neck: the head may tilt up to 45 degrees from the spine's line, any
// way (Bullet's cone-twist joint also limited its twist, which a jump never
// reaches)
const Vector3 neckAtSpine(0, 0.30, 0), neckAtHead(0, -0.14, 0);
const double neckTilt = pi / 4;

// The robot's pelvis, standing: high enough that its feet start 1 cm off
// the ground
const Vector3 pelvisStart(0, 1.0, 0);

// How the robot slows (the fraction of its speed and spin it keeps each
// second; Bullet's damping 0.05 and 0.85 take away that fraction), and
// how its parts grip the floor and each other: rubber on concrete, the
// book's static 1.0 and dynamic 0.8 averaged as it suggests (appendix B)
const real linearDamping = 0.95, angularDamping = 0.15;
const real friction = 0.9;

// How the motors drive: Bullet's motors aimed for 8 rad/s, with an
// impulse of up to the joint's strength each step of 1/60 s, which is a
// twist of 60 x strength (N m)
const double motorSpeed = 8;
const double motorTorquePerStrength = 60;

bool isArm(Robot::Part part)
{
	return part == Robot::leftUpperArm || part == Robot::leftLowerArm || part == Robot::rightUpperArm || part == Robot::rightLowerArm;
}

// How part is turned standing: the arms out (see armTilt), the rest straight
cyclone::Quaternion standingOrientation(Robot::Part part)
{
	if(!isArm(part))
		return cyclone::Quaternion();
	const double sign = (part == Robot::leftUpperArm || part == Robot::leftLowerArm) ? -1 : 1;
	const double half = sign * armTilt / 2;
	return cyclone::Quaternion(real_cos((real)half), 0, 0, real_sin((real)half));
}

// How far from its centre a part reaches, in any direction
double reach(unsigned part)
{
	if(part < Robot::capsuleCount)
		return capsuleShapes[part].radius + capsuleShapes[part].length / 2;
	return footHalfSize.magnitude();
}
} // namespace

Robot::Robot()
{
	for(unsigned i = 0; i < hingeCount; i++)
		m_jointList[i] = &m_hinges[i];
	m_jointList[hingeCount] = &m_neck;

	for(unsigned part = 0; part < partCount; part++)
	{
		cyclone::RigidBody& body = m_bodies[part];
		cyclone::Matrix3 inertia;
		if(part < capsuleCount)
		{
			const CapsuleShape& shape = capsuleShapes[part];
			body.setMass((real)shape.mass);
			inertia.setCapsuleInertiaTensor((real)shape.radius, (real)(shape.length / 2), (real)shape.mass);
			m_capsules[part].body = &body;
			m_capsules[part].radius = (real)shape.radius;
			m_capsules[part].halfLength = (real)(shape.length / 2);
		}
		else
		{
			body.setMass((real)footMass);
			inertia.setBlockInertiaTensor(footHalfSize, (real)footMass);
			m_feet[part - leftFoot].body = &body;
			m_feet[part - leftFoot].halfSize = footHalfSize;
		}
		body.setInertiaTensor(inertia);
		body.setDamping(linearDamping, angularDamping);
		body.setAcceleration(Vector3::GRAVITY);
		body.setCanSleep(false); // its motors keep it going
	}

	// Stand it up, then join the parts where they are
	reset();
	for(unsigned i = 0; i < hingeCount; i++)
	{
		const HingeShape& shape = hingeShapes[i];
		cyclone::HingeJoint& hinge = m_hinges[i];
		hinge.set(&m_bodies[shape.parent], shape.atParent, &m_bodies[shape.child], shape.atChild, 0);
		hinge.setAxis(m_bodies[shape.child].getOrientationNow().transform(Vector3(-1, 0, 0)));
		hinge.setLimits((real)shape.lower, (real)shape.upper);
	}
	m_neck.set(&m_bodies[spine], neckAtSpine, &m_bodies[head], neckAtHead, 0);
	m_neck.setAxis(Vector3(0, 1, 0), (real)neckTilt);

	// Every pair of parts collides, but for those joined together
	for(unsigned a = 0; a < partCount; a++)
		for(unsigned b = 0; b < partCount; b++)
			m_collide[a][b] = a != b;
	for(const HingeShape& shape : hingeShapes)
		m_collide[shape.parent][shape.child] = m_collide[shape.child][shape.parent] = false;
	m_collide[spine][head] = m_collide[head][spine] = false;
}

void Robot::reset()
{
	// The pelvis first, then each part from the one it hangs from, so each
	// joint's two anchors start together (Bullet's robot started some up to
	// 10 cm apart, and its joints pulled them together in the first steps)
	auto place = [this](Part part, const Vector3& position)
	{
		cyclone::RigidBody& body = m_bodies[part];
		body.setPosition(position);
		body.setOrientation(standingOrientation(part));
		body.setVelocity(Vector3());
		body.setRotation(Vector3());
		body.clearAccumulators();
		body.setAwake();
		body.calculateDerivedData();
	};
	place(pelvis, pelvisStart);
	auto placeFrom = [&](Part parent, const Vector3& atParent, Part child, const Vector3& atChild)
	{
		cyclone::Matrix3 turned;
		turned.setOrientation(standingOrientation(child));
		place(child, m_bodies[parent].getPointInWorldSpace(atParent) - turned.transform(atChild));
	};
	for(const HingeShape& shape : hingeShapes)
		placeFrom(shape.parent, shape.atParent, shape.child, shape.atChild);
	placeFrom(spine, neckAtSpine, head, neckAtHead);

	for(cyclone::CollisionCapsule& capsule : m_capsules)
		capsule.calculateInternals();
	for(cyclone::CollisionBox& foot : m_feet)
		foot.calculateInternals();

	// A fresh start for warm starting
	for(cyclone::Joint* joint : m_jointList)
	{
		joint->impulse = Vector3();
		std::fill(std::begin(joint->angularImpulse), std::end(joint->angularImpulse), (real)0);
	}
	for(cyclone::HingeJoint& hinge : m_hinges)
		hinge.setMotor(0, 0);
}

void Robot::setMotors(std::span<const double, hingeCount> outputs)
{
	for(unsigned i = 0; i < hingeCount; i++)
	{
		const double output = outputs[i];
		const double strength = output < 0 ? hingeShapes[i].negativeStrength : hingeShapes[i].positiveStrength;
		const double speed = output > 0 ? motorSpeed : (output < 0 ? -motorSpeed : 0);
		const double impulse = std::min(strength, std::abs(output));
		m_hinges[i].setMotor((real)speed, (real)(impulse * motorTorquePerStrength));
	}
}

void Robot::integrate(real duration)
{
	for(cyclone::RigidBody& body : m_bodies)
		body.integrate(duration);
	for(cyclone::CollisionCapsule& capsule : m_capsules)
		capsule.calculateInternals();
	for(cyclone::CollisionBox& foot : m_feet)
		foot.calculateInternals();
}

const cyclone::CollisionPrimitive& Robot::shape(unsigned part) const
{
	if(part < capsuleCount)
		return m_capsules[part];
	return m_feet[part - leftFoot];
}

void Robot::generateContacts(cyclone::CollisionData& data, unsigned maxContacts) const
{
	using cyclone::CollisionDetector;
	data.reset(maxContacts);
	data.friction = friction;
	data.restitution = 0;
	data.tolerance = (real)0.1;

	cyclone::CollisionPlane ground;
	ground.direction = Vector3(0, 1, 0);
	ground.offset = 0;
	for(unsigned part = 0; part < partCount && data.hasMoreContacts(); part++)
	{
		if(part < capsuleCount)
			CollisionDetector::capsuleAndHalfSpace(m_capsules[part], ground, &data);
		else
			CollisionDetector::boxAndHalfSpace(m_feet[part - leftFoot], ground, &data);
	}

	for(unsigned a = 0; a < partCount; a++)
	{
		for(unsigned b = a + 1; b < partCount && data.hasMoreContacts(); b++)
		{
			// Only parts that aren't joined, and are near enough to touch
			if(!m_collide[a][b])
				continue;
			const cyclone::CollisionPrimitive& one = shape(a);
			const cyclone::CollisionPrimitive& two = shape(b);
			const double apart = (one.getAxis(3) - two.getAxis(3)).magnitude();
			if(apart > reach(a) + reach(b))
				continue;
			if(b < capsuleCount) // (b > a, so both are capsules)
				CollisionDetector::capsuleAndCapsule(m_capsules[a], m_capsules[b], &data);
			else if(a < capsuleCount)
				CollisionDetector::capsuleAndBox(m_capsules[a], m_feet[b - leftFoot], &data);
			else
				CollisionDetector::boxAndBox(m_feet[0], m_feet[1], &data);
		}
	}
}

double Robot::lowestPoint() const
{
	double lowest = 1e9;
	for(unsigned part = 0; part < partCount; part++)
	{
		const cyclone::RigidBody& body = m_bodies[part];
		const cyclone::Matrix3 orientation = body.getOrientationNow();
		const double centre = body.getPosition().y;
		if(part < capsuleCount)
		{
			// The lower end's centre, less the radius
			const double endHeight = std::abs(orientation.transform(Vector3(0, 1, 0)).y) * m_capsules[part].halfLength;
			lowest = std::min<double>(lowest, centre - endHeight - m_capsules[part].radius);
		}
		else
		{
			// The lowest corner: each half-size, along however much of it points down
			double depth = 0;
			for(unsigned axis = 0; axis < 3; axis++)
			{
				Vector3 direction;
				direction[axis] = 1;
				depth += std::abs(orientation.transform(direction).y) * footHalfSize[axis];
			}
			lowest = std::min(lowest, centre - depth);
		}
	}
	return lowest;
}

double Robot::uprightness() const
{
	return m_bodies[spine].getOrientationNow().transform(Vector3(0, 1, 0)).y;
}

double Robot::faceDownness() const
{
	// It faces -z, so lying on its front its +z (its back) points up
	return m_bodies[spine].getOrientationNow().transform(Vector3(0, 0, 1)).y;
}

double Robot::angle(Hinge hinge) const
{
	return m_hinges[hinge].getAngle();
}
