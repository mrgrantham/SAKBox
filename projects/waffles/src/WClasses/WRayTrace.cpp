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

#include "WRayTrace.h"
#include "WError.h"
#include "WHashTable.h"
#include "WDom.h"
#include <algorithm>
#include <functional>
#include <deque>
#include "WImage.h"
#include "WMath.h"
#include <cmath>

namespace WClasses
{

using std::vector;

WDomNode* WRayTraceColor::serialize(WDom* pDoc) const
{
	WDomNode* pNode = pDoc->newObj();
	pNode->add(pDoc, "a", a);
	pNode->add(pDoc, "r", r);
	pNode->add(pDoc, "g", g);
	pNode->add(pDoc, "b", b);
	return pNode;
}

void WRayTraceColor::deserialize(WDomNode* pNode)
{
	a = (W3DReal)pNode->getDouble("a");
	r = (W3DReal)pNode->getDouble("r");
	g = (W3DReal)pNode->getDouble("g");
	b = (W3DReal)pNode->getDouble("b");
}

WRayTraceColor::WRayTraceColor(unsigned int c)
{
	a = ((W3DReal)gAlpha(c)) / 255;
	r = ((W3DReal)gRed(c)) / 255;
	g = ((W3DReal)gGreen(c)) / 255;
	b = ((W3DReal)gBlue(c)) / 255;
}

void WRayTraceColor::set(unsigned int c)
{
	a = ((W3DReal)gAlpha(c)) / 255;
	r = ((W3DReal)gRed(c)) / 255;
	g = ((W3DReal)gGreen(c)) / 255;
	b = ((W3DReal)gBlue(c)) / 255;
}

unsigned int WRayTraceColor::color()
{
	return gARGB(std::min(255, (int)(a * 255)), std::min(255, (int)(r * 255)), std::min(255, (int)(g * 255)), std::min(255, (int)(b * 255)));
}

void WRayTraceColor::makeSliderColor(float f, WRayTraceColor* pDiffuseColor)
{
	a = 1.0f;
	if(f < 0.5)
	{
		f += f;
		r = f * pDiffuseColor->r;
		g = f * pDiffuseColor->g;
		b = f * pDiffuseColor->b;
	}
	else
	{
		f += (f - 0.5f);
		r = (1.0f - f) * pDiffuseColor->r + f;
		g = (1.0f - f) * pDiffuseColor->g + f;
		b = (1.0f - f) * pDiffuseColor->b + f;
	}
}

// -----------------------------------------------------------------------------

WRayTraceCamera::WRayTraceCamera(WDomNode* pNode)
: WCamera(pNode)
{
	m_focalDistance = pNode->getDouble("focal");
	m_lensDiameter = pNode->getDouble("lens");
	m_maxDepth = (int)pNode->getInt("depth");
}

// virtual
WDomNode* WRayTraceCamera::serialize(WDom* pDoc) const
{
	WDomNode* pNode = WCamera::serialize(pDoc);
	pNode->add(pDoc, "focal", m_focalDistance);
	pNode->add(pDoc, "lens", m_lensDiameter);
	pNode->add(pDoc, "depth", (long long)m_maxDepth);
	return pNode;
}

// -----------------------------------------------------------------------------

#define MIN_RAY_DISTANCE ((W3DReal).001)

// Represents a ray for ray tracing
class WRayTraceRay
{
public:
	W3DVector m_collisionPoint;
	W3DVector m_normalVector;
	W3DVector m_reflectionVector;
	WRayTraceColor m_color;
	W3DReal m_indexOfRefraction;
	bool m_bHitTexture;
	W3DReal m_textureX;
	W3DReal m_textureY;
	WRand* m_pRand;

	WRayTraceRay(WRand* m_pRand);
	WRayTraceRay(WRayTraceRay* pThat);
	~WRayTraceRay();

	void Cast(WRayTraceScene* pScene, W3DVector* pRayOrigin, W3DVector* pDirectionVector, int nMaxDepth);
	void Trace(WRayTraceScene* pScene, W3DVector* pRayOrigin, W3DVector* pDirectionVector, int nMaxDepth, bool bEmissive);
	void SetTextureCoords(W3DReal x, W3DReal y);
	void GetTextureCoords(W3DReal* x, W3DReal* y);
	bool DidRayHitTexture() { return m_bHitTexture; }
	static void JitterRay(W3DVector* pVector, W3DReal jitter, WRand* pRand);

protected:
	bool ComputeTransmissionVector(W3DVector* pDirectionVector, W3DVector* pTransmissionVector, W3DReal oldIndexOfRefraction, W3DReal newIndexOfRefraction);
	void ComputeRandomDiffuseVector(W3DVector* pOutVector);
};


WRayTraceRay::WRayTraceRay(WRand* pRand)
{
	m_indexOfRefraction = 1;
	m_pRand = pRand;
}

WRayTraceRay::WRayTraceRay(WRayTraceRay* pThat)
{
	m_indexOfRefraction = pThat->m_indexOfRefraction;
	m_pRand = pThat->m_pRand;
}

WRayTraceRay::~WRayTraceRay()
{
}

bool WRayTraceRay::ComputeTransmissionVector(W3DVector* pDirectionVector, W3DVector* pTransmissionVector, W3DReal oldIndexOfRefraction, W3DReal newIndexOfRefraction)
{
	double ratio = (double)oldIndexOfRefraction / newIndexOfRefraction;
	double comp = (double)pDirectionVector->dotProduct(m_normalVector);
	double tmp = (double)1 - (ratio * ratio * ((double)1 - (comp * comp)));
	if(tmp < 0)
		return false;
	pTransmissionVector->copy(m_normalVector);
	pTransmissionVector->multiply((W3DReal)(-ratio * comp - sqrt(tmp)));
	W3DVector x(*pDirectionVector);
	x.multiply((W3DReal)ratio);
	pTransmissionVector->add(x);
	pTransmissionVector->normalize();
	return true;
}

void WRayTraceRay::ComputeRandomDiffuseVector(W3DVector* pOutVector)
{
	W3DVector u;
	W3DVector v;
	if(std::abs(m_normalVector.m_vals[0]) < .5) // if N is not co-linear with (1, 0, 0)
	{
		v.set(1, 0, 0);
		u.crossProduct(m_normalVector, v);
	}
	else
	{
		v.set(0, 1, 0);
		u.crossProduct(m_normalVector, v);
	}
	u.normalize();
	v.crossProduct(u, m_normalVector);
	double a, b, c;
	while(true)
	{
		a = m_pRand->uniform() * 2 - 1;
		b = m_pRand->uniform() * 2 - 1;
		if((a * a) + (b * b) < 1)
			break;
	}
	c = sqrt(1.0 - (a * a + b * b));
	u.multiply((W3DReal)a);
	v.multiply((W3DReal)b);
	pOutVector->copy(m_normalVector);
	pOutVector->multiply((W3DReal)c);
	pOutVector->add(u);
	pOutVector->add(v);
}

void WRayTraceRay::JitterRay(W3DVector* pVector, W3DReal jitter, WRand* pRand)
{
	pVector->m_vals[0] += (jitter * (W3DReal)(pRand->uniform() * 2 - 1));
	pVector->m_vals[1] += (jitter * (W3DReal)(pRand->uniform() * 2 - 1));
	pVector->m_vals[2] += (jitter * (W3DReal)(pRand->uniform() * 2 - 1));
	pVector->normalize();
}

void WRayTraceRay::Cast(WRayTraceScene* pScene, W3DVector* pRayOrigin, W3DVector* pDirectionVector, int nMaxDepth)
{
	// Find the object
	W3DReal distance;
	WAssert(pScene->boundingBoxTree()); // You must to call RenderBegin first?
	WRayTraceObject* pClosestObject = pScene->boundingBoxTree()->closestIntersection(pRayOrigin, pDirectionVector, &distance);
	if(!pClosestObject)
	{
		m_color.copy(pScene->backgroundColor());
		return;
	}

	// Compute the collision point
	m_collisionPoint.copy(*pDirectionVector);
	m_collisionPoint.multiply(distance);
	m_collisionPoint.add(*pRayOrigin);

	// Compute the normal
	m_bHitTexture = false;
	pClosestObject->normalVector(this);
	if(!pClosestObject->isCulled())
	{
		if(pDirectionVector->dotProduct(m_normalVector) > 0)
			m_normalVector.multiply(-1);
	}

	// Compute the reflection vector
	m_reflectionVector.reflectionVector(*pDirectionVector, m_normalVector);

	// Compute the color
	WRayTraceMaterial* pMaterial = pClosestObject->material();
	pMaterial->computeColor(pScene, this, true, true);

	// Cast child rays
	if(nMaxDepth > 0)
	{
		// Reflection
		WRayTraceColor* pReflectedColor = pMaterial->color(WRayTraceMaterial::Reflective, this);
		if(!pReflectedColor->isBlack())
		{
			W3DReal jitter = pMaterial->glossiness();
			if(jitter > 0)
				JitterRay(&m_reflectionVector, jitter, m_pRand);
			WRayTraceRay reflectionRay(this);
			reflectionRay.Cast(pScene, &m_collisionPoint, &m_reflectionVector, nMaxDepth - 1);
			reflectionRay.m_color.multiply(pReflectedColor);
			m_color.add(&reflectionRay.m_color);
		}

		// Transmission
		WRayTraceColor* pTransmissionColor = pMaterial->color(WRayTraceMaterial::Transmissive, this);
		if(!pTransmissionColor->isBlack())
		{
			// Compute transmission ray
			W3DReal newIndexOfRefraction;
			if(m_indexOfRefraction > .9999 && m_indexOfRefraction < 1.0001)
				newIndexOfRefraction = pMaterial->indexOfRefraction();
			else
				newIndexOfRefraction = 1;
			W3DVector transmissionVector;
			if(!ComputeTransmissionVector(pDirectionVector, &transmissionVector, m_indexOfRefraction, newIndexOfRefraction))
			{
				// total internal reflection occurs
				transmissionVector.copy(m_reflectionVector);
				newIndexOfRefraction = m_indexOfRefraction;
			}

			// Cast transmission ray
			W3DReal jitter = pMaterial->cloudiness();
			if(jitter > 0)
				JitterRay(&transmissionVector, jitter, m_pRand);
			WRayTraceRay transmissionRay(this);
			transmissionRay.m_indexOfRefraction = newIndexOfRefraction;
			transmissionRay.Cast(pScene, &m_collisionPoint, &transmissionVector, nMaxDepth - 1);
			transmissionRay.m_color.multiply(pTransmissionColor);
			m_color.add(&transmissionRay.m_color);
		}
	}
}

#define PATH_TRACER_PROB_REFLECTIVE 0.5
#define PATH_TRACER_PROB_TRANSMISSIVE 0.3

void WRayTraceRay::Trace(WRayTraceScene* pScene, W3DVector* pRayOrigin, W3DVector* pDirectionVector, int nMaxDepth, bool bEmissive)
{
	// Find the object
	W3DReal distance;
	WRayTraceObject* pClosestObject = pScene->boundingBoxTree()->closestIntersection(pRayOrigin, pDirectionVector, &distance);
	if(!pClosestObject)
	{
		m_color.copy(pScene->backgroundColor());
		return;
	}

	// Compute the collision point
	m_collisionPoint.copy(*pDirectionVector);
	m_collisionPoint.multiply(distance);
	m_collisionPoint.add(*pRayOrigin);

	// Compute the normal
	m_bHitTexture = false;
	pClosestObject->normalVector(this);
	if(!pClosestObject->isCulled())
	{
		if(pDirectionVector->dotProduct(m_normalVector) > 0)
			m_normalVector.multiply(-1);
	}

	// Compute the reflection vector
	m_reflectionVector.reflectionVector(*pDirectionVector, m_normalVector);

	// Compute the color
	WRayTraceMaterial* pMaterial = pClosestObject->material();
	pMaterial->computeColor(pScene, this, false, false);
	if(bEmissive)
		m_color.add(pMaterial->color(WRayTraceMaterial::Emissive, this));

	// Cast child rays
	if(nMaxDepth > 0)
	{
		WRayTraceColor* pReflectedColor = pMaterial->color(WRayTraceMaterial::Reflective, this);
		WRayTraceColor* pTransmissionColor = pMaterial->color(WRayTraceMaterial::Transmissive, this);
		WRayTraceColor* pDiffuseColor = pMaterial->color(WRayTraceMaterial::Diffuse, this);
		W3DReal lumReflective = pReflectedColor->r * (W3DReal)0.299 + pReflectedColor->g * (W3DReal)0.587 + pReflectedColor->b * (W3DReal)0.114;
		W3DReal lumTransmissive = pTransmissionColor->r * (W3DReal)0.299 + pTransmissionColor->g * (W3DReal)0.587 + pTransmissionColor->b * (W3DReal)0.114;
		W3DReal lumDiffuse = pDiffuseColor->r * (W3DReal)0.299 + pDiffuseColor->g * (W3DReal)0.587 + pDiffuseColor->b * (W3DReal)0.114;
		W3DReal lumSum = lumReflective + lumTransmissive + lumDiffuse;
		if(lumSum > .0001)
		{
			lumReflective /= lumSum;
			lumTransmissive /= lumSum;
			lumDiffuse /= lumSum;
			double d = m_pRand->uniform();
			if(d < lumReflective)
			{
				// Reflective path
				W3DReal jitter = pMaterial->glossiness();
				if(jitter > 0)
					JitterRay(&m_reflectionVector, jitter, m_pRand);
				WRayTraceRay reflectionRay(this);
				reflectionRay.Trace(pScene, &m_collisionPoint, &m_reflectionVector, nMaxDepth - 1, true);
				reflectionRay.m_color.multiply(pReflectedColor);
				reflectionRay.m_color.multiply((W3DReal)1.0 / lumReflective); // Compensate for less than one probability of selecting this path
				m_color.add(&reflectionRay.m_color);
			}
			else if(d < lumReflective + lumTransmissive)
			{
				// Compute transmission ray
				W3DReal newIndexOfRefraction;
				if(m_indexOfRefraction > .9999 && m_indexOfRefraction < 1.0001)
					newIndexOfRefraction = pMaterial->indexOfRefraction();
				else
					newIndexOfRefraction = 1;
				W3DVector transmissionVector;
				if(!ComputeTransmissionVector(pDirectionVector, &transmissionVector, m_indexOfRefraction, newIndexOfRefraction))
				{
					// total internal reflection occurs
					transmissionVector.copy(m_reflectionVector);
					newIndexOfRefraction = m_indexOfRefraction;
				}

				// Cast transmission ray
				W3DReal jitter = pMaterial->cloudiness();
				if(jitter > 0)
					JitterRay(&transmissionVector, jitter, m_pRand);
				WRayTraceRay transmissionRay(this);
				transmissionRay.m_indexOfRefraction = newIndexOfRefraction;
				transmissionRay.Trace(pScene, &m_collisionPoint, &transmissionVector, nMaxDepth - 1, true);
				transmissionRay.m_color.multiply(pTransmissionColor);
				transmissionRay.m_color.multiply((W3DReal)1.0 / lumTransmissive); // Compensate for less than one probability of selecting this path
				m_color.add(&transmissionRay.m_color);
			}
			else
			{
				// Diffuse path
				W3DVector diffuseVector;
				ComputeRandomDiffuseVector(&diffuseVector);
				WRayTraceRay diffuseRay(this);
				diffuseRay.Trace(pScene, &m_collisionPoint, &diffuseVector, nMaxDepth - 1, false);
				diffuseRay.m_color.multiply(pDiffuseColor);
				diffuseRay.m_color.multiply((W3DReal)1.0 / lumDiffuse); // Compensate for less than one probability of selecting this path
				m_color.add(&diffuseRay.m_color);
			}
		}
	}
}

// I'm not sure it really makes sense to store the texture coordinates
// in the ray, but somehow this data needs to be passed from the tri-mesh
// to the material, and the ray is a convenient carrier
void WRayTraceRay::SetTextureCoords(W3DReal x, W3DReal y)
{
	m_bHitTexture = true;
	m_textureX = x;
	m_textureY = y;
}

void WRayTraceRay::GetTextureCoords(W3DReal* x, W3DReal* y)
{
	*x = m_textureX;
	*y = m_textureY;
}

// -----------------------------------------------------------------------------

WRayTraceScene::WRayTraceScene(WRand* pRand)
: m_backgroundColor(1, (W3DReal).6, (W3DReal).7, (W3DReal).5),
  m_ambientLight(1, (W3DReal).3, (W3DReal).3, (W3DReal).3), m_pRand(pRand)
{
	m_pCamera = new WRayTraceCamera(320, 240);
	m_pBoundingBoxTree = nullptr;
	m_pImage = nullptr;
	m_pDistanceMap = nullptr;
	m_nY = -1;
	m_toneMappingConstant = .5;
	m_eMode = FAST_RAY_TRACE;
}

WRayTraceScene::WRayTraceScene(WDomNode* pNode, WRand* pRand)
: m_pRand(pRand)
{
	m_backgroundColor.deserialize(pNode->get("bgcol"));
	m_ambientLight.deserialize(pNode->get("ambient"));
	for(WDomListIterator it1(pNode->get("materials")); it1.current(); it1.advance())
		m_materials.push_back(WRayTraceMaterial::deserialize(it1.current()));
	for(WDomListIterator it2(pNode->get("meshes")); it2.current(); it2.advance())
		m_meshes.push_back(new WRayTraceTriMesh(it2.current(), this));
	for(WDomListIterator it3(pNode->get("objects")); it3.current(); it3.advance())
		m_objects.push_back(WRayTraceObject::deserialize(it3.current(), this));
	;
	for(WDomListIterator it4(pNode->get("lights")); it4.current(); it4.advance())
		m_lights.push_back(WRayTraceLight::deserialize(it4.current(), this));
	m_pCamera = new WRayTraceCamera(pNode->get("camera"));
	m_toneMappingConstant = pNode->getDouble("tone");
	m_eMode = (RenderMode)pNode->getInt("mode");
}

WRayTraceScene::~WRayTraceScene()
{
	flushObjects();
	for(vector<WRayTraceLight*>::iterator it = m_lights.begin(); it != m_lights.end(); it++)
		delete(*it);
	delete(m_pCamera);
	delete(m_pImage);
	delete(m_pDistanceMap);
}

WDomNode* WRayTraceScene::serialize(WDom* pDoc) const
{
	WDomNode* pSceneNode = pDoc->newObj();
	pSceneNode->add(pDoc, "bgcol", m_backgroundColor.serialize(pDoc));
	pSceneNode->add(pDoc, "ambient", m_ambientLight.serialize(pDoc));
	WDomNode* pMaterialsNode = pSceneNode->add(pDoc, "materials", pDoc->newList());
	for(size_t i = 0; i < m_materials.size(); i++)
		pMaterialsNode->add(pDoc, m_materials[i]->serialize(pDoc));
	WDomNode* pMeshesNode = pSceneNode->add(pDoc, "meshes", pDoc->newList());
	for(size_t i = 0; i < m_meshes.size(); i++)
		pMeshesNode->add(pDoc, m_meshes[i]->serialize(pDoc, this));
	WDomNode* pObjectsNode = pSceneNode->add(pDoc, "objects", pDoc->newList());
	for(size_t i = 0; i < m_objects.size(); i++)
		pObjectsNode->add(pDoc, m_objects[i]->serialize(pDoc, this));
	WDomNode* pLightsNode = pSceneNode->add(pDoc, "lights", pDoc->newList());
	for(size_t i = 0; i < m_lights.size(); i++)
		pLightsNode->add(pDoc, m_lights[i]->serialize(pDoc, this));
	pSceneNode->add(pDoc, "camera", m_pCamera->serialize(pDoc));
	pSceneNode->add(pDoc, "tone", m_toneMappingConstant);
	pSceneNode->add(pDoc, "mode", (long long)m_eMode);
	return pSceneNode;
}

void WRayTraceScene::flushObjects()
{
	for(vector<WRayTraceMaterial*>::iterator it = m_materials.begin(); it != m_materials.end(); it++)
		delete(*it);
	m_materials.clear();
	for(vector<WRayTraceTriMesh*>::iterator it = m_meshes.begin(); it != m_meshes.end(); it++)
		delete(*it);
	m_meshes.clear();
	for(vector<WRayTraceObject*>::iterator it = m_objects.begin(); it != m_objects.end(); it++)
		delete(*it);
	m_objects.clear();
	delete(m_pBoundingBoxTree);
	m_pBoundingBoxTree = nullptr;
}

void WRayTraceScene::swapObjects(WRayTraceScene* pOther)
{
	m_materials.swap(pOther->m_materials);
	m_meshes.swap(pOther->m_meshes);
	m_objects.swap(pOther->m_objects);
	std::swap(m_pBoundingBoxTree, pOther->m_pBoundingBoxTree);
}

size_t WRayTraceScene::materialCount()
{
	return m_materials.size();
}

size_t WRayTraceScene::meshCount()
{
	return m_meshes.size();
}

size_t WRayTraceScene::objectCount()
{
	return m_objects.size();
}

size_t WRayTraceScene::lightCount()
{
	return m_lights.size();
}

WRayTraceMaterial* WRayTraceScene::material(size_t n)
{
	if(n >= m_materials.size())
		throw Ex("out of range");
	return m_materials[n];
}

WRayTraceTriMesh* WRayTraceScene::mesh(size_t n)
{
	if(n >= m_meshes.size())
		throw Ex("out of range");
	return m_meshes[n];
}

WRayTraceObject* WRayTraceScene::object(size_t n)
{
	if(n >= m_objects.size())
		throw Ex("out of range");
	return m_objects[n];
}

WRayTraceLight* WRayTraceScene::light(size_t n)
{
	if(n >= m_lights.size())
		throw Ex("out of range");
	return m_lights[n];
}

void WRayTraceScene::addMaterial(WRayTraceMaterial* pMaterial)
{
	m_materials.push_back(pMaterial);
}

template<typename T>
size_t GetIndexOfPointerInVector(const vector<T*>& vec, T* p)
{
	for(typename vector<T*>::const_iterator it = vec.begin(); it != vec.end(); it++)
		if(p == *it)
			return (it - vec.begin());
	throw Ex("The specified value could not be found");
	return -1;
}

size_t WRayTraceScene::materialIndex(WRayTraceMaterial* pMaterial) const
{
	return GetIndexOfPointerInVector(m_materials, pMaterial);
}

size_t WRayTraceScene::meshIndex(WRayTraceTriMesh* pMesh) const
{
	return GetIndexOfPointerInVector(m_meshes, pMesh);
}

size_t WRayTraceScene::objectIndex(WRayTraceObject* pObj) const
{
	return GetIndexOfPointerInVector(m_objects, pObj);
}

void WRayTraceScene::addMesh(WRayTraceTriMesh* pMesh)
{
	m_meshes.push_back(pMesh);
	size_t nCount = pMesh->triangleCount();
	for(size_t i = 0; i < nCount; i++)
		addObject(new WRayTraceTriangle(pMesh, i));
}

void WRayTraceScene::addObject(WRayTraceObject* pObject)
{
	m_objects.push_back(pObject);
}

void WRayTraceScene::addLight(WRayTraceLight* pLight)
{
	m_lights.push_back(pLight);
}

void WRayTraceScene::activateDistanceMap()
{
	if(m_pDistanceMap)
		delete[] m_pDistanceMap;
	m_pDistanceMap = new W3DReal[m_pCamera->imageWidth() * m_pCamera->imageHeight()];
}

void WRayTraceScene::drawWireFrame()
{
	// Allocate an image
	if(!m_pImage)
		m_pImage = new WImage();
	int nWidth = m_pCamera->imageWidth();
	int nHeight = m_pCamera->imageHeight();
	m_pImage->setSize(nWidth, nHeight);
	m_pImage->clear(0xff000000);
	for(vector<WRayTraceObject*>::iterator it = m_objects.begin(); it != m_objects.end(); it++)
		(*it)->drawWireFrame(m_pCamera, m_pImage);
}

void WRayTraceScene::renderBegin()
{
	// Allocate an image
	if(!m_pImage)
		m_pImage = new WImage();
	int nWidth = m_pCamera->imageWidth();
	int nHeight = m_pCamera->imageHeight();
	m_pImage->setSize(nWidth, nHeight);

	// Rebuild the bounding box tree
	delete(m_pBoundingBoxTree);
	m_pBoundingBoxTree = WRayTraceBoundingBoxBase::makeBoundingBoxTree(this);

	// Precompute vectors
	W3DVector v(*m_pCamera->viewUpVector());
	W3DVector u(*m_pCamera->viewSideVector());
	W3DReal halfViewHeight = m_pCamera->halfViewHeight();
	W3DReal halfViewWidth = halfViewHeight * nWidth / nHeight;
	u.multiply(halfViewWidth);
	v.multiply(halfViewHeight);
	m_pixSide.copy(*m_pCamera->lookFromPoint());
	m_pixSide.add(*m_pCamera->lookDirection());
	m_pixSide.subtract(u);
	m_pixSide.subtract(v);
	m_pixDX.copy(u);
	m_pixDX.multiply((W3DReal)2 / nWidth);
	m_pixDY.copy(v);
	m_pixDY.multiply((W3DReal)2 / nHeight);
	m_nY = nHeight - 1;
}

unsigned int WRayTraceScene::renderPixel(WRayTraceRay* pRay, W3DVector* pScreenPoint, W3DReal* pDistance)
{
	W3DVector directionVector(*pScreenPoint);
	directionVector.subtract(*m_pCamera->lookFromPoint());
	directionVector.normalize();
	pRay->Cast(this, m_pCamera->lookFromPoint(), &directionVector, m_pCamera->maxDepth());
	if(pDistance)
		m_pBoundingBoxTree->closestIntersection(m_pCamera->lookFromPoint(), &directionVector, pDistance);
	return pRay->m_color.color();
}

#define SQRT_RAYS_PER_PIXEL 6

unsigned int WRayTraceScene::renderPixelAntiAliassed(WRayTraceRay* pRay, W3DVector* pScreenPoint, W3DReal* pDistance)
{
	W3DVector jitter;
	int x, y;
	WRayTraceColor col;
	W3DReal focalDistance = m_pCamera->focalDistance();
	W3DReal r1, r2;
	for(y = 0; y < SQRT_RAYS_PER_PIXEL; y++)
	{
		for(x = 0; x < SQRT_RAYS_PER_PIXEL; x++)
		{
			W3DVector directionVector(*pScreenPoint);

			// Jitter in X direction
			jitter.copy(m_pixDX);
			jitter.multiply((W3DReal)(((double)x + m_pRand->uniform()) / SQRT_RAYS_PER_PIXEL - .5));
			directionVector.add(jitter);

			// Jitter in Y direction
			jitter.copy(m_pixDY);
			jitter.multiply((W3DReal)(((double)y + m_pRand->uniform()) / SQRT_RAYS_PER_PIXEL - .5));
			directionVector.add(jitter);

			// Cast the ray
			directionVector.subtract(*m_pCamera->lookFromPoint());
			if(focalDistance > 0)
			{
				// Use focus lens -- Start from a random point on the lens and fire at the focal point
				while(true)
				{
					r1 = (W3DReal)(m_pRand->uniform() - .5);
					r2 = (W3DReal)(m_pRand->uniform() - .5);
					if((r1 * r1) + (r2 * r2) <= .25)
						break;
				}
				W3DVector dx(*m_pCamera->viewSideVector());
				dx.multiply(r1 * m_pCamera->lensDiameter());
				W3DVector dy(*m_pCamera->viewUpVector());
				dy.multiply(r2 * m_pCamera->lensDiameter());
				directionVector.multiply(focalDistance);
				directionVector.subtract(dx);
				directionVector.subtract(dy);
				directionVector.normalize();
				W3DVector lensPoint(*m_pCamera->lookFromPoint());
				lensPoint.add(dx);
				lensPoint.add(dy);
				pRay->Cast(this, &lensPoint, &directionVector, m_pCamera->maxDepth());
			}
			else
			{
				// Perfect focus -- just fire straight
				directionVector.normalize();
				pRay->Cast(this, m_pCamera->lookFromPoint(), &directionVector, m_pCamera->maxDepth());
			}

			// Make sure the color doesn't exceed pure white since it will be added with other measurements
			pRay->m_color.clip();
			col.add(&pRay->m_color);
		}
	}
	col.multiply((W3DReal)1 / (SQRT_RAYS_PER_PIXEL * SQRT_RAYS_PER_PIXEL));

	if(pDistance)
	{
		W3DVector directionVector(*pScreenPoint);
		directionVector.subtract(*m_pCamera->lookFromPoint());
		directionVector.normalize();
		m_pBoundingBoxTree->closestIntersection(m_pCamera->lookFromPoint(), &directionVector, pDistance);
	}

	return col.color();
}

unsigned int WRayTraceScene::renderPixelPathTrace(WRayTraceRay* pRay, W3DVector* pScreenPoint)
{
	W3DVector jitter;
	int x, y;
	WRayTraceColor col;
	W3DReal focalDistance = m_pCamera->focalDistance();
	W3DReal r1, r2;
	for(y = 0; y < SQRT_RAYS_PER_PIXEL; y++)
	{
		for(x = 0; x < SQRT_RAYS_PER_PIXEL; x++)
		{
			W3DVector directionVector(*pScreenPoint);

			// Jitter in X direction
			jitter.copy(m_pixDX);
			jitter.multiply((W3DReal)(((double)x + m_pRand->uniform()) / SQRT_RAYS_PER_PIXEL - .5));
			directionVector.add(jitter);

			// Jitter in Y direction
			jitter.copy(m_pixDY);
			jitter.multiply((W3DReal)(((double)y + m_pRand->uniform()) / SQRT_RAYS_PER_PIXEL - .5));
			directionVector.add(jitter);

			// Cast the ray
			directionVector.subtract(*m_pCamera->lookFromPoint());
			if(focalDistance > 0)
			{
				// Use focus lens -- Start from a random point on the lens and fire at the focal point
				while(true)
				{
					r1 = (W3DReal)(m_pRand->uniform() - .5);
					r2 = (W3DReal)(m_pRand->uniform() - .5);
					if((r1 * r1) + (r2 * r2) <= .25)
						break;
				}
				W3DVector dx(*m_pCamera->viewSideVector());
				dx.multiply(r1 * m_pCamera->lensDiameter());
				W3DVector dy(*m_pCamera->viewUpVector());
				dy.multiply(r2 * m_pCamera->lensDiameter());
				directionVector.multiply(focalDistance);
				directionVector.subtract(dx);
				directionVector.subtract(dy);
				directionVector.normalize();
				W3DVector lensPoint(*m_pCamera->lookFromPoint());
				lensPoint.add(dx);
				lensPoint.add(dy);
				pRay->Trace(this, &lensPoint, &directionVector, m_pCamera->maxDepth(), true);
			}
			else
			{
				// Perfect focus -- just fire straight
				directionVector.normalize();
				pRay->Trace(this, m_pCamera->lookFromPoint(), &directionVector, m_pCamera->maxDepth(), true);
			}
			col.add(&pRay->m_color);
		}
	}
	col.multiply((W3DReal)1 / (SQRT_RAYS_PER_PIXEL * SQRT_RAYS_PER_PIXEL));

	// Apply tone mapping constant
	W3DReal luminance = col.r * (W3DReal)0.299 + col.g * (W3DReal)0.587 + col.b * (W3DReal)0.114;
	W3DReal desiredLuminance = (m_toneMappingConstant * luminance) / ((W3DReal)1.0 + m_toneMappingConstant * luminance);
	col.multiply(desiredLuminance / luminance);

	return col.color();
}

bool WRayTraceScene::renderLine()
{
	if(m_nY < 0)
		return false;
	WRayTraceRay ray(m_pRand);
	int x;
	W3DVector screenPoint(m_pixSide);
	int nWidth = m_pImage->width();
	W3DReal distance;
	W3DReal* pDistance = m_pDistanceMap ? &distance : nullptr;
	unsigned int col = 0;
	for(x = 0; x < nWidth; x++)
	{
		//WAssert(x != 100 || m_nY != 143); // break
		switch(m_eMode)
		{
			case FAST_RAY_TRACE:
				col = renderPixel(&ray, &screenPoint, pDistance);
				break;
			case QUALITY_RAY_TRACE:
				col = renderPixelAntiAliassed(&ray, &screenPoint, pDistance);
				break;
			case PATH_TRACE:
				col = renderPixelPathTrace(&ray, &screenPoint);
				break;
			default:
				WAssert(false); // unrecognized case
		}
		m_pImage->setPixel(x, m_nY, col);
		if(pDistance)
			m_pDistanceMap[nWidth * m_nY + x] = distance;
		screenPoint.add(m_pixDX);
	}
	m_pixSide.add(m_pixDY);
	if(--m_nY >= 0)
		return true;
	else
		return false;
}

void WRayTraceScene::render()
{
	renderBegin();
	while(renderLine())
	{
	}
}

unsigned int WRayTraceScene::renderSinglePixel(int x, int y)
{
	// Init the rendering
	renderBegin();

	// Compute the screen point
	W3DVector screenPoint(m_pixSide);
	m_pixDY.multiply((W3DReal)(m_pImage->height() - 1 - y));
	screenPoint.add(m_pixDY);
	m_pixDX.multiply((W3DReal)(m_pImage->width() - 1 - x));
	screenPoint.add(m_pixDX);

	// Cast the ray
	WRayTraceRay ray(m_pRand);
	unsigned int c = renderPixel(&ray, &screenPoint, nullptr);
	return c;
}

// -----------------------------------------------------------------------------

WRayTraceLight::WRayTraceLight(W3DReal r, W3DReal g, W3DReal b)
: m_color(1, r, g, b)
{
}

WRayTraceLight::WRayTraceLight(WDomNode* pNode)
{
	m_color.deserialize(pNode->get("color"));
}

/*virtual*/ WRayTraceLight::~WRayTraceLight()
{
}

WDomNode* WRayTraceLight::baseDomNode(WDom* pDoc) const
{
	WDomNode* pNode = pDoc->newObj();
	pNode->add(pDoc, "color", m_color.serialize(pDoc));
	pNode->add(pDoc, "type", (long long)lightType());
	return pNode;
}

// static
WRayTraceLight* WRayTraceLight::deserialize(WDomNode* pNode, WRayTraceScene* pScene)
{
	switch((LightType)pNode->getInt("type"))
	{
		case Directional:
			return new WRayTraceDirectionalLight(pNode);
		case Point:
			return new WRayTracePointLight(pNode);
		case Area:
			return new WRayTraceAreaLight(pNode, pScene);
	}
	throw Ex("Unrecognized light type");
	return nullptr;
}

// -----------------------------------------------------------------------------

WRayTraceDirectionalLight::WRayTraceDirectionalLight(W3DReal dx, W3DReal dy, W3DReal dz, W3DReal r, W3DReal g, W3DReal b, W3DReal jitter)
: WRayTraceLight(r, g, b), m_direction(dx, dy, dz), m_jitter(jitter)
{
}

WRayTraceDirectionalLight::WRayTraceDirectionalLight(WDomNode* pNode)
: WRayTraceLight(pNode)
{
	m_direction.deserialize(pNode->get("dir"));
	m_jitter = pNode->getDouble("jit");
}

/*virtual*/ WRayTraceDirectionalLight::~WRayTraceDirectionalLight()
{
}

// virtual
WDomNode* WRayTraceDirectionalLight::serialize(WDom* pDoc, const WRayTraceScene* pScene) const
{
	WDomNode* pNode = baseDomNode(pDoc);
	pNode->add(pDoc, "dir", m_direction.serialize(pDoc));
	pNode->add(pDoc, "jit", m_jitter);
	return pNode;
}

/*virtual*/ void WRayTraceDirectionalLight::colorContribution(WRayTraceScene* pScene, WRayTraceRay* pRay, WRayTraceMaterial* pMaterial, bool bSpecular)
{
	W3DVector direction(m_direction);
	if(m_jitter > 0)
		WRayTraceRay::JitterRay(&direction, m_jitter, pScene->rand());

	// Check if the point is in a shadow
	W3DReal distance;
	if(pScene->boundingBoxTree()->closestIntersection(&pRay->m_collisionPoint, &direction, &distance))
		return;

	// Compute diffuse component of the color
	WRayTraceColor diffuse(pMaterial->color(WRayTraceMaterial::Diffuse, pRay));
	diffuse.multiply(std::max((W3DReal)0, direction.dotProduct(pRay->m_normalVector)));

	// Compute specular component of the color
	if(bSpecular)
	{
		W3DReal mag = (W3DReal)pow(std::max((W3DReal)0, pRay->m_reflectionVector.dotProduct(direction)), pMaterial->specularExponent());
		WRayTraceColor specular(pMaterial->color(WRayTraceMaterial::Specular, pRay));
		specular.multiply(mag);
		diffuse.add(&specular);
	}

	// Multiply by light intensity
	diffuse.multiply(&m_color);
	pRay->m_color.add(&diffuse);
}

// -----------------------------------------------------------------------------


WRayTracePointLight::WRayTracePointLight(W3DReal x, W3DReal y, W3DReal z, W3DReal r, W3DReal g, W3DReal b, W3DReal jitter)
: WRayTraceLight(r, g, b), m_position(x, y, z), m_jitter(jitter)
{
}

WRayTracePointLight::WRayTracePointLight(WDomNode* pNode)
: WRayTraceLight(pNode)
{
	m_position.deserialize(pNode->get("pos"));
	m_jitter = pNode->getDouble("jit");
}

/*virtual*/ WRayTracePointLight::~WRayTracePointLight()
{
}

// virtual
WDomNode* WRayTracePointLight::serialize(WDom* pDoc, const WRayTraceScene* pScene) const
{
	WDomNode* pNode = baseDomNode(pDoc);
	pNode->add(pDoc, "pos", m_position.serialize(pDoc));
	pNode->add(pDoc, "jit", m_jitter);
	return pNode;
}

/*virtual*/ void WRayTracePointLight::colorContribution(WRayTraceScene* pScene, WRayTraceRay* pRay, WRayTraceMaterial* pMaterial, bool bSpecular)
{
	W3DVector lightDirection(m_position);
	if(m_jitter > 0)
	{
		// Jitter light position (to create soft shadows)
		lightDirection.m_vals[0] += (W3DReal)(m_jitter * 2 * pScene->rand()->uniform() - m_jitter);
		lightDirection.m_vals[1] += (W3DReal)(m_jitter * 2 * pScene->rand()->uniform() - m_jitter);
		lightDirection.m_vals[2] += (W3DReal)(m_jitter * 2 * pScene->rand()->uniform() - m_jitter);
	}

	// Check if the point is in a shadow
	lightDirection.subtract(pRay->m_collisionPoint);
	double distsqared = lightDirection.squaredMag();
	lightDirection.normalize();
	W3DReal distance;
	if(pScene->boundingBoxTree()->closestIntersection(&pRay->m_collisionPoint, &lightDirection, &distance))
	{
		if((distance * distance) < distsqared)
			return;
	}

	// Compute diffuse component of the color
	WRayTraceColor diffuse(pMaterial->color(WRayTraceMaterial::Diffuse, pRay));
	diffuse.multiply(std::max((W3DReal)0, lightDirection.dotProduct(pRay->m_normalVector)));

	// Compute specular component of the color
	if(bSpecular)
	{
		W3DReal mag = (W3DReal)pow(std::max((W3DReal)0, pRay->m_reflectionVector.dotProduct(lightDirection)), pMaterial->specularExponent());
		WRayTraceColor specular(pMaterial->color(WRayTraceMaterial::Specular, pRay));
		specular.multiply(mag);
		diffuse.add(&specular);
	}

	// Multiply by light intensity
	diffuse.multiply(&m_color);
	diffuse.multiply((W3DReal)(1.0 / distsqared));
	pRay->m_color.add(&diffuse);
}

// -----------------------------------------------------------------------------

WRayTraceAreaLight::WRayTraceAreaLight(WRayTraceObject* pObject, W3DReal r, W3DReal g, W3DReal b)
: WRayTraceLight(r, g, b), m_pObject(pObject)
{
}

WRayTraceAreaLight::WRayTraceAreaLight(WDomNode* pNode, WRayTraceScene* pScene)
: WRayTraceLight(pNode)
{
	m_pObject = pScene->object((size_t)pNode->getInt("obj"));
}

/*virtual*/ WRayTraceAreaLight::~WRayTraceAreaLight()
{
}

// virtual
WDomNode* WRayTraceAreaLight::serialize(WDom* pDoc, const WRayTraceScene* pScene) const
{
	WDomNode* pNode = baseDomNode(pDoc);
	pNode->add(pDoc, "obj", pScene->objectIndex(m_pObject));
	return pNode;
}

/*virtual*/ void WRayTraceAreaLight::colorContribution(WRayTraceScene* pScene, WRayTraceRay* pRay, WRayTraceMaterial* pMaterial, bool bSpecular)
{
	W3DVector lightDirection;
	if(m_pObject->type() == WRayTraceObject::Triangle)
	{
		// Pick a random point on the triangle
		WRayTraceTriangle* pTri = (WRayTraceTriangle*)m_pObject;
		W3DVector u(*pTri->vertex(1));
		u.subtract(*pTri->vertex(0));
		W3DVector v(*pTri->vertex(2));
		v.subtract(*pTri->vertex(0));
		double a = pScene->rand()->uniform() * 2 - 1;
		double b = pScene->rand()->uniform() * 2 - 1;
		if(a + b > 1)
		{
			a = 1 - a;
			b = 1 - b;
		}
		u.multiply((W3DReal)a);
		v.multiply((W3DReal)b);
		lightDirection.copy(*pTri->vertex(0));
		lightDirection.add(u);
		lightDirection.add(v);
	}
	else if(m_pObject->type() == WRayTraceObject::Sphere)
	{
		// Pick a random point on a disc facing the point of collision
		WRayTraceSphere* pSphere = (WRayTraceSphere*)m_pObject;
		W3DVector u, v;
		W3DVector N(*pSphere->center());
		N.subtract(pRay->m_collisionPoint);
		N.normalize();
		if(std::abs(N.m_vals[0]) < 0.5)
		{
			v.set(1, 0, 0);
			u.crossProduct(N, v);
		}
		else
		{
			v.set(0, 1, 0);
			u.crossProduct(N, v);
		}
		u.normalize();
		v.crossProduct(u, N);
		double a, b;
		while(true)
		{
			a = pScene->rand()->uniform() * 2 - 1;
			b = pScene->rand()->uniform() * 2 - 1;
			if((a * a) + (b * b) < 1)
				break;
		}
		u.multiply((W3DReal)a);
		v.multiply((W3DReal)b);
		lightDirection.copy(*pSphere->center());
		lightDirection.add(u);
		lightDirection.add(v);
	}
	else
	{
		WAssert(false); // Unrecognized object type
	}

	// Check if the point is in a shadow
	lightDirection.subtract(pRay->m_collisionPoint);
	double distsqared = lightDirection.squaredMag();
	lightDirection.normalize();
	W3DReal distance;
	WRayTraceObject* pObj = pScene->boundingBoxTree()->closestIntersection(&pRay->m_collisionPoint, &lightDirection, &distance);
	if(pObj != m_pObject && pObj && distance * distance < distsqared)
		return;

	WRayTraceColor diffuse(&m_color);
	diffuse.multiply(pMaterial->color(WRayTraceMaterial::Diffuse, pRay));
	if(m_pObject->type() == WRayTraceObject::Triangle)
	{
		WRayTraceTriangle* pTri = (WRayTraceTriangle*)m_pObject;
		W3DVector u(*pTri->vertex(1));
		u.subtract(*pTri->vertex(0));
		W3DVector v(*pTri->vertex(2));
		v.subtract(*pTri->vertex(0));
		W3DVector t;
		t.crossProduct(u, v);
		W3DReal triangleArea = (W3DReal)sqrt(t.squaredMag()) / 2;
		W3DVector triangleNormal;
		triangleNormal.triangleNormal(*pTri->vertex(0), *pTri->vertex(1), *pTri->vertex(2));
		diffuse.multiply(std::abs(pRay->m_normalVector.dotProduct(lightDirection) * triangleNormal.dotProduct(lightDirection) * triangleArea / (W3DReal)(M_PI * distsqared)));
	}
	else
	{
		WRayTraceSphere* pSphere = (WRayTraceSphere*)m_pObject;
		diffuse.multiply(pRay->m_normalVector.dotProduct(lightDirection) * pSphere->radius() * pSphere->radius() / (W3DReal)distsqared);
	}

	pRay->m_color.add(&diffuse);
}

// -----------------------------------------------------------------------------

WRayTraceMaterial::WRayTraceMaterial()
{
}

// virtual
WRayTraceMaterial::~WRayTraceMaterial()
{
}

// static
WRayTraceMaterial* WRayTraceMaterial::deserialize(WDomNode* pNode)
{
	switch((MaterialType)pNode->getInt("type"))
	{
		case Physical:
			return new WRayTracePhysicalMaterial(pNode);
		case Image:
			return new WRayTraceImageTexture(pNode);
	}
	return nullptr;
}

void WRayTraceMaterial::computeColor(WRayTraceScene* pScene, WRayTraceRay* pRay, bool bAmbient, bool bSpecular)
{
	if(bAmbient)
	{
		pRay->m_color.copy(pScene->ambientLight());
		pRay->m_color.multiply(color(Ambient, pRay));
	}
	else
		pRay->m_color.set(0, 0, 0, 0);

	// Real lights
	for(size_t n = 0; n < pScene->lightCount(); n++)
		pScene->light(n)->colorContribution(pScene, pRay, this, bSpecular);
}




WRayTracePhysicalMaterial::WRayTracePhysicalMaterial()
{
	setColor(Diffuse, (W3DReal).5, (W3DReal).5, (W3DReal).5);
	setColor(Specular, 1, 1, 1);
	setColor(Reflective, 0, 0, 0);
	setColor(Transmissive, 0, 0, 0);
	setColor(Ambient, (W3DReal).1, (W3DReal).1, (W3DReal).1);
	setColor(Emissive, 0, 0, 0);
	m_indexOfRefraction = 1;
	m_specularExponent = 1;
	m_glossiness = 0;
	m_cloudiness = 0;
}

WRayTracePhysicalMaterial::WRayTracePhysicalMaterial(WDomNode* pNode)
{
	WDomListIterator it(pNode->get("colors"));
	for(int i = 0; i < Color_Type_Count && it.current(); i++)
	{
		m_colors[i].deserialize(it.current());
		it.advance();
	}
	m_indexOfRefraction = pNode->getDouble("iof");
	m_specularExponent = pNode->getDouble("se");
	m_glossiness = pNode->getDouble("glossiness");
	m_cloudiness = pNode->getDouble("cloudiness");
}

// virtual
WRayTracePhysicalMaterial::~WRayTracePhysicalMaterial()
{
}

// virtual
WDomNode* WRayTracePhysicalMaterial::serialize(WDom* pDoc) const
{
	WDomNode* pNode = pDoc->newObj();
	pNode->add(pDoc, "type", (long long)materialType());
	WDomNode* pColors = pNode->add(pDoc, "colors", pDoc->newList());
	for(int i = 0; i < Color_Type_Count; i++)
		pColors->add(pDoc, m_colors[i].serialize(pDoc));
	pNode->add(pDoc, "iof", m_indexOfRefraction);
	pNode->add(pDoc, "se", m_specularExponent);
	pNode->add(pDoc, "glossiness", m_glossiness);
	pNode->add(pDoc, "cloudiness", m_cloudiness);
	return pNode;
}

// virtual
bool WRayTracePhysicalMaterial::isSame(WRayTraceMaterial* pOther)
{
	WRayTracePhysicalMaterial* pThat = (WRayTracePhysicalMaterial*)pOther;
	for(int i = 0; i < Color_Type_Count; i++)
	{
		if(std::abs(pThat->m_colors[i].r - m_colors[i].r) > 1e-6)
			return false;
		if(std::abs(pThat->m_colors[i].g - m_colors[i].g) > 1e-6)
			return false;
		if(std::abs(pThat->m_colors[i].b - m_colors[i].b) > 1e-6)
			return false;
	}
	if(std::abs(pThat->m_indexOfRefraction - m_indexOfRefraction) > 1e-6)
		return false;
	if(std::abs(pThat->m_specularExponent - m_specularExponent) > 1e-6)
		return false;
	if(std::abs(pThat->m_glossiness - m_glossiness) > 1e-6)
		return false;
	if(std::abs(pThat->m_cloudiness - m_cloudiness) > 1e-6)
		return false;
	return true;
}

// virtual
WRayTraceMaterial* WRayTracePhysicalMaterial::copy()
{
	WRayTracePhysicalMaterial* pCopy = new WRayTracePhysicalMaterial();
	for(int i = 0; i < Color_Type_Count; i++)
		pCopy->m_colors[i] = m_colors[i];
	pCopy->m_indexOfRefraction = m_indexOfRefraction;
	pCopy->m_specularExponent = m_specularExponent;
	pCopy->m_glossiness = m_glossiness;
	pCopy->m_cloudiness = m_cloudiness;
	return pCopy;
}

// virtual
WRayTraceColor* WRayTracePhysicalMaterial::color(ColorType eType, WRayTraceRay* pRay)
{
	return &m_colors[eType];
}

void WRayTracePhysicalMaterial::setColor(ColorType eType, WRayTraceColor* pCol)
{
	m_colors[eType].copy(pCol);
}

void WRayTracePhysicalMaterial::setColor(ColorType eType, W3DReal r, W3DReal g, W3DReal b)
{
	if(eType == Emissive)
		m_colors[eType].set(1, r, g, b);
	else
		m_colors[eType].set(1, std::max((W3DReal)0, std::min((W3DReal)1, r)), std::max((W3DReal)0, std::min((W3DReal)1, g)), std::max((W3DReal)0, std::min((W3DReal)1, b)));
}






WRayTraceImageTexture::WRayTraceImageTexture()
{
	m_pTextureImage = nullptr;
	m_bDeleteTextureImage = false;
}

WRayTraceImageTexture::WRayTraceImageTexture(WDomNode* pNode)
{
	throw Ex("Sorry, deserializing images not yet supported");
}

// virtual
WRayTraceImageTexture::~WRayTraceImageTexture()
{
	if(m_bDeleteTextureImage)
		delete(m_pTextureImage);
}

// virtual
WDomNode* WRayTraceImageTexture::serialize(WDom* pDoc) const
{
	WDomNode* pNode = pDoc->newObj();
	pNode->add(pDoc, "type", (long long)materialType());
	throw Ex("Sorry, serializing images not yet supported");
	return pNode;
}

// virtual
bool WRayTraceImageTexture::isSame(WRayTraceMaterial* pThat)
{
	throw Ex("Sorry, not implemented yet");
	return false;
}

// virtual
WRayTraceMaterial* WRayTraceImageTexture::copy()
{
	throw Ex("Sorry, not implemented yet");
	return nullptr;
}

void WRayTraceImageTexture::setTextureImage(WImage* pImage, bool bDeleteImage)
{
	if(m_bDeleteTextureImage)
		delete(m_pTextureImage);
	m_pTextureImage = pImage;
	m_bDeleteTextureImage = bDeleteImage;
}

WRayTraceColor* WRayTraceImageTexture::color(ColorType eType, WRayTraceRay* pRay)
{
	if(eType == Diffuse)
	{
		if(m_pTextureImage)
		{
			WAssert(pRay->DidRayHitTexture()); // ray doesn't know about this
			W3DReal x, y;
			pRay->GetTextureCoords(&x, &y);
			y = m_pTextureImage->height() - y;
			m_col.set(m_pTextureImage->interpolatePixel((float)x, (float)y));
		}
		else
			m_col.set(1, 0, 1, 1); // if you see cyan, you forgot to set the texture image
		return &m_col;
	}
	else
	{
		m_col.set(1, 0, 0, 0);
		return &m_col;
	}
}


// -----------------------------------------------------------------------------

// Sorts the objects by their centres along dimension dim. SAKBox: a stable
// sort, so objects whose centres tie keep the order they were added in.
// With std::sort the tie order was up to the standard library, which shapes
// the tree, and so the pixels where objects meet at equal distances: GCC's
// library rendered such a scene differently from macOS's.
static void WRayTrace_sortByCenter(vector<WRayTraceObject*>& objects, int dim)
{
	std::ranges::stable_sort(objects, std::ranges::less{}, [dim](WRayTraceObject* pObject)
	{
		W3DVector center;
		pObject->center(&center);
		return center.m_vals[dim];
	});
}

//static
WRayTraceBoundingBoxBase* WRayTraceBoundingBoxBase::BuildTree(vector<WRayTraceObject*>& objects)
{
	// Make a leaf node if we can
	if(objects.size() <= MAX_OBJECTS_PER_BOUNDING_BOX)
		return new WRayTraceBoundingBoxLeaf(objects);

	// Find the biggest dimension
	double dBiggestRange = 0;
	int nBiggestDim = -1;
	double dRange;
	int i;
	WRayTraceObject* pFirst;
	WRayTraceObject* pLast;
	W3DVector vFirst, vLast;
	for(i = 0; i < 3; i++)
	{
		WRayTrace_sortByCenter(objects, i);
		pFirst = objects[0];
		pLast = objects[objects.size() - 1];
		pFirst->center(&vFirst);
		pLast->center(&vLast);
		dRange = vLast.m_vals[i] - vFirst.m_vals[i];
		if(dRange > dBiggestRange)
		{
			nBiggestDim = i;
			dBiggestRange = dRange;
		}
	}

	// Split into two arrays
	if(nBiggestDim != 2)
	{
		WRayTrace_sortByCenter(objects, nBiggestDim);
	}
	int nSize = (int)objects.size();
	int nLesserSize = nSize / 2;
	vector<WRayTraceObject*> other;
	other.reserve(nSize - nLesserSize);
	for(i = nSize - 1; i >= nLesserSize; i--)
	{
		other.push_back(objects[objects.size() - 1]);
		objects.pop_back();
	}

	// Make an interior node
	WRayTraceBoundingBoxBase* pLesser = BuildTree(objects);
	WRayTraceBoundingBoxBase* pGreater = BuildTree(other);
	return new WRayTraceBoundingBoxInterior(pLesser, pGreater);
}

//static
WRayTraceBoundingBoxBase* WRayTraceBoundingBoxBase::makeBoundingBoxTree(WRayTraceScene* pScene)
{
	// Make a copy of the object list
	vector<WRayTraceObject*> objects;
	objects.reserve(pScene->objectCount());
	for(size_t i = 0; i < pScene->objectCount(); i++)
		objects.push_back(pScene->object(i));

	// Build the tree
	return BuildTree(objects);
}

bool WRayTraceBoundingBoxBase::DoesRayHitBox(W3DVector* pRayOrigin, W3DVector* pDirectionVector)
{
	W3DVector point;
	if(pDirectionVector->m_vals[0] != 0)
	{
		point.copy(*pDirectionVector);
		point.multiply((m_min.m_vals[0] - pRayOrigin->m_vals[0]) / pDirectionVector->m_vals[0]);
		point.add(*pRayOrigin);
		if(point.m_vals[1] >= m_min.m_vals[1] && point.m_vals[2] >= m_min.m_vals[2] && point.m_vals[1] <= m_max.m_vals[1] && point.m_vals[2] <= m_max.m_vals[2])
			return true;
		point.copy(*pDirectionVector);
		point.multiply((m_max.m_vals[0] - pRayOrigin->m_vals[0]) / pDirectionVector->m_vals[0]);
		point.add(*pRayOrigin);
		if(point.m_vals[1] >= m_min.m_vals[1] && point.m_vals[2] >= m_min.m_vals[2] && point.m_vals[1] <= m_max.m_vals[1] && point.m_vals[2] <= m_max.m_vals[2])
			return true;
	}
	if(pDirectionVector->m_vals[1] != 0)
	{
		point.copy(*pDirectionVector);
		point.multiply((m_min.m_vals[1] - pRayOrigin->m_vals[1]) / pDirectionVector->m_vals[1]);
		point.add(*pRayOrigin);
		if(point.m_vals[0] >= m_min.m_vals[0] && point.m_vals[2] >= m_min.m_vals[2] && point.m_vals[0] <= m_max.m_vals[0] && point.m_vals[2] <= m_max.m_vals[2])
			return true;
		point.copy(*pDirectionVector);
		point.multiply((m_max.m_vals[1] - pRayOrigin->m_vals[1]) / pDirectionVector->m_vals[1]);
		point.add(*pRayOrigin);
		if(point.m_vals[0] >= m_min.m_vals[0] && point.m_vals[2] >= m_min.m_vals[2] && point.m_vals[0] <= m_max.m_vals[0] && point.m_vals[2] <= m_max.m_vals[2])
			return true;
	}
	if(pDirectionVector->m_vals[2] != 0)
	{
		point.copy(*pDirectionVector);
		point.multiply((m_min.m_vals[2] - pRayOrigin->m_vals[2]) / pDirectionVector->m_vals[2]);
		point.add(*pRayOrigin);
		if(point.m_vals[0] >= m_min.m_vals[0] && point.m_vals[1] >= m_min.m_vals[1] && point.m_vals[0] <= m_max.m_vals[0] && point.m_vals[1] <= m_max.m_vals[1])
			return true;
		point.copy(*pDirectionVector);
		point.multiply((m_max.m_vals[2] - pRayOrigin->m_vals[2]) / pDirectionVector->m_vals[2]);
		point.add(*pRayOrigin);
		if(point.m_vals[0] >= m_min.m_vals[0] && point.m_vals[1] >= m_min.m_vals[1] && point.m_vals[0] <= m_max.m_vals[0] && point.m_vals[1] <= m_max.m_vals[1])
			return true;
	}
	return false;
}

// -----------------------------------------------------------------------------

// virtual
WRayTraceObject* WRayTraceBoundingBoxInterior::closestIntersection(W3DVector* pRayOrigin, W3DVector* pDirectionVector, W3DReal* pOutDistance)
{
	if(!DoesRayHitBox(pRayOrigin, pDirectionVector))
		return nullptr;
	W3DReal lesserDist = 0;
	W3DReal greaterDist = 0;
	WRayTraceObject* pLesser = m_pLesser->closestIntersection(pRayOrigin, pDirectionVector, &lesserDist);
	WRayTraceObject* pGreater = m_pGreater->closestIntersection(pRayOrigin, pDirectionVector, &greaterDist);
	if(!pGreater)
	{
		*pOutDistance = lesserDist;
		return pLesser;
	}
	if(!pLesser)
	{
		*pOutDistance = greaterDist;
		return pGreater;
	}
	if(lesserDist < greaterDist)
	{
		*pOutDistance = lesserDist;
		return pLesser;
	}
	else
	{
		*pOutDistance = greaterDist;
		return pGreater;
	}
}

// -----------------------------------------------------------------------------

WRayTraceBoundingBoxLeaf::WRayTraceBoundingBoxLeaf(vector<WRayTraceObject*>& objects)
{
	m_nObjectCount = (int)objects.size();
	m_pObjects = new WRayTraceObject*[m_nObjectCount];
	m_min.set((W3DReal)1e30, (W3DReal)1e30, (W3DReal)1e30);
	m_max.set((W3DReal)-1e30, (W3DReal)-1e30, (W3DReal)-1e30);
	int i;
	for(i = 0; i < m_nObjectCount; i++)
	{
		m_pObjects[i] = objects[i];
		m_pObjects[i]->adjustBoundingBox(&m_min, &m_max);
	}
}

//virtual
WRayTraceBoundingBoxLeaf::~WRayTraceBoundingBoxLeaf()
{
	delete[] m_pObjects;
}

// virtual
WRayTraceObject* WRayTraceBoundingBoxLeaf::closestIntersection(W3DVector* pRayOrigin, W3DVector* pDirectionVector, W3DReal* pOutDistance)
{
	if(!DoesRayHitBox(pRayOrigin, pDirectionVector))
		return nullptr;

	// Find the closest intersection
	W3DReal distance;
	W3DReal closestDistance = (W3DReal)1e30;
	WRayTraceObject* pClosestObject = nullptr;
	int n;
	for(n = 0; n < m_nObjectCount; n++)
	{
		distance = m_pObjects[n]->rayDistance(pRayOrigin, pDirectionVector);
		if(distance < closestDistance && distance > MIN_RAY_DISTANCE)
		{
			closestDistance = distance;
			pClosestObject = m_pObjects[n];
		}
	}
	*pOutDistance = closestDistance;
	return pClosestObject;
}

// -----------------------------------------------------------------------------

WRayTraceTriMesh::WRayTraceTriMesh(WRayTraceMaterial* pMaterial)
{
	m_pMaterial = pMaterial;
	m_pNormals = nullptr;
	m_pTextureCoords = nullptr;
	m_bCulling = false;
}

WRayTraceTriMesh::WRayTraceTriMesh(WDomNode* pNode, WRayTraceScene* pScene)
{
	m_pMaterial = pScene->material((size_t)pNode->getInt("material"));
	WDomNode* pPointsNode = pNode->get("points");
	WDomNode* pTrianglesNode = pNode->get("triangles");
	WDomNode* pNormalsNode = pNode->getIfExists("normals");
	WDomNode* pCoordsNode = pNode->getIfExists("coords");
	WDomListIterator it1(pPointsNode);
	for([[maybe_unused]] size_t i = 0; it1.current(); i++)
	{
		W3DVector tmp;
		tmp.deserialize(it1.current());
		m_points.push_back(tmp);
		it1.advance();
	}
	WDomListIterator it2(pTrianglesNode);
	if(it2.remaining() % 3 != 0)
		throw Ex("triangle points are not a multiple of 3");
	for([[maybe_unused]] size_t i = 0; it2.current(); i++)
	{
		m_triangles.push_back((size_t)it2.currentInt());
		it2.advance();
	}
	if(pNormalsNode)
	{
		WDomListIterator it3(pNormalsNode);
		if(it3.remaining() != m_points.size())
			throw Ex("The number of normals must match the number of vertexes");
		m_pNormals = new W3DVector[m_points.size()];
		for(size_t i = 0; it3.current(); i++)
		{
			m_pNormals[i].deserialize(it3.current());
			it3.advance();
		}
	}
	else
		m_pNormals = nullptr;
	if(pCoordsNode)
	{
		WDomListIterator it4(pCoordsNode);
		if(it4.remaining() != 2 * m_points.size())
			throw Ex("The number of texture coords must be double the number of vertexes");
		m_pTextureCoords = new W3DReal[2 * m_points.size()];
		for(size_t i = 0; it4.current(); i++)
		{
			m_pTextureCoords[i] = it4.currentDouble();
			it4.advance();
		}
	}
	else
		m_pTextureCoords = nullptr;
	m_bCulling = pNode->getBool("culling");
}

/*virtual*/ WRayTraceTriMesh::~WRayTraceTriMesh()
{
	delete[] m_pNormals;
	delete[] m_pTextureCoords;
}

WDomNode* WRayTraceTriMesh::serialize(WDom* pDoc, const WRayTraceScene* pScene) const
{
	WDomNode* pNode = pDoc->newObj();
	pNode->add(pDoc, "material", pScene->materialIndex(m_pMaterial));
	WDomNode* pPointsNode = pNode->add(pDoc, "points", pDoc->newList());
	for(size_t i = 0; i < m_points.size(); i++)
		pPointsNode->add(pDoc, m_points[i].serialize(pDoc));
	WDomNode* pTrianglesNode = pNode->add(pDoc, "triangles", pDoc->newList());
	for(size_t i = 0; i < m_triangles.size(); i++)
		pTrianglesNode->add(pDoc, m_triangles[i]);
	if(m_pNormals)
	{
		WDomNode* pNormalsNode = pNode->add(pDoc, "normals", pDoc->newList());
		for(size_t i = 0; i < m_points.size(); i++)
			pNormalsNode->add(pDoc, m_pNormals[i].serialize(pDoc));
	}
	if(m_pTextureCoords)
	{
		WDomNode* pCoordsNode = pNode->add(pDoc, "coords", pDoc->newList());
		for(size_t i = 0; i < 2 * m_points.size(); i++)
			pCoordsNode->add(pDoc, m_pTextureCoords[i]);
	}
	pNode->add(pDoc, "culling", m_bCulling);
	return pNode;
}

void WRayTraceTriMesh::addPoint(const W3DVector& point)
{
	m_points.push_back(point);
}

void WRayTraceTriMesh::addTriangle(size_t v1, size_t v2, size_t v3)
{
	m_triangles.push_back(v1);
	m_triangles.push_back(v2);
	m_triangles.push_back(v3);
}

void WRayTraceTriMesh::setNormal(size_t nIndex, W3DVector* pNormal)
{
	if(!m_pNormals)
		m_pNormals = new W3DVector[m_points.size()];
	WAssert(nIndex < m_points.size()); // out of range
	m_pNormals[nIndex] = *pNormal;
}

// SAKBox: EdgeRecord and computeTriangleNeighborSides were dead upstream
// since at least 2011 (the function was commented out, and EdgeRecord's
// Hash/Equals didn't match HashTableNode's hash/equals, so it couldn't be
// instantiated). They are revived here, ported to the current mesh and hash
// table, behind WRayTraceTriMesh::orientTriangles.

/// One edge of a triangle mesh, keyed by its two vertices (in either order),
/// with the triangles that share it.
class EdgeRecord : public HashTableNode
{
protected:
	size_t m_v1;
	size_t m_v2;
	vector<size_t> m_triangles;

public:
	EdgeRecord(size_t v1, size_t v2) : HashTableNode()
	{
		setEdge(v1, v2);
	}

	vector<size_t>& triangles() { return m_triangles; }

	void setEdge(size_t v1, size_t v2)
	{
		m_v1 = std::min(v1, v2);
		m_v2 = std::max(v1, v2);
	}

	size_t hash(size_t nBucketCount) override
	{
		return (m_v1 * 7387 + m_v2) % nBucketCount;
	}

	bool equals(HashTableNode* pThat) override
	{
		EdgeRecord* pOther = (EdgeRecord*)pThat;
		return m_v1 == pOther->m_v1 && m_v2 == pOther->m_v2;
	}

	void addTriangle(size_t i)
	{
		m_triangles.push_back(i);
	}
};

void WRayTraceTriMesh::triangle(size_t index, size_t* v1, size_t* v2, size_t* v3)
{
	// SAKBox: this was declared but never defined
	*v1 = m_triangles[3 * index];
	*v2 = m_triangles[3 * index + 1];
	*v3 = m_triangles[3 * index + 2];
}

bool WRayTraceTriMesh::computeTriangleNeighborSides(std::deque<size_t>& queue, WNodeHashTable& edges, std::vector<int>& faces, size_t i)
{
	WAssert(faces[i] != 0); // Expected to already know the facing direction of triangle i
	bool consistent = true;
	EdgeRecord tmp(0, 0);
	for(size_t j = 0; j < 3; j++) // j = which side of the main triangle
	{
		const size_t a = m_triangles[3 * i + j];
		const size_t b = m_triangles[3 * i + (j + 1) % 3];
		tmp.setEdge(a, b);
		EdgeRecord* pEdge = (EdgeRecord*)edges.get(&tmp);
		WAssert(pEdge); // No record of this edge
		for(size_t n : pEdge->triangles()) // n = a neighbor across that side
		{
			if(n == i)
				continue; // It's the same triangle

			// Find the neighbor's copy of the side's first vertex
			size_t l;
			for(l = 0; l < 3; l++)
			{
				if(m_triangles[3 * n + l] == a)
					break;
			}
			if(l == 3)
				continue; // Not really a neighbor (can't happen with a valid edge table)

			// Triangles facing the same way go around a shared side in
			// opposite directions: if the neighbor also goes a -> b, it faces
			// the other way
			const bool sameDirection = m_triangles[3 * n + (l + 1) % 3] == b;
			const int expected = sameDirection ? -faces[i] : faces[i];
			if(faces[n] == 0)
			{
				faces[n] = expected;
				queue.push_back(n); // Visit its neighbors in turn
			}
			else if(faces[n] != expected)
				consistent = false; // Reached by two paths that disagree
		}
	}
	return consistent;
}

bool WRayTraceTriMesh::orientTriangles(size_t* pFlipped)
{
	// Index every edge by the triangles that share it
	const size_t count = triangleCount();
	WNodeHashTable edges(true, std::max<size_t>(16, 3 * count));
	for(size_t i = 0; i < count; i++)
	{
		for(size_t j = 0; j < 3; j++)
		{
			EdgeRecord tmp(m_triangles[3 * i + j], m_triangles[3 * i + (j + 1) % 3]);
			EdgeRecord* pEdge = (EdgeRecord*)edges.get(&tmp);
			if(!pEdge)
			{
				pEdge = new EdgeRecord(tmp);
				edges.add(pEdge);
			}
			pEdge->addTriangle(i);
		}
	}

	// Walk each connected piece from its first triangle, which keeps its
	// orientation (+1); each neighbor gets +1 (same way) or -1 (flip it)
	std::vector<int> faces(count, 0);
	std::deque<size_t> queue;
	bool consistent = true;
	for(size_t start = 0; start < count; start++)
	{
		if(faces[start] != 0)
			continue;
		faces[start] = 1;
		queue.push_back(start);
		while(!queue.empty())
		{
			const size_t i = queue.front();
			queue.pop_front();
			if(!computeTriangleNeighborSides(queue, edges, faces, i))
				consistent = false;
		}
	}

	// Flip the triangles that face the wrong way by swapping two vertices
	size_t flipped = 0;
	for(size_t i = 0; i < count; i++)
	{
		if(faces[i] < 0)
		{
			std::swap(m_triangles[3 * i + 1], m_triangles[3 * i + 2]);
			flipped++;
		}
	}
	if(pFlipped)
		*pFlipped = flipped;
	return consistent;
}
class WTriangleIndexArray
{
public:
	vector<size_t> m_triangles;
};

void WRayTraceTriMesh::computePhongNormals()
{
	// Make lists of all the triangles that touch each vertex
	vector<WTriangleIndexArray> arrVertexTriangles;
	arrVertexTriangles.resize(m_points.size());
	for(size_t i = 0; i < m_triangles.size() / 3; i++)
	{
		for(size_t j = 0; j < 3; j++)
			arrVertexTriangles[m_triangles[3 * i + j]].m_triangles.push_back(i);
	}

	// Make the vertex normals
	size_t nTri;
	delete[] m_pNormals;
	m_pNormals = new W3DVector[m_points.size()];
	W3DVector triNorm;
	for(size_t i = 0; i < m_points.size(); i++)
	{
		m_pNormals[i].set(0, 0, 0);
		for(size_t j = 0; j < arrVertexTriangles[i].m_triangles.size(); j++)
		{
			nTri = arrVertexTriangles[i].m_triangles[j];
			triNorm.triangleNormal(
			  m_points[m_triangles[3 * nTri]],
			  m_points[m_triangles[3 * nTri + 1]],
			  m_points[m_triangles[3 * nTri + 2]]);
			// todo: scale each component vector by the area of the triangle that contributes it?
			m_pNormals[i].add(triNorm);
		}
		m_pNormals[i].normalize();
	}
}

void WRayTraceTriMesh::setTextureCoord(size_t nIndex, W3DReal x, W3DReal y)
{
	if(!m_pTextureCoords)
		m_pTextureCoords = new W3DReal[2 * m_points.size()];
	WAssert(nIndex < m_points.size()); // out of range
	nIndex *= 2;
	m_pTextureCoords[nIndex] = x;
	m_pTextureCoords[nIndex + 1] = y;
}

bool WRayTraceTriMesh::isPointWithinPlanarPolygon(W3DVector* pPoint, W3DVector** ppVertices, size_t nVertices)
{
	// Find the two dimensions with the most significant component (which
	// are the dimensions with the smallest component in the normal vector)
	WAssert(nVertices >= 3); // at least three points are needed to define a planar polygon
	W3DVector plane;
	plane.triangleNormal(*ppVertices[0], *ppVertices[1], *ppVertices[2]);
	plane.m_vals[0] = std::abs(plane.m_vals[0]);
	plane.m_vals[1] = std::abs(plane.m_vals[1]);
	plane.m_vals[2] = std::abs(plane.m_vals[2]);
	int d1, d2;
	if(plane.m_vals[0] >= plane.m_vals[1] && plane.m_vals[0] >= plane.m_vals[2])
	{
		d1 = 1;
		d2 = 2;
	}
	else if(plane.m_vals[1] >= plane.m_vals[0] && plane.m_vals[1] >= plane.m_vals[2])
	{
		d1 = 0;
		d2 = 2;
	}
	else
	{
		d1 = 0;
		d2 = 1;
	}

	// Count the number of times a ray shot out from the point crosses
	// a side of the polygon
	int numCrossings = 0;
	int signHolder, nextSignHolder;
	if(ppVertices[0]->m_vals[d2] - pPoint->m_vals[d2] < 0)
		signHolder = -1;
	else
		signHolder = 1;
	size_t nNextVertex;
	W3DReal u0, u1, v0, v1;
	for(size_t nVertex = 0; nVertex < nVertices; nVertex++)
	{
		nNextVertex = (nVertex + 1) % nVertices;
		v1 = ppVertices[nNextVertex]->m_vals[d2] - pPoint->m_vals[d2];
		if(v1 < 0)
			nextSignHolder = -1;
		else
			nextSignHolder = 1;
		if(signHolder != nextSignHolder)
		{
			u0 = ppVertices[nVertex]->m_vals[d1] - pPoint->m_vals[d1];
			u1 = ppVertices[nNextVertex]->m_vals[d1] - pPoint->m_vals[d1];
			v0 = ppVertices[nVertex]->m_vals[d2] - pPoint->m_vals[d2];
			if(u0 - v0 * (u1 - u0) / (v1 - v0) > 0)
				numCrossings++;
		}
		signHolder = nextSignHolder;
	}
	if(numCrossings & 1)
		return true;
	else
		return false;
}

W3DReal WRayTraceTriMesh::rayDistanceToTriangle(size_t nTriangle, W3DVector* pRayOrigin, W3DVector* pRayDirection)
{
	// Compute the plane equasion Ax + By + Cz + D = 0
	size_t t1 = m_triangles[3 * nTriangle];
	size_t t2 = m_triangles[3 * nTriangle + 1];
	size_t t3 = m_triangles[3 * nTriangle + 2];
	W3DVector plane; // The plane normal is the vector (A, B, C)
	W3DReal d;
	plane.planeEquation(m_points[t1], m_points[t2], m_points[t3], &d);

	// Compute distance and point of intersection
	W3DReal tmp = plane.dotProduct(*pRayDirection);
	if(tmp >= 0)
	{
		if(tmp == 0)
			return 0; // the ray is paralell to the plane
		if(m_bCulling)
			return 0; // the ray hits the back side of the plane
		else
		{
			// Reverse the plane normal
			plane.multiply(-1);
			d = -d;
			tmp = -tmp;
		}
	}
	W3DReal distance = -(plane.dotProduct(*pRayOrigin) + d) / tmp;
	if(distance <= 0)
		return 0; // the intersection point is behind the ray origin
	W3DVector point(*pRayDirection);
	point.multiply(distance);
	point.add(*pRayOrigin);

	// Determine if the intersection point is within the triangle
	W3DVector* pVertices[3];
	pVertices[0] = &m_points[t1];
	pVertices[1] = &m_points[t2];
	pVertices[2] = &m_points[t3];
	if(!isPointWithinPlanarPolygon(&point, pVertices, 3))
		return 0; // the ray misses the triangle
	return distance;
}

void WRayTraceTriMesh::normalVector(WRayTraceRay* pRay, size_t nIndex)
{
	size_t t0 = m_triangles[3 * nIndex];
	size_t t1 = m_triangles[3 * nIndex + 1];
	size_t t2 = m_triangles[3 * nIndex + 2];
	pRay->m_normalVector.triangleNormal(m_points[t0], m_points[t1], m_points[t2]);
	if(!m_pNormals && !m_pTextureCoords)
		return;

	// Find the two most significant dimensions
	int i1, i2;
	W3DReal xx, yy, zz;
	xx = pRay->m_normalVector.m_vals[0] * pRay->m_normalVector.m_vals[0];
	yy = pRay->m_normalVector.m_vals[1] * pRay->m_normalVector.m_vals[1];
	zz = pRay->m_normalVector.m_vals[2] * pRay->m_normalVector.m_vals[2];
	if(xx >= yy && xx >= zz)
	{
		i1 = 1;
		i2 = 2;
	}
	else if(yy >= zz)
	{
		i1 = 0;
		i2 = 2;
	}
	else
	{
		i1 = 0;
		i2 = 1;
	}

	// Find the weights alpha, beta, and gamma
	W3DReal u1 = m_points[t1].m_vals[i1] - m_points[t0].m_vals[i1];
	W3DReal u2 = m_points[t2].m_vals[i1] - m_points[t0].m_vals[i1];
	W3DReal v1 = m_points[t1].m_vals[i2] - m_points[t0].m_vals[i2];
	W3DReal v2 = m_points[t2].m_vals[i2] - m_points[t0].m_vals[i2];
	W3DReal u0 = pRay->m_collisionPoint.m_vals[i1] - m_points[t0].m_vals[i1];
	W3DReal v0 = pRay->m_collisionPoint.m_vals[i2] - m_points[t0].m_vals[i2];
	W3DReal beta = (u0 * v2 - u2 * v0) / (u1 * v2 - v1 * u2);
	W3DReal gamma = (v0 * u1 - u0 * v1) / (u1 * v2 - v1 * u2);
	W3DReal alpha = (W3DReal)1.0 - (gamma + beta);
	/*
	WAssert(ABS(alpha * m_pPoints[t0].m_vals[0] + beta * m_points[t1].m_vals[0] + gamma * m_points[t2].m_vals[0] - pRay->m_collisionPoint.m_vals[0]) < .001, "value is wrong");
	WAssert(ABS(alpha * m_pPoints[t0].m_vals[1] + beta * m_points[t1].m_vals[1] + gamma * m_points[t2].m_vals[1] - pRay->m_collisionPoint.m_vals[1]) < .001, "value is wrong");
	WAssert(ABS(alpha * m_pPoints[t0].m_vals[2] + beta * m_points[t1].m_vals[2] + gamma * m_points[t2].m_vals[2] - pRay->m_collisionPoint.m_vals[2]) < .001, "value is wrong");
*/
	// todo: If beta and gamma > 0 and beta + gamma <= 1,
	// then the point of intersection is inside the triangle.
	// This could be used as a more efficient inside-outside test for triangles.

	// Compute the Phong normal
	if(m_pNormals)
	{
		W3DVector tmpNormal(m_pNormals[t0]);
		tmpNormal.multiply(alpha);
		pRay->m_normalVector.copy(tmpNormal);
		tmpNormal.copy(m_pNormals[t1]);
		tmpNormal.multiply(beta);
		pRay->m_normalVector.add(tmpNormal);
		tmpNormal.copy(m_pNormals[t2]);
		tmpNormal.multiply(gamma);
		pRay->m_normalVector.add(tmpNormal);
		pRay->m_normalVector.normalize();
	}

	// Compute texture coordinate
	if(m_pTextureCoords)
	{
		W3DReal x = alpha * m_pTextureCoords[2 * t0] + beta * m_pTextureCoords[2 * t1] + gamma * m_pTextureCoords[2 * t2];
		W3DReal y = alpha * m_pTextureCoords[2 * t0 + 1] + beta * m_pTextureCoords[2 * t1 + 1] + gamma * m_pTextureCoords[2 * t2 + 1];
		pRay->SetTextureCoords(x, y);
	}
}

void WRayTraceTriMesh::center(W3DVector* pOutPoint, size_t nIndex)
{
	size_t t0 = m_triangles[3 * nIndex];
	size_t t1 = m_triangles[3 * nIndex + 1];
	size_t t2 = m_triangles[3 * nIndex + 2];
	pOutPoint->copy(m_points[t0]);
	pOutPoint->add(m_points[t1]);
	pOutPoint->add(m_points[t2]);
	pOutPoint->multiply((W3DReal)1.0 / (W3DReal)3.0);
}

W3DVector* WRayTraceTriMesh::vertex(size_t nIndex, size_t nVertex)
{
	return &m_points[m_triangles[3 * nIndex + nVertex]];
}

void WRayTraceTriMesh::adjustBoundingBox(size_t nIndex, W3DVector* pMin, W3DVector* pMax)
{
	size_t t0 = m_triangles[3 * nIndex];
	size_t t1 = m_triangles[3 * nIndex + 1];
	size_t t2 = m_triangles[3 * nIndex + 2];

	// Vertex 0
	pMin->m_vals[0] = std::min(pMin->m_vals[0], m_points[t0].m_vals[0]);
	pMin->m_vals[1] = std::min(pMin->m_vals[1], m_points[t0].m_vals[1]);
	pMin->m_vals[2] = std::min(pMin->m_vals[2], m_points[t0].m_vals[2]);
	pMax->m_vals[0] = std::max(pMax->m_vals[0], m_points[t0].m_vals[0]);
	pMax->m_vals[1] = std::max(pMax->m_vals[1], m_points[t0].m_vals[1]);
	pMax->m_vals[2] = std::max(pMax->m_vals[2], m_points[t0].m_vals[2]);

	// Vertex 1
	pMin->m_vals[0] = std::min(pMin->m_vals[0], m_points[t1].m_vals[0]);
	pMin->m_vals[1] = std::min(pMin->m_vals[1], m_points[t1].m_vals[1]);
	pMin->m_vals[2] = std::min(pMin->m_vals[2], m_points[t1].m_vals[2]);
	pMax->m_vals[0] = std::max(pMax->m_vals[0], m_points[t1].m_vals[0]);
	pMax->m_vals[1] = std::max(pMax->m_vals[1], m_points[t1].m_vals[1]);
	pMax->m_vals[2] = std::max(pMax->m_vals[2], m_points[t1].m_vals[2]);

	// Vertex 2
	pMin->m_vals[0] = std::min(pMin->m_vals[0], m_points[t2].m_vals[0]);
	pMin->m_vals[1] = std::min(pMin->m_vals[1], m_points[t2].m_vals[1]);
	pMin->m_vals[2] = std::min(pMin->m_vals[2], m_points[t2].m_vals[2]);
	pMax->m_vals[0] = std::max(pMax->m_vals[0], m_points[t2].m_vals[0]);
	pMax->m_vals[1] = std::max(pMax->m_vals[1], m_points[t2].m_vals[1]);
	pMax->m_vals[2] = std::max(pMax->m_vals[2], m_points[t2].m_vals[2]);
}

// static
WRayTraceTriMesh* WRayTraceTriMesh::makeQuadSurface(WRayTraceMaterial* pMaterial, W3DVector* p1, W3DVector* p2, W3DVector* p3, W3DVector* p4)
{
	bool imageTexture = (pMaterial->materialType() == WRayTraceMaterial::Image);
	WRayTraceTriMesh* pMesh = new WRayTraceTriMesh(pMaterial);
	pMesh->addPoint(*p1);
	pMesh->addPoint(*p2);
	pMesh->addPoint(*p3);
	pMesh->addPoint(*p4);
	pMesh->addTriangle(0, 1, 2);
	pMesh->addTriangle(2, 3, 0);
	if(imageTexture)
	{
		WImage* pImage = ((WRayTraceImageTexture*)pMaterial)->textureImage();
		pMesh->setTextureCoord(0, 0, 0);
		pMesh->setTextureCoord(1, (W3DReal)(pImage->width() - 1), 0);
		pMesh->setTextureCoord(2, (W3DReal)(pImage->width() - 1), (W3DReal)(pImage->height() - 1));
		pMesh->setTextureCoord(3, 0, (W3DReal)(pImage->height() - 1));
	}
	return pMesh;
}

// static
WRayTraceTriMesh* WRayTraceTriMesh::makeSingleTriangle(WRayTraceMaterial* pMaterial, W3DVector* p1, W3DVector* p2, W3DVector* p3)
{
	bool imageTexture = (pMaterial->materialType() == WRayTraceMaterial::Image);
	WRayTraceTriMesh* pMesh = new WRayTraceTriMesh(pMaterial);
	pMesh->addPoint(*p1);
	pMesh->addPoint(*p2);
	pMesh->addPoint(*p3);
	pMesh->addTriangle(0, 1, 2);
	if(imageTexture)
	{
		WImage* pImage = ((WRayTraceImageTexture*)pMaterial)->textureImage();
		pMesh->setTextureCoord(0, 0, 0);
		pMesh->setTextureCoord(1, (W3DReal)(pImage->width() - 1), 0);
		pMesh->setTextureCoord(2, 0, (W3DReal)(pImage->height() - 1));
	}
	return pMesh;
}

// static
WRayTraceTriMesh* WRayTraceTriMesh::makeCylinder(WRayTraceMaterial* pMaterial, W3DVector* pCenter1, W3DVector* pCenter2, W3DReal radius, size_t nSides, bool bEndCaps)
{
	// Compute t, u, and v as the three component vectors of the MakeCylinder. w is just any
	// vector that is not parallel to t
	W3DVector t(*pCenter1);
	t.subtract(*pCenter2);
	W3DVector w;
	if(t.m_vals[0] > t.m_vals[1] && t.m_vals[0] > t.m_vals[2])
		w.set(0, 1, 0);
	else
		w.set(1, 0, 0);
	W3DVector u, v;
	u.crossProduct(t, w);
	u.normalize();
	u.multiply(radius);
	v.crossProduct(t, u);
	v.normalize();
	v.multiply(radius);

	// Make the mesh
	size_t nVertices = nSides * 2 + (bEndCaps ? 2 : 0);
	bool imageTexture = (pMaterial->materialType() == WRayTraceMaterial::Image);
	WImage* pImage = imageTexture ? ((WRayTraceImageTexture*)pMaterial)->textureImage() : nullptr;
	WRayTraceTriMesh* pMesh = new WRayTraceTriMesh(pMaterial);
	W3DVector p1(*pCenter1);
	p1.add(u);
	W3DVector p2(*pCenter2);
	p2.add(u);
	W3DVector p3, p4;
	double dRads;
	double dStep = 2 * M_PI / nSides;
	if(bEndCaps)
	{
		pMesh->addPoint(*pCenter1);
		pMesh->addPoint(*pCenter2);
		if(pImage)
		{
			pMesh->setTextureCoord(pMesh->pointCount() - 2, (W3DReal)pImage->width() / 2, (W3DReal)pImage->height() / 2);
			pMesh->setTextureCoord(pMesh->pointCount() - 1, (W3DReal)pImage->width() / 2, (W3DReal)pImage->height() / 2);
		}
	}
	size_t nPrevVertexPos = nVertices;
	for(dRads = 2 * M_PI - .000000001; dRads > 0;) // the small constant is to ensure that rounding error doesn't cause an extra side to be added
	{
		// Move forward
		dRads -= dStep;
		p3.copy(p1);
		p4.copy(p2);

		// Compute the two vertices of the new edge
		w.copy(u);
		w.multiply((W3DReal)cos(dRads));
		p1.copy(v);
		p1.multiply((W3DReal)sin(dRads));
		p1.add(w);
		p2.copy(p1);
		p1.add(*pCenter1);
		p2.add(*pCenter2);
		pMesh->addPoint(p1);
		pMesh->addPoint(p2);

		// Set the texture coordinates
		if(pImage)
		{
			W3DReal y = (W3DReal)(dRads * (pImage->height() - 1) / (2 * M_PI));
			pMesh->setTextureCoord(pMesh->pointCount() - 2, 0, y);
			pMesh->setTextureCoord(pMesh->pointCount() - 1, (W3DReal)(pImage->width() - 1), y);
		}

		// Make the triangles, all facing outward (their vertices go
		// counter-clockwise seen from outside, which is the front for
		// rayDistanceToTriangle's culling).
		// SAKBox: the walls and the second cap used to face inward and the
		// first cap outward, so the mesh was inside out and inconsistent;
		// with culling on, most of a cylinder was invisible from outside.
		const size_t n1 = pMesh->pointCount() - 2, n2 = pMesh->pointCount() - 1; // new edge
		const size_t p1i = nPrevVertexPos - 2, p2i = nPrevVertexPos - 1; // previous edge
		pMesh->addTriangle(n2, p2i, n1);
		pMesh->addTriangle(p1i, n1, p2i);
		if(bEndCaps)
		{
			pMesh->addTriangle(n1, p1i, 0);
			pMesh->addTriangle(p2i, n2, 1);
		}

		nPrevVertexPos = pMesh->pointCount();
	}
	//pMesh->ComputePhongNormals();
	return pMesh;
}

// -----------------------------------------------------------------------------

// static
WRayTraceObject* WRayTraceObject::deserialize(WDomNode* pNode, WRayTraceScene* pScene)
{
	switch((ObjectType)pNode->getInt("type"))
	{
		case Sphere:
			return new WRayTraceSphere(pNode, pScene);
		case Triangle:
			return new WRayTraceTriangle(pNode, pScene);
	}
	throw Ex("Unexpected type");
	return nullptr;
}

// -----------------------------------------------------------------------------

WRayTraceSphere::WRayTraceSphere(WDomNode* pNode, WRayTraceScene* pScene)
: WRayTraceObject()
{
	m_pMaterial = pScene->material((size_t)pNode->getInt("material"));
	m_center.deserialize(pNode->get("center"));
	m_radius = pNode->getDouble("radius");
}

// virtual
WDomNode* WRayTraceSphere::serialize(WDom* pDoc, const WRayTraceScene* pScene) const
{
	WDomNode* pNode = pDoc->newObj();
	pNode->add(pDoc, "type", (long long)type());
	pNode->add(pDoc, "material", pScene->materialIndex(m_pMaterial));
	pNode->add(pDoc, "center", m_center.serialize(pDoc));
	pNode->add(pDoc, "radius", m_radius);
	return pNode;
}

// virtual
W3DReal WRayTraceSphere::rayDistance(W3DVector* pRayOrigin, W3DVector* pRayDirection)
{
	W3DReal b = (W3DReal)2 * (pRayDirection->m_vals[0] * (pRayOrigin->m_vals[0] - m_center.m_vals[0]) + pRayDirection->m_vals[1] * (pRayOrigin->m_vals[1] - m_center.m_vals[1]) + pRayDirection->m_vals[2] * (pRayOrigin->m_vals[2] - m_center.m_vals[2]));
	W3DReal c = pRayOrigin->m_vals[0] * pRayOrigin->m_vals[0] - (W3DReal)2 * pRayOrigin->m_vals[0] * m_center.m_vals[0] + m_center.m_vals[0] * m_center.m_vals[0] + pRayOrigin->m_vals[1] * pRayOrigin->m_vals[1] - (W3DReal)2 * pRayOrigin->m_vals[1] * m_center.m_vals[1] + m_center.m_vals[1] * m_center.m_vals[1] + pRayOrigin->m_vals[2] * pRayOrigin->m_vals[2] - (W3DReal)2 * pRayOrigin->m_vals[2] * m_center.m_vals[2] + m_center.m_vals[2] * m_center.m_vals[2] - m_radius * m_radius;
	W3DReal discriminant = b * b - (W3DReal)4 * c;
	if(discriminant < 0)
		return 0;
	W3DReal dist = (b + (W3DReal)sqrt(discriminant)) / (-2);
	if(dist > MIN_RAY_DISTANCE)
		return dist;
	dist = (b - (W3DReal)sqrt(discriminant)) / (-2);
	return dist;
}

//virtual
void WRayTraceSphere::normalVector(WRayTraceRay* pRay)
{
	pRay->m_normalVector.copy(pRay->m_collisionPoint);
	pRay->m_normalVector.subtract(m_center);
	pRay->m_normalVector.normalize();
}

//virtual
void WRayTraceSphere::adjustBoundingBox(W3DVector* pMin, W3DVector* pMax)
{
	pMin->m_vals[0] = std::min(pMin->m_vals[0], m_center.m_vals[0] - m_radius);
	pMin->m_vals[1] = std::min(pMin->m_vals[1], m_center.m_vals[1] - m_radius);
	pMin->m_vals[2] = std::min(pMin->m_vals[2], m_center.m_vals[2] - m_radius);
	pMax->m_vals[0] = std::max(pMax->m_vals[0], m_center.m_vals[0] + m_radius);
	pMax->m_vals[1] = std::max(pMax->m_vals[1], m_center.m_vals[1] + m_radius);
	pMax->m_vals[2] = std::max(pMax->m_vals[2], m_center.m_vals[2] + m_radius);
}

void WRayTraceSphere::drawWireFrame(WCamera* pCamera, WImage* pImage)
{
	W3DVector centr, perim, tmp;
	tmp.copy(*pCamera->viewSideVector());
	tmp.multiply(m_radius);
	tmp.add(m_center);
	pCamera->project(&m_center, &centr);
	if(centr.m_vals[2] < 0)
		return; // behind the camera
	pCamera->project(&tmp, &perim);
	double rad = sqrt((perim.m_vals[0] - centr.m_vals[0]) * (perim.m_vals[0] - centr.m_vals[0]) + (perim.m_vals[1] - centr.m_vals[1]) * (perim.m_vals[1] - centr.m_vals[1]));
	int bot = pImage->height() - 1;
	pImage->circle((int)floor(centr.m_vals[0] + 0.5), bot - (int)floor(centr.m_vals[1] + 0.5), (float)rad, 0xff00ffff);
}

// virtual
void WRayTraceSphere::center(W3DVector* pOutPoint)
{
	*pOutPoint = m_center; // SAKBox: was memcpy, undefined for a class with its own operator=
}

// -----------------------------------------------------------------------------

WRayTraceTriangle::WRayTraceTriangle(WDomNode* pNode, WRayTraceScene* pScene)
: WRayTraceObject()
{
	m_pMesh = pScene->mesh((size_t)pNode->getInt("mesh"));
	m_nIndex = (size_t)pNode->getInt("index");
	if(m_nIndex >= m_pMesh->triangleCount())
		throw Ex("out of range");
}

// virtual
WDomNode* WRayTraceTriangle::serialize(WDom* pDoc, const WRayTraceScene* pScene) const
{
	WDomNode* pNode = pDoc->newObj();
	pNode->add(pDoc, "type", (long long)type());
	pNode->add(pDoc, "mesh", pScene->meshIndex(m_pMesh));
	pNode->add(pDoc, "index", m_nIndex);
	return pNode;
}

//virtual
W3DReal WRayTraceTriangle::rayDistance(W3DVector* pRayOrigin, W3DVector* pRayDirection)
{
	return m_pMesh->rayDistanceToTriangle(m_nIndex, pRayOrigin, pRayDirection);
}

//virtual
void WRayTraceTriangle::normalVector(WRayTraceRay* pRay)
{
	m_pMesh->normalVector(pRay, m_nIndex);
}

//virtual
void WRayTraceTriangle::center(W3DVector* pOutPoint)
{
	m_pMesh->center(pOutPoint, m_nIndex);
}

//virtual
void WRayTraceTriangle::adjustBoundingBox(W3DVector* pMin, W3DVector* pMax)
{
	m_pMesh->adjustBoundingBox(m_nIndex, pMin, pMax);
}

W3DVector* WRayTraceTriangle::vertex(int nVertex)
{
	return m_pMesh->vertex(m_nIndex, nVertex);
}

void WRayTraceTriangle::drawWireFrame(WCamera* pCamera, WImage* pImage)
{
	W3DVector a, b, c;
	pCamera->project(m_pMesh->vertex(m_nIndex, 0), &a);
	pCamera->project(m_pMesh->vertex(m_nIndex, 1), &b);
	pCamera->project(m_pMesh->vertex(m_nIndex, 2), &c);
	if(a.m_vals[2] < 0 && b.m_vals[2] < 0 && c.m_vals[2] < 0)
		return;
	int bot = pImage->height() - 1;
	pImage->line((int)floor(a.m_vals[0] + 0.5), bot - (int)floor(a.m_vals[1] + 0.5), (int)floor(b.m_vals[0] + 0.5), bot - (int)floor(b.m_vals[1] + 0.5), 0xffff00ff);
	pImage->line((int)floor(b.m_vals[0] + 0.5), bot - (int)floor(b.m_vals[1] + 0.5), (int)floor(c.m_vals[0] + 0.5), bot - (int)floor(c.m_vals[1] + 0.5), 0xffff00ff);
	pImage->line((int)floor(c.m_vals[0] + 0.5), bot - (int)floor(c.m_vals[1] + 0.5), (int)floor(a.m_vals[0] + 0.5), bot - (int)floor(a.m_vals[1] + 0.5), 0xffff00ff);
}







WTriMeshBuilder::WTriMeshBuilder(WRayTraceMaterial* pMaterial)
: m_pMaterial(pMaterial)
{
	if(pMaterial->materialType() == WRayTraceMaterial::Image)
		throw Ex("Sorry, this class does not support image materials");
}

WTriMeshBuilder::~WTriMeshBuilder()
{
}

size_t WTriMeshBuilder::addPoint(const W3DVector& v)
{
	size_t index = 0;
	for(vector<W3DVector>::iterator it = m_points.begin(); it != m_points.end(); it++)
	{
		if(it->isEqual(v))
			return index;
		index++;
	}
	m_points.push_back(v);
	return index;
}

void WTriMeshBuilder::add(const W3DVector& a, const W3DVector& b, const W3DVector& c)
{
	if(a.isEqual(b) || b.isEqual(c) || a.isEqual(c))
		return;
	m_indexes.push_back(addPoint(a));
	m_indexes.push_back(addPoint(b));
	m_indexes.push_back(addPoint(c));
}

WRayTraceTriMesh* WTriMeshBuilder::mesh()
{
	WRayTraceTriMesh* pMesh = new WRayTraceTriMesh(m_pMaterial);
	for(size_t i = 0; i < m_points.size(); i++)
		pMesh->addPoint(m_points[i]);
	for(size_t i = 0; i < m_indexes.size();)
	{
		size_t a = m_indexes[i++];
		size_t b = m_indexes[i++];
		size_t c = m_indexes[i++];
		pMesh->addTriangle(a, b, c);
	}
	m_points.clear();
	m_indexes.clear();
	return pMesh;
}





W3dLetterMaker::W3dLetterMaker(WRayTraceMaterial* pMaterial)
: m_builder(pMaterial), m_lineWidth(0.1), m_circleSegments(24), m_spaceWidth(0.4)
{
	m_pos.set(0, 0, 0);
	m_basis.setToIdentity();
}

W3dLetterMaker::~W3dLetterMaker()
{
}

void W3dLetterMaker::move(W3DReal dx, W3DReal dy, W3DReal dz)
{
	W3DVector tmp(dx, dy, dz);
	W3DVector ofs;
	m_basis.multiply(&tmp, &ofs);
	m_pos.add(ofs);
}

void W3dLetterMaker::move(W3DVector& vec)
{
	W3DVector ofs;
	m_basis.multiply(&vec, &ofs);
	m_pos.add(ofs);
	m_pos.add(vec);
}

void W3dLetterMaker::scale(W3DReal width, W3DReal height, W3DReal depth)
{
	m_basis.m_rows[0].multiply(width);
	m_basis.m_rows[1].multiply(height);
	m_basis.m_rows[2].multiply(depth);
}

void W3dLetterMaker::rotate(W3DReal yaw, W3DReal pitch, W3DReal roll)
{
	if(roll != 0)
	{
		W3DMatrix a;
		a.copy(&m_basis);
		W3DMatrix b;
		b.setToIdentity();
		b.m_rows[0].set(cos(roll), sin(roll), 0.0);
		b.m_rows[1].set(-sin(roll), cos(roll), 0.0);
		m_basis.multiply(&a, &b);
	}
	if(pitch != 0)
	{
		W3DMatrix a;
		a.copy(&m_basis);
		W3DMatrix b;
		b.setToIdentity();
		b.m_rows[1].set(0.0, cos(pitch), sin(pitch));
		b.m_rows[2].set(0.0, -sin(pitch), cos(pitch));
		m_basis.multiply(&a, &b);
	}
	if(yaw != 0)
	{
		W3DMatrix a;
		a.copy(&m_basis);
		W3DMatrix b;
		b.setToIdentity();
		b.m_rows[0].set(cos(yaw), 0.0, sin(yaw));
		b.m_rows[2].set(-sin(yaw), 0.0, cos(yaw));
		m_basis.multiply(&a, &b);
	}
}

double W3dLetterMaker::letterWidth(char c)
{
	double w = 0.5;
	switch(c)
	{
		case ' ': w = m_spaceWidth; break;
		case '"': w = m_lineWidth + m_lineWidth; break;
		case '\'': w = m_lineWidth; break;
		case '(':
		case ')':
		case ',':
		case 'i': w = 0.1; break;
		case '.': w = 0.2; break;
		case 'j':
		case 'l': w = 0.225; break;
		case 't': w = 0.3; break;
		case '+':
		case '-': w = 0.4; break;
		case '@':
		case 'M':
		case 'W':
		case 'm':
		case 'q':
		case 'w': w = 0.6; break;
	}
	return w + m_lineWidth;
}

WRayTraceTriMesh* W3dLetterMaker::makeLetter(char c)
{
	switch(c)
	{
		case ' ':
			break;
		case '!':
			vbar(0.0, 0.3, 0.0, 1.0);
			curve(0.025, m_lineWidth, 0.5 * m_lineWidth, 0.75 * m_lineWidth, 0.0, 2.0);
			break;
		case '"':
			vbar(0.0, 0.8, 0.0, 1.0);
			vbar(m_lineWidth + m_lineWidth, 0.8, m_lineWidth + m_lineWidth, 1.0);
			break;
		case '#':
			vbar(0.1, 0.0, 0.2, 1.0);
			vbar(0.3, 0.0, 0.4, 1.0);
			hbar(0.0, 0.333 - 0.5 * m_lineWidth, 0.5 + m_lineWidth);
			hbar(0.0, 0.667 - 0.5 * m_lineWidth, 0.5 + m_lineWidth);
			break;
		case '\'': // apostrophe
			vbar(0.0, 0.8, 0.0, 1.0);
			break;
		case '(':
			curve(0.1, 0.5, 0.4, 0.8, 1.8, 2.2);
			break;
		case ')':
			curve(0.5 - m_lineWidth, 0.5, 0.4, 0.8, 0.8, 1.2);
			break;
		case '+':
			vbar(0.2, 0.3, 0.2, 0.7);
			hbar(0.0, 0.5 - 0.5 * m_lineWidth, 0.4);
			break;
		case ',':
			curve(0.05, m_lineWidth, 0.5 * m_lineWidth, 0.75 * m_lineWidth, 0.0, 2.0);
			curve(0.05, m_lineWidth, 0.5 * m_lineWidth, 1.6 * m_lineWidth, 1.4, 2.0);
			break;
		case '-':
			hbar(0.0, 0.5 - 0.5 * m_lineWidth, 0.4);
			break;
		case '.':
			curve(0.025, m_lineWidth, 0.5 * m_lineWidth, 0.75 * m_lineWidth, 0.0, 2.0);
			break;
		case '/':
			vbar(0.0, 0.0, 0.5, 1.0);
			break;
		case '0':
			curve(0.5, 0.6, 0.5, 0.4, 0.0, 1.0);
			curve(0.5, 0.4, 0.5, 0.4, 1.0, 2.0);
			vbar(0.0, 0.4, 0.0, 0.6);
			vbar(0.5, 0.4, 0.5, 0.6);
			vbar(0.0, 0.4, 0.5, 0.6);
			break;
		case '1':
			vbar(0.15, 1.0, 0.0, 0.85);
			vbar(0.15, m_lineWidth, 0.15, 1.0);
			hbar(0.0, 0.0, 0.3 + m_lineWidth);
			break;
		case '2':
			curve(0.5, 0.65, 0.5, 0.35, 0.0, 1.0);
			vbar(0.0, m_lineWidth, 0.5, 0.65);
			hbar(0.0, 0.0, 0.5 + m_lineWidth);
			break;
		case '3':
		{
			double w = 0.5;
			double h = (w + m_lineWidth) / (4 * (w + m_lineWidth) - 2 * m_lineWidth);
			curve(0.5, 1.0 - h, w, h, 1.5, 3.0);
			curve(0.5, h, w, h, 1.0, 2.5);
		}
		break;
		case '4':
			vbar(0.375, 0.0, 0.375, 1.0);
			vbar(0.375, 1.0, 0.0, 0.35);
			hbar(0.0, 0.35 - m_lineWidth, 0.5 + m_lineWidth);
			break;
		case '5':
		{
			double w = 0.5;
			double h = 0.30;
			double hScale = 2.0 * h / (w + m_lineWidth);
			hbar(0.0, 1.0 - m_lineWidth, 0.5 + m_lineWidth);
			vbar(0.0, h + h, 0.0, 1.0 - m_lineWidth);
			hbar(0.0, h + h - m_lineWidth * hScale, 0.25 + 0.5 * m_lineWidth, hScale);
			curve(0.5, h, 0.5, h, 1.0, 2.5);
		}
		break;
		case '6':
			curve(0.5, 0.7, 0.5, 0.3, 0.25, 1.0);
			vbar(0.0, 0.3, 0.0, 0.7);
			curve(0.5, 0.3, 0.5, 0.3, 0.0, 2.0);
			break;
		case '7':
			hbar(0.0, 1.0 - m_lineWidth, 0.5 + m_lineWidth);
			vbar(0.15, 0.0, 0.5, 1.0 - m_lineWidth);
			break;
		case '8':
		{
			double w = 0.5;
			double h = (w + m_lineWidth) / (4 * (w + m_lineWidth) - 2 * m_lineWidth);
			curve(0.5, 1.0 - h, w, h, 0.0, 2.0);
			curve(0.5, h, w, h, 0.0, 2.0);
		}
		break;
		case '9':
			curve(0.5, 0.7, 0.5, 0.3, 0.0, 2.0);
			vbar(0.5, 0.3, 0.5, 0.7);
			curve(0.5, 0.3, 0.5, 0.3, 1.25, 2.0);
			break;
		case ':':
			curve(0.025, 0.4 + m_lineWidth, 0.5 * m_lineWidth, 0.75 * m_lineWidth, 0.0, 2.0);
			curve(0.025, m_lineWidth, 0.5 * m_lineWidth, 0.75 * m_lineWidth, 0.0, 2.0);
			break;
		case ';':
			curve(0.05, m_lineWidth, 0.5 * m_lineWidth, 0.75 * m_lineWidth, 0.0, 2.0);
			curve(0.05, m_lineWidth, 0.5 * m_lineWidth, 1.6 * m_lineWidth, 1.4, 2.0);
			curve(0.025, 0.4 + m_lineWidth, 0.5 * m_lineWidth, 0.75 * m_lineWidth, 0.0, 2.0);
			break;
		case '<':
			vbar(0.5, 0.8, 0.0, 0.5);
			vbar(0.0, 0.5, 0.5, 0.2);
			break;
		case '=':
			hbar(0.0, 0.3, 0.5 + m_lineWidth);
			hbar(0.0, 0.6, 0.5 + m_lineWidth);
			break;
		case '>':
			vbar(0.0, 0.8, 0.5, 0.5);
			vbar(0.5, 0.5, 0.0, 0.2);
			break;
		case '?':
		{
			curve(0.5, 0.75, 0.5, 0.25, 0.0, 1.0);
			vbar(0.5, 0.65, 0.5, 0.75);
			double h = (0.65 - 0.4) * (0.25 + m_lineWidth) / (0.25 * 2);
			curve(0.5, 0.65, 0.25, h, 1.5, 2.0);
			curve(0.5, 0.4, 0.25, h, 0.5, 1.0);
			vbar(0.25, 0.3, 0.25, 0.4);
			curve(0.275, m_lineWidth, 0.5 * m_lineWidth, 0.75 * m_lineWidth, 0.0, 2.0);
		}
		break;
		case '@':
			curve(0.42, 0.5, 0.26, 0.16, 0.0, 2.0);
			curve(0.6, 0.5, 0.6, 0.36, 0.0, 1.7);
			vbar(0.42, 0.4, 0.42, 0.66);
			vbar(0.6, 0.4, 0.6, 0.5);
			curve(0.6, 0.4, 0.18, 0.09, 1.0, 2.0);
			break;
		case 'A':
			vbar(0.0, 0.0, 0.25, 1.0);
			vbar(0.25, 1.0, 0.5, 0.0);
			hbar(0.08, 0.2, 0.32 + m_lineWidth);
			break;
		case 'B':
		{
			vbar(0.0, 0.0, 0.0, 1.0);
			double w = 0.6;
			double h = (w + m_lineWidth) / (4 * (w + m_lineWidth) - 2 * m_lineWidth);
			double hScale = 2.0 * h / (w + m_lineWidth);
			double bl = 0.5 + m_lineWidth - 0.5 * (w + m_lineWidth) - m_lineWidth;
			curve(0.5, 1.0 - h, w, h, 1.5, 2.5);
			curve(0.5, h, w, h, 1.5, 2.5);
			hbar(m_lineWidth, 1.0 - m_lineWidth * hScale, bl, hScale);
			hbar(m_lineWidth, 0.5 - 0.5 * m_lineWidth * hScale, bl, hScale);
			hbar(m_lineWidth, 0.0, bl, hScale);
		}
		break;
		case 'C':
			curve(0.5, 0.75, 0.5, 0.25, 0.0, 1.0);
			vbar(0.0, 0.25, 0.0, 0.75);
			curve(0.5, 0.25, 0.5, 0.25, 1.0, 2.0);
			break;
		case 'D':
			curve(0.5, 0.5, 0.6, 0.5, 1.5, 2.5);
			vbar(0.0, 0.0, 0.0, 1.0);
			hbar(m_lineWidth, 1.0 - m_lineWidth / (0.6 + m_lineWidth), 0.2 - 0.5 * m_lineWidth, 1.0 / (0.6 + m_lineWidth));
			hbar(m_lineWidth, 0.0, 0.2 - 0.5 * m_lineWidth, 1.0 / (0.6 + m_lineWidth));
			break;
		case 'E':
			vbar(0.0, 0.0, 0.0, 1.0);
			hbar(m_lineWidth, 1.0 - m_lineWidth, 0.5);
			hbar(m_lineWidth, 0.5 - 0.5 * m_lineWidth, 0.5);
			hbar(m_lineWidth, 0.0, 0.5);
			break;
		case 'F':
			vbar(0.0, 0.0, 0.0, 1.0);
			hbar(m_lineWidth, 1.0 - m_lineWidth, 0.5 - m_lineWidth);
			hbar(m_lineWidth, 0.5 - 0.5 * m_lineWidth, 0.5 - m_lineWidth);
			break;
		case 'G':
			curve(0.5, 0.75, 0.5, 0.25, 0.0, 1.0);
			vbar(0.0, 0.25, 0.0, 0.75);
			curve(0.5, 0.25, 0.5, 0.25, 1.0, 2.0);
			hbar(0.25, 0.5 - 0.5 * m_lineWidth, 0.25);
			vbar(0.5, 0.0, 0.5, 0.5 + 0.5 * m_lineWidth);
			break;
		case 'H':
			vbar(0.0, 0.0, 0.0, 1.0);
			hbar(m_lineWidth, 0.5 - 0.5 * m_lineWidth, 0.5 - m_lineWidth);
			vbar(0.5, 0.0, 0.5, 1.0);
			break;
		case 'I':
			hbar(0.0, 1.0 - m_lineWidth, 0.5 + m_lineWidth);
			vbar(0.25, m_lineWidth, 0.25, 1.0 - m_lineWidth);
			hbar(0.0, 0.0, 0.5 + m_lineWidth);
			break;
		case 'J':
			hbar(0.2, 1.0 - m_lineWidth, 0.3 + m_lineWidth);
			vbar(0.35, 0.2, 0.35, 1.0 - m_lineWidth);
			curve(0.35, 0.2, 0.35, 0.2, 1.0, 2.0);
			break;
		case 'K':
			vbar(0.0, 0.0, 0.0, 1.0);
			vbar(0.0, 0.5, 0.5 - 0.4 * m_lineWidth, 1.0, 1.4);
			vbar(0.0, 0.5, 0.5 - 0.4 * m_lineWidth, 0.0, 1.4);
			break;
		case 'L':
			vbar(0.0, 0.0, 0.0, 1.0);
			hbar(m_lineWidth, 0.0, 0.5);
			break;
		case 'M':
			vbar(0.0, 0.0, 0.0, 1.0);
			vbar(0.0, 1.0, 0.3, 0.5);
			vbar(0.3, 0.5, 0.6, 1.0);
			vbar(0.6, 1.0, 0.6, 0.0);
			break;
		case 'N':
			vbar(0.0, 0.0, 0.0, 1.0);
			vbar(0.0, 1.0, 0.5, 0.0);
			vbar(0.5, 1.0, 0.5, 0.0);
			break;
		case 'O':
			curve(0.5, 0.8, 0.5, 0.2, 0.0, 1.0);
			curve(0.5, 0.2, 0.5, 0.2, 1.0, 2.0);
			vbar(0.0, 0.2, 0.0, 0.8);
			vbar(0.5, 0.2, 0.5, 0.8);
			break;
		case 'P':
		{
			double h = (0.6 + m_lineWidth) / (4 * (0.6 + m_lineWidth) - 2 * m_lineWidth);
			curve(0.5, 1.0 - h, 0.6, h, 1.5, 2.5);
			vbar(0.0, 0.0, 0.0, 1.0);
			hbar(m_lineWidth, 1.0 - 2 * h * m_lineWidth / (0.6 + m_lineWidth), 0.2, 2 * h / (0.6 + m_lineWidth));
			hbar(m_lineWidth, 0.5 - h * m_lineWidth / (0.6 + m_lineWidth), 0.2, 2 * h / (0.6 + m_lineWidth));
		}
		break;
		case 'Q':
			curve(0.5, 0.8, 0.5, 0.2, 0.0, 1.0);
			curve(0.5, 0.2, 0.5, 0.2, 1.0, 2.0);
			vbar(0.0, 0.2, 0.0, 0.8);
			vbar(0.5, 0.2, 0.5, 0.8);
			vbar(0.25, 0.5, 0.5, 0.0);
			break;
		case 'R':
		{
			double h = (0.6 + m_lineWidth) / (4 * (0.6 + m_lineWidth) - 2 * m_lineWidth);
			curve(0.5, 1.0 - h, 0.6, h, 1.5, 2.5);
			vbar(0.0, 0.0, 0.0, 1.0);
			hbar(m_lineWidth, 1.0 - 2 * h * m_lineWidth / (0.6 + m_lineWidth), 0.2, 2 * h / (0.6 + m_lineWidth));
			hbar(m_lineWidth, 0.5 - h * m_lineWidth / (0.6 + m_lineWidth), 0.2, 2 * h / (0.6 + m_lineWidth));
			vbar(0.2, 0.5, 0.5, 0.0);
		}
		break;
		case 'S':
			curve(0.5, 0.75, 0.5, 0.25, 0.0, 1.0);
			curve(0.5, 0.75, 0.5, 0.25 + 0.5 * m_lineWidth, 1.0, 1.5);
			curve(0.5, 0.25, 0.5, 0.25 + 0.5 * m_lineWidth, 0.0, 0.5);
			curve(0.5, 0.25, 0.5, 0.25, 1.0, 2.0);
			break;
		case 'T':
			hbar(0.0, 1.0 - m_lineWidth, 0.5 + m_lineWidth);
			vbar(0.25, 0.0, 0.25, 1.0 - m_lineWidth);
			break;
		case 'U':
			curve(0.5, 0.2, 0.5, 0.2, 1.0, 2.0);
			vbar(0.0, 0.2, 0.0, 1.0);
			vbar(0.5, 0.2, 0.5, 1.0);
			break;
		case 'V':
			vbar(0.0, 1.0, 0.25, 0.0);
			vbar(0.25, 0.0, 0.5, 1.0);
			break;
		case 'W':
			vbar(0.0, 1.0, 0.15, 0.0);
			vbar(0.15, 0.0, 0.3, 0.7);
			vbar(0.3, 0.7, 0.45, 0.0);
			vbar(0.45, 0.0, 0.6, 1.0);
			break;
		case 'X':
			vbar(0.0, 0.0, 0.5, 1.0);
			vbar(0.0, 1.0, 0.5, 0.0);
			break;
		case 'Y':
			vbar(0.0, 1.0, 0.25, 0.5);
			vbar(0.25, 0.5, 0.5, 1.0);
			vbar(0.25, 0.0, 0.25, 0.5);
			break;
		case 'Z':
			vbar(0.0, m_lineWidth, 0.5, 1.0 - m_lineWidth);
			hbar(0.0, 0.0, 0.5 + m_lineWidth);
			hbar(0.0, 1.0 - m_lineWidth, 0.5 + m_lineWidth);
			break;
		case '\\':
			vbar(0.0, 1.0, 0.5, 0.0);
			break;
		case 'a':
			curve(0.5, 0.5, 0.5, 0.2, 0.0, 1.0);
			curve(0.5, 0.2, 0.5, 0.15 + 0.3 * m_lineWidth, 0.5, 1.0);
			curve(0.5, 0.5, 0.5, 0.15 + 0.3 * m_lineWidth, 1.5, 2.0);
			curve(0.5, 0.2, 0.5, 0.2, 1.0, 2.0);
			vbar(0.5, 0.0, 0.5, 0.5);
			break;
		case 'b':
			curve(0.5, 0.5, 0.5, 0.2, 0.0, 1.0);
			curve(0.5, 0.2, 0.5, 0.2, 1.0, 2.0);
			vbar(0.0, 0.0, 0.0, 1.0);
			vbar(0.5, 0.2, 0.5, 0.5);
			break;
		case 'c':
			curve(0.5, 0.5, 0.5, 0.2, 0.0, 1.0);
			curve(0.5, 0.2, 0.5, 0.2, 1.0, 2.0);
			vbar(0.0, 0.2, 0.0, 0.5);
			break;
		case 'd':
			curve(0.5, 0.5, 0.5, 0.2, 0.0, 1.0);
			curve(0.5, 0.2, 0.5, 0.2, 1.0, 2.0);
			vbar(0.0, 0.2, 0.0, 0.5);
			vbar(0.5, 0.0, 0.5, 1.0);
			break;
		case 'e':
			curve(0.5, 0.5, 0.5, 0.2, 0.0, 1.0);
			curve(0.5, 0.2, 0.5, 0.2, 1.0, 2.0);
			vbar(0.0, 0.2, 0.0, 0.5);
			vbar(0.5, 0.35 - 0.5 * m_lineWidth, 0.5, 0.5);
			hbar(m_lineWidth, 0.35 - 0.5 * m_lineWidth, 0.5);
			break;
		case 'f':
			curve(0.5, 0.8, 0.4, 0.2, 0.0, 1.0);
			vbar(0.1, 0.0, 0.1, 0.8);
			hbar(0.0, 0.6 - m_lineWidth, 0.4);
			break;
		case 'g':
			curve(0.5, 0.5, 0.5, 0.2, 0.0, 1.0);
			curve(0.5, 0.2, 0.5, 0.2, 1.0, 2.0);
			vbar(0.0, 0.2, 0.0, 0.5);
			vbar(0.5, -0.1, 0.5, 0.7);
			curve(0.5, -0.1, 0.5, 0.2, 1.0, 2.0);
			break;
		case 'h':
			curve(0.5, 0.5, 0.5, 0.2, 0.0, 1.0);
			vbar(0.0, 0.0, 0.0, 1.0);
			vbar(0.5, 0.0, 0.5, 0.5);
			break;
		case 'i':
			vbar(0.0, 0.2, 0.0, 0.5);
			curve(0.25, 0.2, 0.25, 0.2, 1.0, 1.5);
			curve(0.0, 0.7, 0.25 * m_lineWidth, 0.625 * m_lineWidth, 0.0, 2.0);
			break;
		case 'j':
			vbar(0.125, -0.1, 0.125, 0.5);
			curve(0.125, -0.1, 0.25, 0.2, 1.5, 2.0);
			curve(0.125, 0.7, 0.25 * m_lineWidth, 0.625 * m_lineWidth, 0.0, 2.0);
			break;
		case 'k':
			vbar(0.0, 0.0, 0.0, 1.0);
			vbar(0.0, 0.35, 0.5 - 0.4 * m_lineWidth, 0.7, 1.4);
			vbar(0.0, 0.35, 0.5 - 0.4 * m_lineWidth, 0.0, 1.4);
			break;
		case 'l':
			vbar(0.0, 0.2, 0.0, 1.0);
			curve(0.25, 0.2, 0.25, 0.2, 1.0, 1.5);
			break;
		case 'm':
			curve(0.3, 0.5, 0.3, 0.2, 0.0, 1.0);
			curve(0.6, 0.5, 0.3, 0.2, 0.0, 1.0);
			vbar(0.0, 0.0, 0.0, 0.7);
			vbar(0.3, 0.0, 0.3, 0.5);
			vbar(0.6, 0.0, 0.6, 0.5);
			break;
		case 'n':
			curve(0.5, 0.5, 0.5, 0.2, 0.0, 1.0);
			vbar(0.0, 0.0, 0.0, 0.7);
			vbar(0.5, 0.0, 0.5, 0.5);
			break;
		case 'o':
			curve(0.5, 0.5, 0.5, 0.2, 0.0, 1.0);
			curve(0.5, 0.2, 0.5, 0.2, 1.0, 2.0);
			vbar(0.0, 0.2, 0.0, 0.5);
			vbar(0.5, 0.2, 0.5, 0.5);
			break;
		case 'p':
			curve(0.5, 0.5, 0.5, 0.2, 0.0, 1.0);
			curve(0.5, 0.2, 0.5, 0.2, 1.0, 2.0);
			vbar(0.0, -0.3, 0.0, 0.7);
			vbar(0.5, 0.2, 0.5, 0.5);
			break;
		case 'q':
			curve(0.5, 0.5, 0.5, 0.2, 0.0, 1.0);
			curve(0.5, 0.2, 0.5, 0.2, 1.0, 2.0);
			vbar(0.0, 0.2, 0.0, 0.5);
			vbar(0.5, -0.1, 0.5, 0.7);
			curve(0.7, -0.1, 0.2, 0.2, 1.0, 1.5);
			break;
		case 'r':
			curve(0.5, 0.5, 0.5, 0.2, 0.0, 1.0);
			vbar(0.0, 0.0, 0.0, 0.7);
			break;
		case 's':
			curve(0.5, 0.5, 0.5, 0.2, 0.0, 1.0);
			curve(0.5, 0.5, 0.5, 0.15 + 0.3 * m_lineWidth, 1.0, 1.5);
			curve(0.5, 0.2, 0.5, 0.15 + 0.3 * m_lineWidth, 0.0, 0.5);
			curve(0.5, 0.2, 0.5, 0.2, 1.0, 2.0);
			break;
		case 't':
			vbar(0.15, 0.2, 0.15, 1.0);
			hbar(0.0, 0.7 - m_lineWidth, 0.3 + m_lineWidth);
			curve(0.4, 0.2, 0.25, 0.2, 1.0, 1.5);
			break;
		case 'u':
			curve(0.5, 0.2, 0.5, 0.2, 1.0, 2.0);
			vbar(0.0, 0.2, 0.0, 0.7);
			vbar(0.5, 0.0, 0.5, 0.7);
			break;
		case 'v':
			vbar(0.0, 0.7, 0.25, 0.0);
			vbar(0.25, 0.0, 0.5, 0.7);
			break;
		case 'w':
			vbar(0.0, 0.7, 0.15, 0.0);
			vbar(0.15, 0.0, 0.3, 0.5);
			vbar(0.3, 0.5, 0.45, 0.0);
			vbar(0.45, 0.0, 0.6, 0.7);
			break;
		case 'x':
			vbar(0.0, 0.0, 0.5, 0.7);
			vbar(0.0, 0.7, 0.5, 0.0);
			break;
		case 'y':
			curve(0.5, 0.2, 0.5, 0.2, 1.0, 2.0);
			vbar(0.0, 0.2, 0.0, 0.7);
			vbar(0.5, -0.1, 0.5, 0.7);
			curve(0.5, -0.1, 0.5, 0.2, 1.0, 2.0);
			break;
		case 'z':
			vbar(0.0, m_lineWidth, 0.5, 0.7 - m_lineWidth);
			hbar(0.0, 0.0, 0.5 + m_lineWidth);
			hbar(0.0, 0.7 - m_lineWidth, 0.5 + m_lineWidth);
			break;
		default:
			// make an asterisk
			vbar(0.1, 0.0, 0.4, 0.7);
			vbar(0.1, 0.7, 0.4, 0.0);
			hbar(0.0, 0.35 - 0.5 * m_lineWidth, 0.5 + m_lineWidth);
			break;
	};
	return m_builder.mesh();
}

WRayTraceTriMesh* W3dLetterMaker::specialChar(const char* szName)
{
	if(strcmp(szName, "iota") == 0)
	{
		vbar(0.0, 0.2, 0.0, 0.5);
		curve(0.25, 0.2, 0.25, 0.2, 1.0, 1.5);
	}
	else
		throw Ex("Unrecognized special char");
	return m_builder.mesh();
}

void W3dLetterMaker::writeString(WRayTraceScene* pScene, const char* szString)
{
	while(*szString != '\0')
	{
		pScene->addMesh(makeLetter(*szString));
		move(letterWidth(*szString) + m_lineWidth);
		szString++;
	}
}

double W3dLetterMaker::measureWidth(const char* szString)
{
	double width = 0.0;
	while(*szString != '\0')
	{
		width += letterWidth(*szString) + m_lineWidth;
		szString++;
	}
	return width;
}

void W3dLetterMaker::set(W3DVector* pBack, W3DVector* pFront, W3DReal x, W3DReal y)
{
	W3DVector tmp(x, y, 0.0);
	m_basis.multiply(&tmp, pBack);
	pBack->add(m_pos);
	tmp.set(x, y, 1.0);
	m_basis.multiply(&tmp, pFront);
	pFront->add(m_pos);
}

void W3dLetterMaker::addQuad(W3DReal x1, W3DReal y1, W3DReal x2, W3DReal y2, W3DReal x3, W3DReal y3, W3DReal x4, W3DReal y4)
{
	W3DVector bl1, br1, bl2, br2;
	W3DVector fl1, fr1, fl2, fr2;
	set(&bl1, &fl1, x1, y1);
	set(&br1, &fr1, x2, y2);
	set(&bl2, &fl2, x3, y3);
	set(&br2, &fr2, x4, y4);

	// front face
	m_builder.add(fl1, fr1, fl2);
	m_builder.add(fl2, fr2, fr1);

	// back face
	m_builder.add(bl1, br1, bl2);
	m_builder.add(bl2, br2, br1);

	// left face
	m_builder.add(fl1, fl2, bl1);
	m_builder.add(bl1, bl2, fl2);

	// right face
	m_builder.add(fr1, fr2, br1);
	m_builder.add(br1, br2, fr2);

	// top face
	m_builder.add(fl1, fr1, bl1);
	m_builder.add(bl1, br1, fr1);

	// bottom face
	m_builder.add(fl2, fr2, bl2);
	m_builder.add(bl2, br2, fr2);
}

void W3dLetterMaker::vbar(W3DReal x1, W3DReal y1, W3DReal x2, W3DReal y2, W3DReal width)
{
	addQuad(x1, y1, x1 + width * m_lineWidth, y1, x2, y2, x2 + width * m_lineWidth, y2);
}

void W3dLetterMaker::hbar(W3DReal x, W3DReal y, W3DReal w, W3DReal height)
{
	addQuad(x, y + m_lineWidth * height, x + w, y + m_lineWidth * height, x, y, x + w, y);
}

void W3dLetterMaker::curve(W3DReal x, W3DReal y, W3DReal w, W3DReal hh, double from, double to)
{
	double fromRads = from * M_PI;
	double toRads = to * M_PI;
	if(toRads < fromRads)
		std::swap(fromRads, toRads);
	size_t segments = size_t((double(m_circleSegments) + 0.5) * (toRads - fromRads) / (2.0 * M_PI));
	double step = (toRads - fromRads) / segments;
	double r = fromRads;
	double cx = x - 0.5 * (w - m_lineWidth);
	double cy = y;
	double innerRadius = x - cx;
	double outerRadius = innerRadius + m_lineWidth;
	double verticalScale = hh / outerRadius;
	for(size_t s = 0; s < segments; s++)
	{
		double p = r;
		r += step;
		addQuad(cx + outerRadius * cos(p), cy + verticalScale * outerRadius * sin(p), cx + outerRadius * cos(r), cy + verticalScale * outerRadius * sin(r), cx + innerRadius * cos(p), cy + verticalScale * innerRadius * sin(p), cx + innerRadius * cos(r), cy + verticalScale * innerRadius * sin(r));
	}
}

} // namespace WClasses

