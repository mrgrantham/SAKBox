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

#ifndef __GRAYTRACE_H__
#define __GRAYTRACE_H__

#include <stddef.h>
#include <math.h>
#include "W3D.h"
#include "WRand.h"
#include <deque>
#include <vector>
#include <cmath>

namespace WClasses
{

class WRayTraceLight;
class WRayTraceMaterial;
class WRayTraceObject;
class WRayTraceRay;
class WRayTraceScene;
class WRayTraceTriMesh;
class WRayTraceBoundingBoxBase;
class WNodeHashTable;
class WImage;

/// This class represents a color. It's more precise than WColor, but takes up more
/// memory. Note that the ray tracer ignores the alpha channel because the material
/// specifies a unique transmission color.
class WRayTraceColor
{
public:
	W3DReal a, r, g, b;

	WRayTraceColor()
	: a(1), r(0), g(0), b(0)
	{
	}

	WRayTraceColor(WRayTraceColor* pThat)
	{
		a = pThat->a;
		r = pThat->r;
		g = pThat->g;
		b = pThat->b;
	}

	WRayTraceColor(unsigned int c);

	WRayTraceColor(W3DReal alpha, W3DReal red, W3DReal green, W3DReal blue)
	{
		set(alpha, red, green, blue);
	}

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const;

	/// Load this object from a DOM.
	void deserialize(WDomNode* pNode);

	bool isBlack()
	{
		if(r == 0 && g == 0 && b == 0)
			return true;
		else
			return false;
	}

	void set(W3DReal alpha, W3DReal red, W3DReal green, W3DReal blue)
	{
		a = alpha;
		r = red;
		g = green;
		b = blue;
	}

	void set(unsigned int c);

	void copy(WRayTraceColor* pThat)
	{
		a = pThat->a;
		r = pThat->r;
		g = pThat->g;
		b = pThat->b;
	}

	void add(WRayTraceColor* pThat)
	{
		a = std::max(a, pThat->a);
		r += pThat->r;
		g += pThat->g;
		b += pThat->b;
	}

	void clip()
	{
		r = std::min((W3DReal)1, r);
		g = std::min((W3DReal)1, g);
		b = std::min((W3DReal)1, b);
	}

	void multiply(W3DReal mag)
	{
		r *= mag;
		g *= mag;
		b *= mag;
	}

	void multiply(WRayTraceColor* pThat)
	{
		a *= pThat->a;
		r *= pThat->r;
		g *= pThat->g;
		b *= pThat->b;
	}

	unsigned int color();

	void makeSliderColor(float f, WRayTraceColor* pDiffuseColor);
};




/// Represents the camera for a ray tracing scene
class WRayTraceCamera : public WCamera
{
protected:
	W3DReal m_focalDistance;
	W3DReal m_lensDiameter;
	int m_maxDepth; // max number of ray bounces

public:
	WRayTraceCamera(int width, int height) : WCamera(width, height)
	{
		m_focalDistance = 0;
		m_lensDiameter = 0;
		m_maxDepth = 4;
	}

	WRayTraceCamera(WDomNode* pNode);

	~WRayTraceCamera() override
	{
	}

	WDomNode* serialize(WDom* pDoc) const override;
	void setMaxDepth(int val) { m_maxDepth = val; }
	void setFocalDistance(W3DReal val) { m_focalDistance = val; }
	void setLensDiameter(W3DReal val) { m_lensDiameter = val; }
	inline int maxDepth() { return m_maxDepth; }
	inline W3DReal focalDistance() { return m_focalDistance; }
	inline W3DReal lensDiameter() { return m_lensDiameter; }
};



/// Represents a scene that you can ray-trace
class WRayTraceScene
{
public:
	enum RenderMode
	{
		FAST_RAY_TRACE,
		QUALITY_RAY_TRACE,
		PATH_TRACE,
	};

protected:
	WRayTraceColor m_backgroundColor;
	WRayTraceColor m_ambientLight;
	std::vector<WRayTraceMaterial*> m_materials;
	std::vector<WRayTraceTriMesh*> m_meshes; // meshes reference a material
	std::vector<WRayTraceObject*> m_objects; // triangles reference a mesh, spheres reference a material
	std::vector<WRayTraceLight*> m_lights; // area lights reference an object
	WRayTraceCamera* m_pCamera;
	WRayTraceBoundingBoxBase* m_pBoundingBoxTree;
	W3DReal m_toneMappingConstant;

	/// Rendering values
	RenderMode m_eMode;
	WImage* m_pImage;
	W3DReal* m_pDistanceMap;
	int m_nY;
	W3DVector m_pixSide;
	W3DVector m_pixDX;
	W3DVector m_pixDY;
	WRand* m_pRand;

public:
	WRayTraceScene(WRand* pRand);
	WRayTraceScene(WDomNode* pNode, WRand* pRand);
	~WRayTraceScene();

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const;

	/// Deletes all the materials and objects. (Leaves the lights
	/// and camera as they are.)
	void flushObjects();

	/// Swap all matrials, meshes and objects with the other scene. (Leaves
	/// the camera and lights as they are.)
	void swapObjects(WRayTraceScene* pOther);

	/// Specify whether to emphasize quality or speed
	void setRenderMode(RenderMode eMode) { m_eMode = eMode; }

	/// This method calls RenderBegin, then calls RenderLine until
	/// the whole image has been rendered.
	void render();

	/// Call this before calling RenderLine(). It resets the image and
	/// computes values necessary for rendering.
	void renderBegin();

	/// Call this to render a singe horizontal line of the image. Returns true if there's
	/// still more rendering to do. Returns false if it's done. You must call RenderBegin()
	/// once before you start calling this method.
	bool renderLine();

	/// This draws a wire frame of the scene. This is a fast way to ensure your
	/// camera is looking where you think it is looking before you perform a long render.
	void drawWireFrame();

	/// This calls RenderBegine and then renders a single pixel. It's not efficient
	/// to call this method for every pixel. The only purpose for this method is to
	/// make debugging the ray tracer easier. Pick a pixel that isn't rendered the way
	/// you want and step through the ray tracing process to see why.
	unsigned int renderSinglePixel(int x, int y);

	/// Returns the rendered image (or partially rendered image). Returns NULL if Render()
	/// or RenderBegin() has not been called yet.
	WImage* image() { return m_pImage; }

	WImage* releaseImage()
	{
		WImage* pImage = m_pImage;
		m_pImage = nullptr;
		return pImage;
	}

	void setBackgroundColor(W3DReal a, W3DReal r, W3DReal g, W3DReal b)
	{
		m_backgroundColor.set(a, r, g, b);
	}

	void setAmbientLight(W3DReal r, W3DReal g, W3DReal b)
	{
		m_ambientLight.set(1, r, g, b);
	}

	WRayTraceColor* ambientLight() { return &m_ambientLight; }

	WRayTraceCamera* camera()
	{
		return m_pCamera;
	}

	WRand* rand() { return m_pRand; }
	void activateDistanceMap();
	W3DReal* distanceMap() { return m_pDistanceMap; }
	void addMaterial(WRayTraceMaterial* pMaterial);
	void addMesh(WRayTraceTriMesh* pMesh);
	void addObject(WRayTraceObject* pObject);
	void addLight(WRayTraceLight* pLight);
	size_t materialCount();
	size_t meshCount();
	size_t objectCount();
	size_t lightCount();
	WRayTraceMaterial* material(size_t n);
	WRayTraceTriMesh* mesh(size_t n);
	WRayTraceObject* object(size_t n);
	WRayTraceLight* light(size_t n);
	std::vector<WRayTraceMaterial*>& materials() { return m_materials; }
	WRayTraceColor* backgroundColor() { return &m_backgroundColor; }
	WRayTraceBoundingBoxBase* boundingBoxTree() { return m_pBoundingBoxTree; }
	void setToneMappingConstant(W3DReal c) { m_toneMappingConstant = c; }
	size_t materialIndex(WRayTraceMaterial* pMaterial) const;
	size_t meshIndex(WRayTraceTriMesh* pMesh) const;
	size_t objectIndex(WRayTraceObject* pObj) const;

	unsigned int renderPixel(WRayTraceRay* pRay, W3DVector* pScreenPoint, W3DReal* pDistance);
	unsigned int renderPixelAntiAliassed(WRayTraceRay* pRay, W3DVector* pScreenPoint, W3DReal* pDistance);
	unsigned int renderPixelPathTrace(WRayTraceRay* pRay, W3DVector* pScreenPoint);
};



/// Represents a source of light in a ray-tracing scene
class WRayTraceLight
{
public:
	enum LightType
	{
		Directional = 0,
		Point,
		Area,
	};

protected:
	WRayTraceColor m_color;

public:
	WRayTraceLight(W3DReal r, W3DReal g, W3DReal b);
	WRayTraceLight(WDomNode* pNode);
	virtual ~WRayTraceLight();

	static WRayTraceLight* deserialize(WDomNode* pNode, WRayTraceScene* pScene);
	virtual WDomNode* serialize(WDom* pDoc, const WRayTraceScene* pScene) const = 0;
	virtual LightType lightType() const = 0;
	virtual void colorContribution(WRayTraceScene* pScene, WRayTraceRay* pRay, WRayTraceMaterial* pMaterial, bool bSpecular) = 0;

protected:
	WDomNode* baseDomNode(WDom* pDoc) const;
};




/// Represents directional light in a ray-tracing scene
class WRayTraceDirectionalLight : public WRayTraceLight
{
protected:
	W3DVector m_direction;
	W3DReal m_jitter;

public:
	/// Specify the direction to the light, not the direction the light travels
	WRayTraceDirectionalLight(W3DReal dx, W3DReal dy, W3DReal dz, W3DReal r, W3DReal g, W3DReal b, W3DReal jitter);
	WRayTraceDirectionalLight(WDomNode* pNode);
	~WRayTraceDirectionalLight() override;

	WDomNode* serialize(WDom* pDoc, const WRayTraceScene* pScene) const override;
	LightType lightType() const override { return Directional; }
	void colorContribution(WRayTraceScene* pScene, WRayTraceRay* pRay, WRayTraceMaterial* pMaterial, bool bSpecular) override;
};



/// Represents a point light in a ray-tracing scene
class WRayTracePointLight : public WRayTraceLight
{
protected:
	W3DVector m_position;
	W3DReal m_jitter;

public:
	WRayTracePointLight(W3DReal x, W3DReal y, W3DReal z, W3DReal r, W3DReal g, W3DReal b, W3DReal jitter);
	WRayTracePointLight(WDomNode* pNode);
	~WRayTracePointLight() override;

	W3DVector* position() { return &m_position; }
	WDomNode* serialize(WDom* pDoc, const WRayTraceScene* pScene) const override;
	LightType lightType() const override { return Point; }
	void colorContribution(WRayTraceScene* pScene, WRayTraceRay* pRay, WRayTraceMaterial* pMaterial, bool bSpecular) override;
};



/// Represents a light source with area
class WRayTraceAreaLight : public WRayTraceLight
{
protected:
	WRayTraceObject* m_pObject;

public:
	WRayTraceAreaLight(WRayTraceObject* pObject, W3DReal r, W3DReal g, W3DReal b);
	WRayTraceAreaLight(WDomNode* pNode, WRayTraceScene* pScene);
	~WRayTraceAreaLight() override;

	WDomNode* serialize(WDom* pDoc, const WRayTraceScene* pScene) const override;
	LightType lightType() const override { return Area; }
	void colorContribution(WRayTraceScene* pScene, WRayTraceRay* pRay, WRayTraceMaterial* pMaterial, bool bSpecular) override;
};



class WRayTraceMaterial
{
public:
	enum MaterialType
	{
		Physical,
		Image,
	};

	enum ColorType
	{
		Diffuse = 0, // if the ray can see the light from the point of intersection
		Specular, // if the ray reflects into a light
		Reflective, // colors reflected by the object
		Transmissive, // colors that pass through the object
		Ambient, // light that is always present in every direction
		Emissive, // for object lights or flourescent objects
		Color_Type_Count,
	};

	WRayTraceMaterial();
	virtual ~WRayTraceMaterial();

	static WRayTraceMaterial* deserialize(WDomNode* pNode);

	virtual WDomNode* serialize(WDom* pDoc) const = 0;
	virtual MaterialType materialType() const = 0;
	virtual WRayTraceColor* color(ColorType eType, WRayTraceRay* pRay) = 0;
	virtual W3DReal indexOfRefraction() = 0;
	virtual W3DReal specularExponent() = 0;
	virtual W3DReal glossiness() = 0; // how much to jitter reflected rays
	virtual W3DReal cloudiness() = 0; // how much to jitter transmitted rays
	virtual bool isSame(WRayTraceMaterial* pThat) = 0;
	virtual WRayTraceMaterial* copy() = 0;

	void computeColor(WRayTraceScene* pScene, WRayTraceRay* pRay, bool bAmbient, bool bSpecular);
};


/// Represents the material of which an object is made in a ray-tracing scene
class WRayTracePhysicalMaterial : public WRayTraceMaterial
{
protected:
	WRayTraceColor m_colors[Color_Type_Count];
	W3DReal m_indexOfRefraction;
	W3DReal m_specularExponent;
	W3DReal m_glossiness;
	W3DReal m_cloudiness;

public:
	WRayTracePhysicalMaterial();
	WRayTracePhysicalMaterial(WDomNode* pNode);
	~WRayTracePhysicalMaterial() override;

	WDomNode* serialize(WDom* pDoc) const override;
	MaterialType materialType() const override { return Physical; }

	/// Ignores pRay and returns the color of the specified type. (pRay is
	/// used by image-texture materials to determine which pixel applies.)
	WRayTraceColor* color(ColorType eType, WRayTraceRay* pRay) override;

	W3DReal indexOfRefraction() override { return m_indexOfRefraction; }
	W3DReal specularExponent() override { return m_specularExponent; }
	W3DReal glossiness() override { return m_glossiness; }
	W3DReal cloudiness() override { return m_cloudiness; }
	bool isSame(WRayTraceMaterial* pThat) override;
	WRayTraceMaterial* copy() override;

	void setColor(ColorType eType, W3DReal r, W3DReal g, W3DReal b);
	void setColor(ColorType eType, WRayTraceColor* pCol);
	void setIndexOfRefraction(W3DReal val) { m_indexOfRefraction = val; }
	void setSpecularExponent(W3DReal val) { m_specularExponent = val; }
	void setGlossiness(W3DReal val) { m_glossiness = val; }
	void setCloudiness(W3DReal val) { m_cloudiness = val; }
};


class WRayTraceImageTexture : public WRayTraceMaterial
{
protected:
	WImage* m_pTextureImage;
	bool m_bDeleteTextureImage;
	WRayTraceColor m_col;

public:
	WRayTraceImageTexture();
	WRayTraceImageTexture(WDomNode* pNode);
	~WRayTraceImageTexture() override;

	WDomNode* serialize(WDom* pDoc) const override;
	MaterialType materialType() const override { return Image; }
	WRayTraceColor* color(ColorType eType, WRayTraceRay* pRay) override;
	W3DReal indexOfRefraction() override { return 1; }
	W3DReal specularExponent() override { return 1; }
	W3DReal glossiness() override { return 0; }
	W3DReal cloudiness() override { return 0; }
	bool isSame(WRayTraceMaterial* pThat) override;
	WRayTraceMaterial* copy() override;

	void setTextureImage(WImage* pImage, bool bDeleteImage);
	WImage* textureImage() { return m_pTextureImage; }
};




#define MAX_OBJECTS_PER_BOUNDING_BOX 5

/// A class used for making ray-tracing faster
class WRayTraceBoundingBoxBase
{
public:
	W3DVector m_min;
	W3DVector m_max;

public:
	WRayTraceBoundingBoxBase() {}
	virtual ~WRayTraceBoundingBoxBase() {}

	static WRayTraceBoundingBoxBase* makeBoundingBoxTree(WRayTraceScene* pScene);
	virtual bool isLeaf() = 0;
	virtual WRayTraceObject* closestIntersection(W3DVector* pRayOrigin, W3DVector* pDirectionVector, W3DReal* pOutDistance) = 0;

protected:
	static WRayTraceBoundingBoxBase* BuildTree(std::vector<WRayTraceObject*>& objects);
	bool DoesRayHitBox(W3DVector* pRayOrigin, W3DVector* pDirectionVector);
};


/// A class used for making ray-tracing faster
class WRayTraceBoundingBoxInterior : public WRayTraceBoundingBoxBase
{
protected:
	WRayTraceBoundingBoxBase* m_pLesser;
	WRayTraceBoundingBoxBase* m_pGreater;

public:
	WRayTraceBoundingBoxInterior(WRayTraceBoundingBoxBase* pLesser, WRayTraceBoundingBoxBase* pGreater)
	: WRayTraceBoundingBoxBase()
	{
		m_pLesser = pLesser;
		m_pGreater = pGreater;
		m_min.m_vals[0] = std::min(pLesser->m_min.m_vals[0], pGreater->m_min.m_vals[0]);
		m_min.m_vals[1] = std::min(pLesser->m_min.m_vals[1], pGreater->m_min.m_vals[1]);
		m_min.m_vals[2] = std::min(pLesser->m_min.m_vals[2], pGreater->m_min.m_vals[2]);
		m_max.m_vals[0] = std::max(pLesser->m_max.m_vals[0], pGreater->m_max.m_vals[0]);
		m_max.m_vals[1] = std::max(pLesser->m_max.m_vals[1], pGreater->m_max.m_vals[1]);
		m_max.m_vals[2] = std::max(pLesser->m_max.m_vals[2], pGreater->m_max.m_vals[2]);
	}

	~WRayTraceBoundingBoxInterior() override
	{
		delete(m_pLesser);
		delete(m_pGreater);
	}

	bool isLeaf() override { return false; };
	WRayTraceObject* closestIntersection(W3DVector* pRayOrigin, W3DVector* pDirectionVector, W3DReal* pOutDistance) override;
};


/// A class used for making ray-tracing faster
class WRayTraceBoundingBoxLeaf : public WRayTraceBoundingBoxBase
{
protected:
	int m_nObjectCount;
	WRayTraceObject** m_pObjects;

public:
	WRayTraceBoundingBoxLeaf(std::vector<WRayTraceObject*>& objects);
	~WRayTraceBoundingBoxLeaf() override;

	bool isLeaf() override { return true; }
	WRayTraceObject* closestIntersection(W3DVector* pRayOrigin, W3DVector* pDirectionVector, W3DReal* pOutDistance) override;
};



/// Represents a triangle mesh in a ray-tracing scene
class WRayTraceTriMesh
{
protected:
	WRayTraceMaterial* m_pMaterial;
	std::vector<W3DVector> m_points;
	std::vector<size_t> m_triangles;
	W3DVector* m_pNormals;
	W3DReal* m_pTextureCoords;
	bool m_bCulling;

public:
	WRayTraceTriMesh(WRayTraceMaterial* pMaterial);
	WRayTraceTriMesh(WDomNode* pNode, WRayTraceScene* pScene);
	~WRayTraceTriMesh();

	WDomNode* serialize(WDom* pDoc, const WRayTraceScene* pScene) const;

	static WRayTraceTriMesh* makeCylinder(WRayTraceMaterial* pMaterial, W3DVector* pCenter1, W3DVector* pCenter2, W3DReal radius, size_t nSides, bool bEndCaps);

	/// The vertices must go around the surface. Don't cut across corners.
	static WRayTraceTriMesh* makeQuadSurface(WRayTraceMaterial* pMaterial, W3DVector* p1, W3DVector* p2, W3DVector* p3, W3DVector* p4);

	static WRayTraceTriMesh* makeSingleTriangle(WRayTraceMaterial* pMaterial, W3DVector* p1, W3DVector* p2, W3DVector* p3);

	W3DReal rayDistanceToTriangle(size_t nTriangle, W3DVector* pRayOrigin, W3DVector* pRayDirection);
	void normalVector(WRayTraceRay* pRay, size_t nIndex);
	bool isCulled() { return m_bCulling || m_pNormals; }
	void activateCulling() { m_bCulling = true; }
	void addPoint(const W3DVector& point);
	void addTriangle(size_t v1, size_t v2, size_t v3);
	void setNormal(size_t nIndex, W3DVector* pNormal);
	void setTextureCoord(size_t nIndex, W3DReal x, W3DReal y);
	void center(W3DVector* pOutPoint, size_t nIndex);
	size_t pointCount() { return m_points.size(); }
	size_t triangleCount() { return m_triangles.size() / 3; }
	void triangle(size_t index, size_t* v1, size_t* v2, size_t* v3);
	WRayTraceMaterial* material() { return m_pMaterial; }
	void adjustBoundingBox(size_t nIndex, W3DVector* pMin, W3DVector* pMax);
	W3DVector* vertex(size_t nIndex, size_t nVertex);

	/// Automatically compute phong normals at the vertices to make the object appear smooth
	void computePhongNormals();

	/// Makes every triangle face the same way as its neighbors, so that two
	/// triangles sharing a side go around it in opposite directions (which
	/// back-face culling and computePhongNormals rely on). Each connected
	/// piece keeps the orientation of its first triangle, and triangles that
	/// disagree are flipped (two of their vertices swapped). Returns false if
	/// some piece can't be oriented consistently, as with a Moebius strip, in
	/// which case its triangles are left as the walk found them. If pFlipped
	/// is given, it receives the number of triangles flipped. Call
	/// computePhongNormals afterwards if the mesh uses them.
	bool orientTriangles(size_t* pFlipped = nullptr);

protected:
	/// Sets the facing (+1 same, -1 flipped) of triangle i's unvisited
	/// neighbors from its own and queues them; false if a visited neighbor
	/// disagrees. Part of orientTriangles.
	bool computeTriangleNeighborSides(std::deque<size_t>& queue, WNodeHashTable& edges, std::vector<int>& faces, size_t i);

	bool isPointWithinPlanarPolygon(W3DVector* pPoint, W3DVector** ppVertices, size_t nVertices);
};




/// An object in a ray-tracing scene
class WRayTraceObject
{
public:
	enum ObjectType
	{
		Sphere,
		Triangle,
	};

	WRayTraceObject()
	{
	}

	virtual ~WRayTraceObject()
	{
	}

	static WRayTraceObject* deserialize(WDomNode* pNode, WRayTraceScene* pScene);
	virtual WDomNode* serialize(WDom* pDoc, const WRayTraceScene* pScene) const = 0;
	virtual ObjectType type() const = 0;
	virtual WRayTraceMaterial* material() = 0;
	virtual W3DReal rayDistance(W3DVector* pRayOrigin, W3DVector* pRayDirection) = 0;
	virtual void normalVector(WRayTraceRay* pRay) = 0;
	virtual bool isCulled() = 0;
	virtual void center(W3DVector* pOutPoint) = 0;
	virtual void adjustBoundingBox(W3DVector* pMin, W3DVector* pMax) = 0;
	virtual void drawWireFrame(WCamera* pCamera, WImage* pImage) = 0;
};



/// A sphere in a ray-tracing scene
class WRayTraceSphere : public WRayTraceObject
{
protected:
	WRayTraceMaterial* m_pMaterial;
	W3DVector m_center;
	W3DReal m_radius;

public:
	WRayTraceSphere(WRayTraceMaterial* pMaterial, W3DReal x, W3DReal y, W3DReal z, W3DReal rad)
	: WRayTraceObject(), m_pMaterial(pMaterial), m_center(x, y, z), m_radius(rad)
	{
	}

	WRayTraceSphere(WDomNode* pNode, WRayTraceScene* pScene);

	~WRayTraceSphere() override
	{
	}

	WDomNode* serialize(WDom* pDoc, const WRayTraceScene* pScene) const override;
	ObjectType type() const override { return Sphere; }
	WRayTraceMaterial* material() override { return m_pMaterial; }
	W3DReal rayDistance(W3DVector* pRayOrigin, W3DVector* pRayDirection) override;
	void normalVector(WRayTraceRay* pRay) override;
	bool isCulled() override { return false; }
	void center(W3DVector* pOutPoint) override;
	void adjustBoundingBox(W3DVector* pMin, W3DVector* pMax) override;
	void drawWireFrame(WCamera* pCamera, WImage* pImage) override;
	W3DVector* center() { return &m_center; }
	W3DReal radius() { return m_radius; }
};





/// A single triangle in a ray-tracing scene
class WRayTraceTriangle : public WRayTraceObject
{
protected:
	WRayTraceTriMesh* m_pMesh;
	size_t m_nIndex;

public:
	WRayTraceTriangle(WRayTraceTriMesh* pMesh, size_t nIndex)
	: WRayTraceObject(), m_pMesh(pMesh), m_nIndex(nIndex)
	{
	}

	WRayTraceTriangle(WDomNode* pNode, WRayTraceScene* pScene);

	~WRayTraceTriangle() override
	{
	}

	WDomNode* serialize(WDom* pDoc, const WRayTraceScene* pScene) const override;
	ObjectType type() const override { return Triangle; }
	W3DReal rayDistance(W3DVector* pRayOrigin, W3DVector* pRayDirection) override;
	void normalVector(WRayTraceRay* pRay) override;
	bool isCulled() override { return m_pMesh->isCulled(); }
	WRayTraceMaterial* material() override { return m_pMesh->material(); }
	void center(W3DVector* pOutPoint) override;
	void adjustBoundingBox(W3DVector* pMin, W3DVector* pMax) override;
	void drawWireFrame(WCamera* pCamera, WImage* pImage) override;
	W3DVector* vertex(int nVertex);
};



class WTriMeshBuilder
{
protected:
	WRayTraceMaterial* m_pMaterial;
	std::vector<W3DVector> m_points;
	std::vector<size_t> m_indexes;
	W3DReal m_lineWidth;

public:
	WTriMeshBuilder(WRayTraceMaterial* pMaterial);
	~WTriMeshBuilder();

	/// Add a triangle to the mesh
	void add(const W3DVector& a, const W3DVector& b, const W3DVector& c);

	/// Return the mesh (and clear the internal queues)
	WRayTraceTriMesh* mesh();

protected:
	/// Adds a point to the internal set if it is not already there.
	/// Returns its index.
	size_t addPoint(const W3DVector& v);
};




class W3dLetterMaker
{
protected:
	WTriMeshBuilder m_builder;
	W3DVector m_pos;
	W3DMatrix m_basis;
	W3DReal m_lineWidth;
	size_t m_circleSegments;
	double m_spaceWidth;

public:
	W3dLetterMaker(WRayTraceMaterial* pMaterial);
	~W3dLetterMaker();

	WRayTraceTriMesh* makeLetter(char c);
	WRayTraceTriMesh* specialChar(const char* szName);
	void writeString(WRayTraceScene* pScene, const char* szString);
	void setLineWidth(W3DReal lw) { m_lineWidth = lw; }
	void move(W3DReal dx, W3DReal dy = 0.0, W3DReal dz = 0.0);
	void move(W3DVector& vec);
	void scale(W3DReal width, W3DReal height, W3DReal depth);
	void rotate(W3DReal yaw, W3DReal pitch, W3DReal roll);
	double letterWidth(char c);
	double measureWidth(const char* szPhrase);
	W3DVector& pos() { return m_pos; }

protected:
	void addQuad(W3DReal x1, W3DReal y1, W3DReal x2, W3DReal y2, W3DReal x3, W3DReal y3, W3DReal x4, W3DReal y4);
	void set(W3DVector* pBack, W3DVector* pFront, W3DReal x, W3DReal y);

	/// Adds a vertical or slanted line
	void vbar(W3DReal x1, W3DReal y1, W3DReal x2, W3DReal y2, W3DReal width = 1.0);

	/// Adds a horizontal line
	void hbar(W3DReal x, W3DReal y, W3DReal w, W3DReal height = 1.0);

	/// Adds a curve. x and y specify the position where 0 radians does (or would) occur.
	/// w is the width of the full circly. hh is the height of half the circle.
	/// from and to specifies the arc segment in radians divided by PI.
	void curve(W3DReal x, W3DReal y, W3DReal w, W3DReal hh, double fromRads, double toRads);
};

} // namespace WClasses

#endif // __GRAYTRACE_H__
