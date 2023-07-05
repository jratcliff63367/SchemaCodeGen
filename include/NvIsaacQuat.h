// This header file defines and implements a quaternion class

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

#ifndef NV_ISAAC_MATH_QUAT_H
#define NV_ISAAC_MATH_QUAT_H

/** \addtogroup foundation
@{
*/

#include "NvIsaacVec3.h"

#if !NV_DOXYGEN
namespace NvIsaac
{
#endif

/**
\brief This is a quaternion class. For more information on quaternion mathematics
consult a mathematics source on complex numbers.

*/

class Mat33;

class Quat
{
  public:
	/**
	\brief Default constructor, sets identity.
	*/
	 inline Quat() : x(0.0f), y(0.0f), z(0.0f), w(1.0f)
	{
	}

	//! identity constructor
	 inline Quat(NvIDENTITY r) : x(0.0f), y(0.0f), z(0.0f), w(1.0f)
	{
		NV_UNUSED(r);
	}

	/**
	\brief Constructor from a scalar: sets the real part w to the scalar value, and the imaginary parts (x,y,z) to zero
	*/
	explicit  inline Quat(float r) : x(0.0f), y(0.0f), z(0.0f), w(r)
	{
	}

	/**
	\brief Constructor.  Take note of the order of the elements!
	*/
	 inline Quat(float nx, float ny, float nz, float nw) : x(nx), y(ny), z(nz), w(nw)
	{
	}

	/**
	\brief Creates from angle-axis representation.

	Axis must be normalized!

	Angle is in radians!

	<b>Unit:</b> Radians
	*/
	 inline Quat(float angleRadians, const Vec3& unitAxis)
	{
		const float a = angleRadians * 0.5f;
		const float s = NvSin(a);
		w = NvCos(a);
		x = unitAxis.x * s;
		y = unitAxis.y * s;
		z = unitAxis.z * s;
	}

	/**
	\brief Copy ctor.
	*/
	 inline Quat(const Quat& v) : x(v.x), y(v.y), z(v.z), w(v.w)
	{
	}

	/**
	\brief Creates from orientation matrix.

	\param[in] m Rotation matrix to extract quaternion from.
	*/
	 inline explicit Quat(const Mat33& m); /* defined in Mat33.h */

	/**
	\brief returns true if all elements are finite (not NAN or INF, etc.)
	*/
	 bool isFinite() const
	{
		return NvIsFinite(x) && NvIsFinite(y) && NvIsFinite(z) && NvIsFinite(w);
	}

	/**
	\brief returns true if finite and magnitude is close to unit
	*/

	 bool isUnit() const
	{
		const float unitTolerance = 1e-4f;
		return isFinite() && NvAbs(magnitude() - 1) < unitTolerance;
	}

	/**
	\brief returns true if finite and magnitude is reasonably close to unit to allow for some accumulation of error vs
	isValid
	*/

	 bool isSane() const
	{
		const float unitTolerance = 1e-2f;
		return isFinite() && NvAbs(magnitude() - 1) < unitTolerance;
	}

	/**
	\brief returns true if the two quaternions are exactly equal
	*/
	 inline bool operator==(const Quat& q) const
	{
		return x == q.x && y == q.y && z == q.z && w == q.w;
	}

	/**
	\brief converts this quaternion to angle-axis representation
	*/

	 inline void toRadiansAndUnitAxis(float& angle, Vec3& axis) const
	{
		const float quatEpsilon = 1.0e-8f;
		const float s2 = x * x + y * y + z * z;
		if(s2 < quatEpsilon * quatEpsilon) // can't extract a sensible axis
		{
			angle = 0.0f;
			axis = Vec3(1.0f, 0.0f, 0.0f);
		}
		else
		{
			const float s = NvRecipSqrt(s2);
			axis = Vec3(x, y, z) * s;
			angle = NvAbs(w) < quatEpsilon ? NvPi : NvAtan2(s2 * s, w) * 2.0f;
		}
	}

	/**
	\brief Gets the angle between this quat and the identity quaternion.

	<b>Unit:</b> Radians
	*/
	 inline float getAngle() const
	{
		return NvAcos(w) * 2.0f;
	}

	/**
	\brief Gets the angle between this quat and the argument

	<b>Unit:</b> Radians
	*/
	 inline float getAngle(const Quat& q) const
	{
		return NvAcos(dot(q)) * 2.0f;
	}

	/**
	\brief This is the squared 4D vector length, should be 1 for unit quaternions.
	*/
	 inline float magnitudeSquared() const
	{
		return x * x + y * y + z * z + w * w;
	}

	/**
	\brief returns the scalar product of this and other.
	*/
	 inline float dot(const Quat& v) const
	{
		return x * v.x + y * v.y + z * v.z + w * v.w;
	}

	 inline Quat getNormalized() const
	{
		const float s = 1.0f / magnitude();
		return Quat(x * s, y * s, z * s, w * s);
	}

	 inline float magnitude() const
	{
		return NvSqrt(magnitudeSquared());
	}

	// modifiers:
	/**
	\brief maps to the closest unit quaternion.
	*/
	 inline float normalize() // convert this Quat to a unit quaternion
	{
		const float mag = magnitude();
		if(mag != 0.0f)
		{
			const float imag = 1.0f / mag;

			x *= imag;
			y *= imag;
			z *= imag;
			w *= imag;
		}
		return mag;
	}

	/*
	\brief returns the conjugate.

	\note for unit quaternions, this is the inverse.
	*/
	 inline Quat getConjugate() const
	{
		return Quat(-x, -y, -z, w);
	}

	/*
	\brief returns imaginary part.
	*/
	 inline Vec3 getImaginaryPart() const
	{
		return Vec3(x, y, z);
	}

	/** brief computes rotation of x-axis */
	 inline Vec3 getBasisVector0() const
	{
		const float x2 = x * 2.0f;
		const float w2 = w * 2.0f;
		return Vec3((w * w2) - 1.0f + x * x2, (z * w2) + y * x2, (-y * w2) + z * x2);
	}

	/** brief computes rotation of y-axis */
	 inline Vec3 getBasisVector1() const
	{
		const float y2 = y * 2.0f;
		const float w2 = w * 2.0f;
		return Vec3((-z * w2) + x * y2, (w * w2) - 1.0f + y * y2, (x * w2) + z * y2);
	}

	/** brief computes rotation of z-axis */
	 inline Vec3 getBasisVector2() const
	{
		const float z2 = z * 2.0f;
		const float w2 = w * 2.0f;
		return Vec3((y * w2) + x * z2, (-x * w2) + y * z2, (w * w2) - 1.0f + z * z2);
	}

	/**
	rotates passed vec by this (assumed unitary)
	*/
	 inline const Vec3 rotate(const Vec3& v) const
	{
		const float vx = 2.0f * v.x;
		const float vy = 2.0f * v.y;
		const float vz = 2.0f * v.z;
		const float w2 = w * w - 0.5f;
		const float dot2 = (x * vx + y * vy + z * vz);
		return Vec3((vx * w2 + (y * vz - z * vy) * w + x * dot2), (vy * w2 + (z * vx - x * vz) * w + y * dot2),
		              (vz * w2 + (x * vy - y * vx) * w + z * dot2));
	}

	/**
	inverse rotates passed vec by this (assumed unitary)
	*/
	 inline const Vec3 rotateInv(const Vec3& v) const
	{
		const float vx = 2.0f * v.x;
		const float vy = 2.0f * v.y;
		const float vz = 2.0f * v.z;
		const float w2 = w * w - 0.5f;
		const float dot2 = (x * vx + y * vy + z * vz);
		return Vec3((vx * w2 - (y * vz - z * vy) * w + x * dot2), (vy * w2 - (z * vx - x * vz) * w + y * dot2),
		              (vz * w2 - (x * vy - y * vx) * w + z * dot2));
	}

	/**
	\brief Assignment operator
	*/
	 inline Quat& operator=(const Quat& p)
	{
		x = p.x;
		y = p.y;
		z = p.z;
		w = p.w;
		return *this;
	}

	 inline Quat& operator*=(const Quat& q)
	{
		const float tx = w * q.x + q.w * x + y * q.z - q.y * z;
		const float ty = w * q.y + q.w * y + z * q.x - q.z * x;
		const float tz = w * q.z + q.w * z + x * q.y - q.x * y;

		w = w * q.w - q.x * x - y * q.y - q.z * z;
		x = tx;
		y = ty;
		z = tz;

		return *this;
	}

	 inline Quat& operator+=(const Quat& q)
	{
		x += q.x;
		y += q.y;
		z += q.z;
		w += q.w;
		return *this;
	}

	 inline Quat& operator-=(const Quat& q)
	{
		x -= q.x;
		y -= q.y;
		z -= q.z;
		w -= q.w;
		return *this;
	}

	 inline Quat& operator*=(const float s)
	{
		x *= s;
		y *= s;
		z *= s;
		w *= s;
		return *this;
	}

	/** quaternion multiplication */
	 inline Quat operator*(const Quat& q) const
	{
		return Quat(w * q.x + q.w * x + y * q.z - q.y * z, w * q.y + q.w * y + z * q.x - q.z * x,
		              w * q.z + q.w * z + x * q.y - q.x * y, w * q.w - x * q.x - y * q.y - z * q.z);
	}

	/** quaternion addition */
	 inline Quat operator+(const Quat& q) const
	{
		return Quat(x + q.x, y + q.y, z + q.z, w + q.w);
	}

	/** quaternion subtraction */
	 inline Quat operator-() const
	{
		return Quat(-x, -y, -z, -w);
	}

	 inline Quat operator-(const Quat& q) const
	{
		return Quat(x - q.x, y - q.y, z - q.z, w - q.w);
	}

	 inline Quat operator*(float r) const
	{
		return Quat(x * r, y * r, z * r, w * r);
	}

	 inline void fromAxisAngle(const Vec3 &axis, float angle)
	 {
		 Vec3 v = axis;	// Copy the axis
		 v.normalize();		// Make sure it is normalized
		 float half = angle*0.5f;
		 float qw = cosf(half);
		 float sin_theta_over_two = sinf(half);
		 v *= sin_theta_over_two;
		 x = v.x;
		 y = v.y;
		 z = v.z;
		 w = qw;
	 }

	 // Converts Roll/Pitch/Yaw (radians) into a quaternion
	 inline void fromRollPitchYaw(float roll, float pitch, float yaw)
	 {
		 // Abbreviations for the various angular functions
		 float cy = NvCos(yaw * 0.5f);
		 float sy = NvSin(yaw * 0.5f);
		 float cr = NvCos(roll * 0.5f);
		 float sr = NvSin(roll * 0.5f);
		 float cp = NvCos(pitch * 0.5f);
		 float sp = NvSin(pitch * 0.5f);
		 w = (float)(cy * cr * cp + sy * sr * sp);
		 x = (float)(cy * sr * cp - sy * cr * sp);
		 y = (float)(cy * cr * sp + sy * sr * cp);
		 z = (float)(sy * cr * cp - cy * sr * sp);
	 }

	/** the quaternion elements */
	float x, y, z, w;
};

#if !NV_DOXYGEN
} // namespace NvIsaac
#endif

/** @} */
#endif // #ifndef NV_NVFOUNDATION_Quat_H
