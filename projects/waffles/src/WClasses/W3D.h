/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
    Eric Moyer,
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

#ifndef __G3D_H__
#define __G3D_H__

#include <math.h>
#include "WError.h"
#include <vector>

namespace WClasses
{

typedef double W3DReal;
class W3DMatrix;
class WDomNode;
class WDom;
class WRand;
class WImage;
class WBBAugmented;


/// Represents a 3D vector
class W3DVector
{
public:
	W3DReal m_vals[3];

	W3DVector()
	{
	}

	// Copied as three numbers (trivially copyable)
	W3DVector(const W3DVector& that) = default;

	/// Initializes the values to <x,y,z>
	W3DVector(W3DReal x, W3DReal y, W3DReal z)
	{
		m_vals[0] = x;
		m_vals[1] = y;
		m_vals[2] = z;
	}

	W3DVector& operator=(const W3DVector& that) = default;

	/// \brief Returns a reference to the specified element.
	inline W3DReal& operator[](size_t index)
	{
		WAssert(index < 3);
		return m_vals[index];
	}

	/// \brief Returns a const reference to the specified element
	inline const W3DReal& operator[](size_t index) const
	{
		WAssert(index < 3);
		return m_vals[index];
	}

	/// Returns the vector as an array of reals
	W3DReal* vals() { return m_vals; }

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const;

	/// Load this object from a DOM.
	void deserialize(WDomNode* pNode);

	/// Returns true iff all three elements are equal.
	bool isEqual(const W3DVector& that) const
	{
		return m_vals[0] == that.m_vals[0] && m_vals[1] == that.m_vals[1] && m_vals[2] == that.m_vals[2];
	}

	/// Makes a copy of pThat
	void copy(const W3DVector& that)
	{
		m_vals[0] = that[0];
		m_vals[1] = that[1];
		m_vals[2] = that[2];
	}

	/// Sets the values of this vector
	void set(W3DReal x, W3DReal y, W3DReal z)
	{
		m_vals[0] = x;
		m_vals[1] = y;
		m_vals[2] = z;
	}

	/// Normalizes this vector
	inline void normalize()
	{
		W3DReal mag = (W3DReal)sqrt(squaredMag());
		if(mag == 0)
			throw Ex("Can't normalize a vector with zero magnitude");
		m_vals[0] /= mag;
		m_vals[1] /= mag;
		m_vals[2] /= mag;
	}

	/// Picks a random vector from a spherical distribution
	void makeRandom(WRand* pRand);

	/// returns the squared distance between this and pThat
	inline W3DReal squaredDist(const W3DVector& that) const
	{
		return (that[0] - m_vals[0]) * (that[0] - m_vals[0]) + (that[1] - m_vals[1]) * (that[1] - m_vals[1]) + (that[2] - m_vals[2]) * (that[2] - m_vals[2]);
	}

	/// returns the squared magnitude of this vector
	inline double squaredMag() const
	{
		return (m_vals[0] * m_vals[0] + m_vals[1] * m_vals[1] + m_vals[2] * m_vals[2]);
	}

	/// adds that to this
	inline void add(const W3DVector& that)
	{
		m_vals[0] += that[0];
		m_vals[1] += that[1];
		m_vals[2] += that[2];
	}

	/// adds mag * that to this
	inline void add(W3DReal mag, const W3DVector& that)
	{
		m_vals[0] += mag * that[0];
		m_vals[1] += mag * that[1];
		m_vals[2] += mag * that[2];
	}

	inline void add(W3DReal dx, W3DReal dy, W3DReal dz)
	{
		m_vals[0] += dx;
		m_vals[1] += dy;
		m_vals[2] += dz;
	}

	/// subtracts pThat from this
	inline void subtract(const W3DVector& that)
	{
		m_vals[0] -= that[0];
		m_vals[1] -= that[1];
		m_vals[2] -= that[2];
	}

	/// multiplies this by mag
	inline void multiply(W3DReal mag)
	{
		m_vals[0] *= mag;
		m_vals[1] *= mag;
		m_vals[2] *= mag;
	}

	/// this = pMatrix * pVector
	void multiply(W3DMatrix* pMatrix, W3DVector* pVector);

	/// returns the dot product of this and that
	inline W3DReal dotProduct(const W3DVector& that) const
	{
		return m_vals[0] * that[0] + m_vals[1] * that[1] + m_vals[2] * that[2];
	}

	/// this = pA x pB
	inline void crossProduct(W3DVector& a, W3DVector& b)
	{
		m_vals[0] = a[1] * b[2] - a[2] * b[1];
		m_vals[1] = a[2] * b[0] - a[0] * b[2];
		m_vals[2] = a[0] * b[1] - a[1] * b[0];
	}

	/// Sets this to the normal vector of the triangle specified by three points
	inline void triangleNormal(const W3DVector& point1, const W3DVector& point2, const W3DVector& point3)
	{
		W3DVector a(point2);
		a.subtract(point1);
		W3DVector b(point3);
		b.subtract(point1);
		crossProduct(a, b);
		normalize();
	}

	/// Computes the plane equation (ax + by + cz + d = 0) of the
	/// specified triangle. this = (a, b, c) and *pOutD = d.
	void planeEquation(const W3DVector& point1, const W3DVector& point2, const W3DVector& point3, W3DReal* pOutD)
	{
		triangleNormal(point1, point2, point3);
		*pOutD = -(this->dotProduct(point1));
	}

	/// Sets this to the reflection of pRay on a surface with
	/// normal vector pNormal.
	void reflectionVector(const W3DVector& ray, const W3DVector& normal);

	/// *pYaw and *pPitch are in radians
	void yawAndPitch(W3DReal* pYaw, W3DReal* pPitch) const;

	/// dYaw and dPitch are in radians
	inline void fromYawAndPitch(W3DReal dYaw, W3DReal dPitch)
	{
		m_vals[1] = (W3DReal)sin(dPitch);
		double dTmp = (W3DReal)cos(dPitch);
		m_vals[0] = (W3DReal)(dTmp * sin(dYaw));
		m_vals[2] = (W3DReal)(dTmp * cos(dYaw));
	}
};



/// Represents a 3x3 matrix
class W3DMatrix
{
public:
	W3DVector m_rows[3];

	/// serializes this matrix
	WDomNode* serialize(WDom* pDoc) const;

	/// deserializes this matrix
	void deserialize(WDomNode* pNode);

	/// sets this to the identity matrix
	inline void setToIdentity()
	{
		m_rows[0].set(1, 0, 0);
		m_rows[1].set(0, 1, 0);
		m_rows[2].set(0, 0, 1);
	}

	/// copies pThat
	inline void copy(W3DMatrix* pThat)
	{
		m_rows[0].copy(pThat->m_rows[0]);
		m_rows[1].copy(pThat->m_rows[1]);
		m_rows[2].copy(pThat->m_rows[2]);
	}

	/// multiplies this by d
	inline void multiply(double d)
	{
		m_rows[0].multiply(d);
		m_rows[1].multiply(d);
		m_rows[2].multiply(d);
	}

	/// pVecOut = this x pVecIn
	inline void multiply(const W3DVector* pVecIn, W3DVector* pVecOut)
	{
		pVecOut->m_vals[0] = m_rows[0].dotProduct(*pVecIn);
		pVecOut->m_vals[1] = m_rows[1].dotProduct(*pVecIn);
		pVecOut->m_vals[2] = m_rows[2].dotProduct(*pVecIn);
	}

	/// Computes the dot-product of pVec with the specified column of this matrix
	inline double dotColumn(const W3DVector* pVec, int column) const
	{
		return pVec->m_vals[0] * m_rows[0].m_vals[column] + pVec->m_vals[1] * m_rows[1].m_vals[column] + pVec->m_vals[2] * m_rows[2].m_vals[column];
	}

	/// this = pA x pB
	inline void multiply(const W3DMatrix* pA, const W3DMatrix* pB)
	{
		m_rows[0].m_vals[0] = pB->dotColumn(&pA->m_rows[0], 0);
		m_rows[0].m_vals[1] = pB->dotColumn(&pA->m_rows[0], 1);
		m_rows[0].m_vals[2] = pB->dotColumn(&pA->m_rows[0], 2);
		m_rows[1].m_vals[0] = pB->dotColumn(&pA->m_rows[1], 0);
		m_rows[1].m_vals[1] = pB->dotColumn(&pA->m_rows[1], 1);
		m_rows[1].m_vals[2] = pB->dotColumn(&pA->m_rows[1], 2);
		m_rows[2].m_vals[0] = pB->dotColumn(&pA->m_rows[2], 0);
		m_rows[2].m_vals[1] = pB->dotColumn(&pA->m_rows[2], 1);
		m_rows[2].m_vals[2] = pB->dotColumn(&pA->m_rows[2], 2);
	}

	/// Creates a matrix comprised of a random set of orthonormal basis vectors
	void makeRandom(WRand* pRand);

	/// Creates a matrix for rotating about the specified axis
	void makeAxisRotationMatrix(int axis, double radians);
};




/// This camera assumes the canvas is specified in cartesian
/// coordinates. The 3D space is based on a right-handed coordinate
/// system. (So if x goes to the right and y goes up, then z comes
/// out of the screen toward you.)
class WCamera
{
protected:
	W3DVector m_lookFromPoint;
	W3DVector m_viewSideVector; // to the right
	W3DVector m_viewUpVector;
	W3DVector m_lookDirection;
	W3DReal m_halfViewHeight; // tan(viewAngle / 2)
	int m_nWidth, m_nHeight;

public:
	/// width and height specify the size of the image that this camera will produce
	WCamera(int width, int height)
	: m_lookFromPoint(0, 0, 10),
	  m_viewSideVector(1, 0, 0),
	  m_viewUpVector(0, 1, 0),
	  m_lookDirection(0, 0, -1),
	  m_halfViewHeight(1.0),
	  m_nWidth(width),
	  m_nHeight(height)
	{
	}

	/// deserializing constructor
	WCamera(WDomNode* pNode);

	virtual ~WCamera()
	{
	}

	/// serializes this object
	virtual WDomNode* serialize(WDom* pDoc) const;

	/// Specifies the size of the 2-D image this camera will produce
	void setImageSize(int width, int height)
	{
		m_nWidth = width;
		m_nHeight = height;
	}

	/// Returns the width of the 2-D image this camera will produce
	inline int imageWidth() { return m_nWidth; }

	/// Returns the height of the 2-D image this camera will produce
	inline int imageHeight() { return m_nHeight; }

	/// Returns a reference to the location of this camera. (You can set the
	/// values in the vector this returns to move the camera.)
	W3DVector* lookFromPoint() { return &m_lookFromPoint; }

	/// Specifies the direction that the camera faces, and the roll in radians.
	/// (If rollRads is zero, then the horizon of the XZ-plane would appear as a horizontal
	/// line with positive Y above and negative Y below.)
	void setDirection(W3DVector* pDirection, W3DReal rollRads);

	/// If pUpVector is not orthogonal to pDirection, it will be changed
	/// to the nearest vector that is orthogonal to pDirection.
	void setDirection(W3DVector* pDirection, W3DVector* pUpVector);

	/// Returns the direction in which this camera is facing
	const W3DVector* lookDirection() { return &m_lookDirection; }

	/// Returns the up vector with respect to this camera
	const W3DVector* viewUpVector() { return &m_viewUpVector; }

	/// Returns the right side vector with respect to this camera
	const W3DVector* viewSideVector() { return &m_viewSideVector; }

	/// This is the vertical view angle in radians
	void setViewAngle(W3DReal val);

	/// Returns tan(viewAngle / 2)
	W3DReal halfViewHeight() { return m_halfViewHeight; }

	/// Projects the 3D point onto the canvas. The x and y position
	/// in the output vector specify the location where the point
	/// projects onto the camera's canvas (positive x goes to the
	/// right with 0 at the left side, positive y goes up with 0 at
	/// the bottom). The z position specifies the distance from the camera.
	/// If the z position is <= 0, then x and y are set to 0.
	void project(const W3DVector* pPoint, W3DVector* pOut);

	/// Computes the direction that a ray must travel from lookFromPoint()
	/// for the specified pixel coordinates (relative to the bottom-left corner
	/// of the view image). Note that the ray is not normalized.
	void computeRayDirection(int x, int y, W3DVector* pOutRay);
};

/// This is a billboard (a 2-D image in a 3-D world) for use with WBillboardWorld.
/// You can set m_repeatX and/or m_repeatY to make the image repeat across the billboard.
class WBillboard
{
public:
	/// The image associated with this billboard
	WImage* m_pImage;

	/// The number of times the image repeats horizontally on the billboard
	int m_repeatX;

	/// The number of times the image repeats vertically on the billboard
	int m_repeatY;

	/// The origin corner of the billboard (where the bottom-left corner of the image is displayed)
	W3DVector m_origin;

	/// The horizontal vector of the billboard. The magnitude of this vector specifies
	/// the width of the billboard.
	W3DVector m_x;

	/// The vertical vector of the billboard. The magnitude of this vector specifies
	/// the height of the billboard.
	W3DVector m_y;

	/// Does not take ownership of pImage.
	WBillboard(WImage* pImage);

	/// Ensures that m_y is orthogonal to m_x, and adjusts the magnitude of m_y to restore
	/// the aspect ratio of the billboard image
	void adjustHeightToRestoreAspect();
};

/// This class represents a world of billboards, and provides a rendering engine.
class WBillboardWorld
{
protected:
	std::vector<WBillboard*> m_billboards;

public:
	WBillboardWorld();
	~WBillboardWorld();

	/// Takes ownership of pBB
	void addBillboard(WBillboard* pBB);

	/// Draws all the billboards onto pImage from the perspective of the camera, and
	/// sets the values in pDepthMap to indicate the distance to each pixel.
	/// (pDepthMap should be a buffer with the same number of elements as pixels in pImage.)
	/// The pixels in pDepthMap correspond with the image left-to-right, bottom-to-top.
	void draw(WImage* pImage, double* pDepthMap, WCamera& camera);

protected:
	void drawSection(WImage* pImage, double* pDepthMap, WCamera& camera, WBBAugmented& bb, W3DVector* pBotLeft, W3DVector* pBotRight, W3DVector* pTopRight, W3DVector* pTopLeft);
};

/*
/// This is for making a 3D scene ala QTVR
class WBoxScene
{
protected:
	WImage m_image;
	float* m_pDepthMap;

public:
	WBoxScene(int nWidth)
	{
		m_image.setSize(nWidth, 6 * nWidth);
		m_pDepthMap = new float[nWidth * 6 * nWidth];
	}

	~WBoxScene()
	{
		delete[] m_pDepthMap;
	}

	WImage* GetImage() { return &m_image; }
	float* GetDepthMap() { return m_pDepthMap; }

	void RayToPoint(float* pnOutX, float* pnOutY, W3DVector* pRay)
	{
		int nFrame, u, v;
		W3DReal xx = pRay->m_vals[0] * pRay->m_vals[0];
		W3DReal yy = pRay->m_vals[1] * pRay->m_vals[1];
		W3DReal zz = pRay->m_vals[2] * pRay->m_vals[2];
		if(xx >= yy && xx >= zz)
		{
			nFrame = 0;
			u = 2;
			v = 1;
		}
		else if(yy >= zz)
		{
			u = 2;
			nFrame = 1;
			v = 0;
		}
		else
		{
			u = 0;
			v = 1;
			nFrame = 2;
		}
		int halfWidth = m_image.width() / 2;
		float t = (float)halfWidth / (float)pRay->m_vals[nFrame];
		*pnOutX = t * (float)pRay->m_vals[u] + halfWidth;
		*pnOutY = t * (float)pRay->m_vals[v] + halfWidth;
		if(pRay->m_vals[nFrame] < 0)
			nFrame += 3;
		(*pnOutY) += m_image.width() * nFrame;
	}

	/// The ray this returns is NOT normalized
	void PointToRay(W3DVector* pRay, int x, int y)
	{
		int nFrame = y / m_image.width();
		y = y % m_image.width();
		if(nFrame >= 3)
		{
			nFrame -= 3;
			pRay->m_vals[nFrame] = -1;
			x = m_image.width() - x;
			y = m_image.width() - y;
		}
		else
			pRay->m_vals[nFrame] = 1;
		int u = (nFrame == 2 ? 0 : 2);
		int v = (nFrame == 1 ? 0 : 1);
		pRay->m_vals[u] = (float)(x * 2) / m_image.width() - 1;
		pRay->m_vals[v] = (float)(y * 2) / m_image.width() - 1;
	}
};
*/

} // namespace WClasses

#endif // __G3D_H__
