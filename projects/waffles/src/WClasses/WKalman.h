/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
    anonymous contributors,

  to the public domain (http://creativecommons.org/publicdomain/zero/1.0/).

  Note that some moral obligations still exist in the absence of legal ones.
  For example, it would still be dishonest to deliberately misrepresent the
  origin of a work. Although we impose no legal requirements to obtain a
  license, it is beseeming for those who build on the works of others to
  give back useful improvements, or find a way to pay it forward. If
  you would like to cite us, a published paper about Waffles can be found
  at http://jmlr.org/papers/volume12/gashler11a/gashler11a.pdf. If you find
  our code to be useful, the Waffles team would love to hear how you use it.
*/

#ifndef __GKALMAN_H__
#define __GKALMAN_H__

#include <cstring>
#include "WVec.h"

namespace WClasses
{

class WMatrix;

/// This is an implementation of the Extended Kalman Filter. This class
/// is used by alternately calling advance and correct.
class WExtendedKalmanFilter
{
protected:
	size_t m_stateDims, m_obsDims;
	WVec m_x; // estimated state
	WVec m_z; // observation buffer
	WVec m_zz; // state correction buffer
	WMatrix* m_pP; // estimated covariance of state

public:
	WExtendedKalmanFilter(int stateDims, int obsDims, int controlDims);
	~WExtendedKalmanFilter();

	/// Use this to get and/or set the estimated state
	WVec& state() { return m_x; }

	/// Use this to get and/or set the estimated covariance of the state
	WMatrix* stateCovariance() { return m_pP; }

	/// Advance to the next time step. pControl is the control vector
	/// to apply to the system at the current time step. pA is the
	/// Jacobian of the transition function at the current state, and
	/// with this control vector. Each column in pA represents the
	/// change in f(x,u) over the change in one of the dimensions of
	/// x, where f is the transition function, x is the state, and u
	/// is the control vector.
	void advance(const WVec& control, WMatrix* pA);

	/// Correct the estimates of state and covariance based on the
	/// observations made in the new state (and assuming the observation function
	/// is locally linear around the current state). pH is the Jacobian of
	/// the observation function at the current
	/// state. Each column in pH represents the change in h(x) over the change
	/// in one of the dimensions of x, where h is the observation function,
	/// and x is the state.
	void correct(const WVec& observation, WMatrix* pH);

	/// Computes the transition function. (Adjust the values in pInOutState
	/// as directed by pControl.)
	virtual void transition(WVec& inOutState, const WVec& control) = 0;

	/// Computes the observation function. Put results in pOutObs.
	virtual void observation(WVec& outObs, const WVec& state) = 0;

	/// pInOutCov is a nxn matrix, where n is the number of state dims.
	/// This method should add the covariance of the transition noise to pInOutCov.
	virtual void addTransitionNoise(WMatrix* pInOutCov) = 0;

	/// pInOutCov is a mxm matrix, where m is the number of observation dims.
	/// This method should add the covariance of the observation noise to pInOutCov.
	virtual void addObservationNoise(WMatrix* pInOutCov) = 0;
};

} // namespace WClasses

#endif // __GKALMAN_H__
