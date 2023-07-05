// This header file defines a 3D bounding box class and accessor methods.

// This code contains NVIDIA Confidential Information and is disclosed to you
// under a form of NVIDIA software license agreement provided separately to you.
//
// Notice
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation and
// any modifications thereto. Any use, reproduction, disclosure, or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA Corporation is strictly prohibited.
//
// ALL NVIDIA DESIGN SPECIFICATIONS, CODE ARE PROVIDED "AS IS.". NVIDIA MAKES
// NO WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ALL IMPLIED WARRANTIES OF NONINFRINGEMENT,
// MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.
//
// Information and code furnished is believed to be accurate and reliable.
// However, NVIDIA Corporation assumes no responsibility for the consequences of use of such
// information or for any infringement of patents or other rights of third parties that may
// result from its use. No license is granted by implication or otherwise under any patent
// or patent rights of NVIDIA Corporation. Details are subject to change without notice.
// This code supersedes and replaces all information previously supplied.
// NVIDIA Corporation products are not authorized for use as critical
// components in life support devices or systems without express written approval of
// NVIDIA Corporation.
//
// Copyright (c) 2008-2014 NVIDIA Corporation. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.

#ifndef NV_ISAAC_MATH_BOUNDS3_H
#define NV_ISAAC_MATH_BOUNDS3_H

/** \addtogroup foundation
@{
*/

#include "NvIsaacTransform.h"
#include "NvIsaacMat33.h"

#if !NV_DOXYGEN
namespace NvIsaac
{
#endif

// maximum extents defined such that floating point exceptions are avoided for standard use cases
#define NV_MAX_BOUNDS_EXTENTS (NV_MAX_REAL * 0.25f)

/**
\brief Class representing 3D range or axis aligned bounding box.

Stored as minimum and maximum extent corners. Alternate representation
would be center and dimensions.
May be empty or nonempty. For nonempty bounds, minimum <= maximum has to hold for all axes.
Empty bounds have to be represented as minimum = NV_MAX_BOUNDS_EXTENTS and maximum = -NV_MAX_BOUNDS_EXTENTS for all
axes.
All other representations are invalid and the behavior is undefined.
*/
class Bounds3
{
  public:
	/**
	\brief Default constructor, not performing any initialization for performance reason.
	\remark Use empty() function below to construct empty bounds.
	*/
	 inline Bounds3()
	{
	}

	/**
	\brief Construct from two bounding points
	*/
	 inline Bounds3(const Vec3& minimum, const Vec3& maximum);

	/**
	\brief Return empty bounds.
	*/
	static  inline Bounds3 empty();

	/**
	\brief returns the AABB containing v0 and v1.
	\param v0 first point included in the AABB.
	\param v1 second point included in the AABB.
	*/
	static  inline Bounds3 boundsOfPoints(const Vec3& v0, const Vec3& v1);

	/**
	\brief returns the AABB from center and extents vectors.
	\param center Center vector
	\param extent Extents vector
	*/
	static  inline Bounds3 centerExtents(const Vec3& center, const Vec3& extent);

	/**
	\brief Construct from center, extent, and (not necessarily orthogonal) basis
	*/
	static  inline Bounds3
	basisExtent(const Vec3& center, const Mat33& basis, const Vec3& extent);

	/**
	\brief Construct from pose and extent
	*/
	static  inline Bounds3 poseExtent(const Transform& pose, const Vec3& extent);

	/**
	\brief gets the transformed bounds of the passed AABB (resulting in a bigger AABB).

	This version is safe to call for empty bounds.

	\param[in] matrix Transform to apply, can contain scaling as well
	\param[in] bounds The bounds to transform.
	*/
	static  inline Bounds3 transformSafe(const Mat33& matrix, const Bounds3& bounds);

	/**
	\brief gets the transformed bounds of the passed AABB (resulting in a bigger AABB).

	Calling this method for empty bounds leads to undefined behavior. Use #transformSafe() instead.

	\param[in] matrix Transform to apply, can contain scaling as well
	\param[in] bounds The bounds to transform.
	*/
	static  inline Bounds3 transformFast(const Mat33& matrix, const Bounds3& bounds);

	/**
	\brief gets the transformed bounds of the passed AABB (resulting in a bigger AABB).

	This version is safe to call for empty bounds.

	\param[in] transform Transform to apply, can contain scaling as well
	\param[in] bounds The bounds to transform.
	*/
	static  inline Bounds3 transformSafe(const Transform& transform, const Bounds3& bounds);

	/**
	\brief gets the transformed bounds of the passed AABB (resulting in a bigger AABB).

	Calling this method for empty bounds leads to undefined behavior. Use #transformSafe() instead.

	\param[in] transform Transform to apply, can contain scaling as well
	\param[in] bounds The bounds to transform.
	*/
	static  inline Bounds3 transformFast(const Transform& transform, const Bounds3& bounds);

	/**
	\brief Sets empty to true
	*/
	 inline void setEmpty();

	/**
	\brief Sets the bounds to maximum size [-NV_MAX_BOUNDS_EXTENTS, NV_MAX_BOUNDS_EXTENTS].
	*/
	 inline void setMaximal();

	/**
	\brief expands the volume to include v
	\param v Point to expand to.
	*/
	 inline void include(const Vec3& v);

	/**
	\brief expands the volume to include b.
	\param b Bounds to perform union with.
	*/
	 inline void include(const Bounds3& b);

	 inline bool isEmpty() const;

	/**
	\brief indicates whether the intersection of this and b is empty or not.
	\param b Bounds to test for intersection.
	*/
	 inline bool intersects(const Bounds3& b) const;

	/**
	 \brief computes the 1D-intersection between two AABBs, on a given axis.
	 \param	a		the other AABB
	 \param	axis	the axis (0, 1, 2)
	 */
	 inline bool intersects1D(const Bounds3& a, uint32_t axis) const;

	/**
	\brief indicates if these bounds contain v.
	\param v Point to test against bounds.
	*/
	 inline bool contains(const Vec3& v) const;

	/**
	 \brief	checks a box is inside another box.
	 \param	box		the other AABB
	 */
	 inline bool isInside(const Bounds3& box) const;

	/**
	\brief returns the center of this axis aligned box.
	*/
	 inline Vec3 getCenter() const;

	/**
	\brief get component of the box's center along a given axis
	*/
	 inline float getCenter(uint32_t axis) const;

	/**
	\brief get component of the box's extents along a given axis
	*/
	 inline float getExtents(uint32_t axis) const;

	/**
	\brief returns the dimensions (width/height/depth) of this axis aligned box.
	*/
	 inline Vec3 getDimensions() const;

	/**
	\brief returns the extents, which are half of the width/height/depth.
	*/
	 inline Vec3 getExtents() const;

	/**
	\brief scales the AABB.

	This version is safe to call for empty bounds.

	\param scale Factor to scale AABB by.
	*/
	 inline void scaleSafe(float scale);

	/**
	\brief scales the AABB.

	Calling this method for empty bounds leads to undefined behavior. Use #scaleSafe() instead.

	\param scale Factor to scale AABB by.
	*/
	 inline void scaleFast(float scale);

	/**
	fattens the AABB in all 3 dimensions by the given distance.

	This version is safe to call for empty bounds.
	*/
	 inline void fattenSafe(float distance);

	/**
	fattens the AABB in all 3 dimensions by the given distance.

	Calling this method for empty bounds leads to undefined behavior. Use #fattenSafe() instead.
	*/
	 inline void fattenFast(float distance);

	/**
	checks that the AABB values are not NaN
	*/
	 inline bool isFinite() const;

	/**
	checks that the AABB values describe a valid configuration.
	*/
	 inline bool isValid() const;

	Vec3 minimum, maximum;
};

 inline Bounds3::Bounds3(const Vec3& minimum_, const Vec3& maximum_)
: minimum(minimum_), maximum(maximum_)
{
}

 inline Bounds3 Bounds3::empty()
{
	return Bounds3(Vec3(NV_MAX_BOUNDS_EXTENTS), Vec3(-NV_MAX_BOUNDS_EXTENTS));
}

 inline bool Bounds3::isFinite() const
{
	return minimum.isFinite() && maximum.isFinite();
}

 inline Bounds3 Bounds3::boundsOfPoints(const Vec3& v0, const Vec3& v1)
{
	return Bounds3(v0.minimum(v1), v0.maximum(v1));
}

 inline Bounds3 Bounds3::centerExtents(const Vec3& center, const Vec3& extent)
{
	return Bounds3(center - extent, center + extent);
}

 inline Bounds3
Bounds3::basisExtent(const Vec3& center, const Mat33& basis, const Vec3& extent)
{
	// extended basis vectors
	Vec3 c0 = basis.column0 * extent.x;
	Vec3 c1 = basis.column1 * extent.y;
	Vec3 c2 = basis.column2 * extent.z;

	Vec3 w;
	// find combination of base vectors that produces max. distance for each component = sum of abs()
	w.x = NvAbs(c0.x) + NvAbs(c1.x) + NvAbs(c2.x);
	w.y = NvAbs(c0.y) + NvAbs(c1.y) + NvAbs(c2.y);
	w.z = NvAbs(c0.z) + NvAbs(c1.z) + NvAbs(c2.z);

	return Bounds3(center - w, center + w);
}

 inline Bounds3 Bounds3::poseExtent(const Transform& pose, const Vec3& extent)
{
	return basisExtent(pose.p, Mat33(pose.q), extent);
}

 inline void Bounds3::setEmpty()
{
	minimum = Vec3(NV_MAX_BOUNDS_EXTENTS);
	maximum = Vec3(-NV_MAX_BOUNDS_EXTENTS);
}

 inline void Bounds3::setMaximal()
{
	minimum = Vec3(-NV_MAX_BOUNDS_EXTENTS);
	maximum = Vec3(NV_MAX_BOUNDS_EXTENTS);
}

 inline void Bounds3::include(const Vec3& v)
{
	minimum = minimum.minimum(v);
	maximum = maximum.maximum(v);
}

 inline void Bounds3::include(const Bounds3& b)
{
	minimum = minimum.minimum(b.minimum);
	maximum = maximum.maximum(b.maximum);
}

 inline bool Bounds3::isEmpty() const
{
	return minimum.x > maximum.x;
}

 inline bool Bounds3::intersects(const Bounds3& b) const
{
	return !(b.minimum.x > maximum.x || minimum.x > b.maximum.x || b.minimum.y > maximum.y || minimum.y > b.maximum.y ||
	         b.minimum.z > maximum.z || minimum.z > b.maximum.z);
}

 inline bool Bounds3::intersects1D(const Bounds3& a, uint32_t axis) const
{
	return maximum[axis] >= a.minimum[axis] && a.maximum[axis] >= minimum[axis];
}

 inline bool Bounds3::contains(const Vec3& v) const
{
	return !(v.x < minimum.x || v.x > maximum.x || v.y < minimum.y || v.y > maximum.y || v.z < minimum.z ||
	         v.z > maximum.z);
}

 inline bool Bounds3::isInside(const Bounds3& box) const
{
	if(box.minimum.x > minimum.x)
		return false;
	if(box.minimum.y > minimum.y)
		return false;
	if(box.minimum.z > minimum.z)
		return false;
	if(box.maximum.x < maximum.x)
		return false;
	if(box.maximum.y < maximum.y)
		return false;
	if(box.maximum.z < maximum.z)
		return false;
	return true;
}

 inline Vec3 Bounds3::getCenter() const
{
	return (minimum + maximum) * 0.5f;
}

 inline float Bounds3::getCenter(uint32_t axis) const
{
	return (minimum[axis] + maximum[axis]) * 0.5f;
}

 inline float Bounds3::getExtents(uint32_t axis) const
{
	return (maximum[axis] - minimum[axis]) * 0.5f;
}

 inline Vec3 Bounds3::getDimensions() const
{
	return maximum - minimum;
}

 inline Vec3 Bounds3::getExtents() const
{
	return getDimensions() * 0.5f;
}

 inline void Bounds3::scaleSafe(float scale)
{
	if(!isEmpty())
		scaleFast(scale);
}

 inline void Bounds3::scaleFast(float scale)
{
	*this = centerExtents(getCenter(), getExtents() * scale);
}

 inline void Bounds3::fattenSafe(float distance)
{
	if(!isEmpty())
		fattenFast(distance);
}

 inline void Bounds3::fattenFast(float distance)
{
	minimum.x -= distance;
	minimum.y -= distance;
	minimum.z -= distance;

	maximum.x += distance;
	maximum.y += distance;
	maximum.z += distance;
}

 inline Bounds3 Bounds3::transformSafe(const Mat33& matrix, const Bounds3& bounds)
{
	return !bounds.isEmpty() ? transformFast(matrix, bounds) : bounds;
}

 inline Bounds3 Bounds3::transformFast(const Mat33& matrix, const Bounds3& bounds)
{
	return Bounds3::basisExtent(matrix * bounds.getCenter(), matrix, bounds.getExtents());
}

 inline Bounds3 Bounds3::transformSafe(const Transform& transform, const Bounds3& bounds)
{
	return !bounds.isEmpty() ? transformFast(transform, bounds) : bounds;
}

 inline Bounds3 Bounds3::transformFast(const Transform& transform, const Bounds3& bounds)
{
	return Bounds3::basisExtent(transform.transform(bounds.getCenter()), Mat33(transform.q), bounds.getExtents());
}

 inline bool Bounds3::isValid() const
{
	return (isFinite() && (((minimum.x <= maximum.x) && (minimum.y <= maximum.y) && (minimum.z <= maximum.z)) ||
	                       ((minimum.x == NV_MAX_BOUNDS_EXTENTS) && (minimum.y == NV_MAX_BOUNDS_EXTENTS) &&
	                        (minimum.z == NV_MAX_BOUNDS_EXTENTS) && (maximum.x == -NV_MAX_BOUNDS_EXTENTS) &&
	                        (maximum.y == -NV_MAX_BOUNDS_EXTENTS) && (maximum.z == -NV_MAX_BOUNDS_EXTENTS))));
}

#if !NV_DOXYGEN
} // namespace NvIsaac
#endif

/** @} */
#endif // #ifndef NV_NVFOUNDATION_Bounds3_H
