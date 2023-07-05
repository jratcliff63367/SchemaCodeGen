// This header file defines and implements a transform class which
// can represent a position and rotation (as a quaternion) but not scale

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

#ifndef NV_ISAAC_MATH_TRANSFORM_H
#define NV_ISAAC_MATH_TRANSFORM_H
/** \addtogroup foundation
  @{
*/

#include "NvIsaacQuat.h"
#include "NvIsaacPlane.h"

#if !NV_DOXYGEN
namespace NvIsaac
{
#endif

/*!
\brief class representing a rigid euclidean transform as a quaternion and a vector
*/

class Mat44;

class Transform
{
  public:
	Quat q;
	Vec3 p;

	 inline Transform()
	{
	}

	 inline explicit Transform(const Vec3& position) : q(NvIdentity), p(position)
	{
	}

	 inline explicit Transform(NvIDENTITY r) : q(NvIdentity), p(NvZero)
	{
		NV_UNUSED(r);
	}

	 inline explicit Transform(const Quat& orientation) : q(orientation), p(0)
	{
	}

	 inline Transform(float x, float y, float z, Quat aQ = Quat(NvIdentity))
	: q(aQ), p(x, y, z)
	{
	}

	 inline Transform(const Vec3& p0, const Quat& q0) : q(q0), p(p0)
	{
	}

	 inline explicit Transform(const Mat44& m); // defined in Mat44.h

	/**
	\brief returns true if the two transforms are exactly equal
	*/
	 inline bool operator==(const Transform& t) const
	{
		return p == t.p && q == t.q;
	}

	 inline Transform operator*(const Transform& x) const
	{
		return transform(x);
	}

	//! Equals matrix multiplication
	 inline Transform& operator*=(Transform& other)
	{
		*this = *this * other;
		return *this;
	}

	 inline Transform getInverse() const
	{
		return Transform(q.rotateInv(-p), q.getConjugate());
	}

	 inline Vec3 transform(const Vec3& input) const
	{
		return q.rotate(input) + p;
	}

	 inline Vec3 transformInv(const Vec3& input) const
	{
		return q.rotateInv(input - p);
	}

	 inline Vec3 rotate(const Vec3& input) const
	{
		return q.rotate(input);
	}

	 inline Vec3 rotateInv(const Vec3& input) const
	{
		return q.rotateInv(input);
	}

	//! Transform transform to parent (returns compound transform: first src, then *this)
	 inline Transform transform(const Transform& src) const
	{
		// src = [srct, srcr] -> [r*srct + t, r*srcr]
		return Transform(q.rotate(src.p) + p, q * src.q);
	}

	/**
	\brief returns true if finite and q is a unit quaternion
	*/

	 bool isValid() const
	{
		return p.isFinite() && q.isFinite() && q.isUnit();
	}

	/**
	\brief returns true if finite and quat magnitude is reasonably close to unit to allow for some accumulation of error
	vs isValid
	*/

	 bool isSane() const
	{
		return isFinite() && q.isSane();
	}

	/**
	\brief returns true if all elems are finite (not NAN or INF, etc.)
	*/
	 inline bool isFinite() const
	{
		return p.isFinite() && q.isFinite();
	}

	//! Transform transform from parent (returns compound transform: first src, then this->inverse)
	 inline Transform transformInv(const Transform& src) const
	{
		// src = [srct, srcr] -> [r^-1*(srct-t), r^-1*srcr]
		Quat qinv = q.getConjugate();
		return Transform(qinv.rotate(src.p - p), qinv * src.q);
	}

	/**
	\brief transform plane
	*/

	 inline Plane transform(const Plane& plane) const
	{
		Vec3 transformedNormal = rotate(plane.n);
		return Plane(transformedNormal, plane.d - p.dot(transformedNormal));
	}

	/**
	\brief inverse-transform plane
	*/

	 inline Plane inverseTransform(const Plane& plane) const
	{
		Vec3 transformedNormal = rotateInv(plane.n);
		return Plane(transformedNormal, plane.d + p.dot(plane.n));
	}

	/**
	\brief return a normalized transform (i.e. one in which the quaternion has unit magnitude)
	*/
	 inline Transform getNormalized() const
	{
		return Transform(p, q.getNormalized());
	}
};

#if !NV_DOXYGEN
} // namespace NvIsaac
#endif

/** @} */
#endif // #ifndef NV_NVFOUNDATION_Transform_H
