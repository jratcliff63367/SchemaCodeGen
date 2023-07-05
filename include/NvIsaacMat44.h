// This header file declares and implements a 4x4 matrix class

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

#ifndef NV_ISAAC_MATH_MAT44_H
#define NV_ISAAC_MATH_MAT44_H
/** \addtogroup foundation
@{
*/

#include "NvIsaacQuat.h"
#include "NvIsaacVec4.h"
#include "NvIsaacMat33.h"
#include "NvIsaacTransform.h"

#if !NV_DOXYGEN
namespace NvIsaac
{
#endif

/*!
\brief 4x4 matrix class

This class is layout-compatible with D3D and OpenGL matrices. More notes on layout are given in the Mat33

@see Mat33 Transform
*/

class Mat44
{
  public:
	//! Default constructor
	 inline Mat44()
	: column0(1.0f, 0.0f, 0.0f, 0.0f)
	, column1(0.0f, 1.0f, 0.0f, 0.0f)
	, column2(0.0f, 0.0f, 1.0f, 0.0f)
	, column3(0.0f, 0.0f, 0.0f, 1.0f)
	{
	}

	//! identity constructor
	 inline Mat44(NvIDENTITY r)
	: column0(1.0f, 0.0f, 0.0f, 0.0f)
	, column1(0.0f, 1.0f, 0.0f, 0.0f)
	, column2(0.0f, 0.0f, 1.0f, 0.0f)
	, column3(0.0f, 0.0f, 0.0f, 1.0f)
	{
		NV_UNUSED(r);
	}

	//! zero constructor
	 inline Mat44(NvZERO r) : column0(NvZero), column1(NvZero), column2(NvZero), column3(NvZero)
	{
		NV_UNUSED(r);
	}

	//! Construct from four 4-vectors
	 Mat44(const Vec4& col0, const Vec4& col1, const Vec4& col2, const Vec4& col3)
	: column0(col0), column1(col1), column2(col2), column3(col3)
	{
	}

	//! constructor that generates a multiple of the identity matrix
	explicit  inline Mat44(float r)
	: column0(r, 0.0f, 0.0f, 0.0f)
	, column1(0.0f, r, 0.0f, 0.0f)
	, column2(0.0f, 0.0f, r, 0.0f)
	, column3(0.0f, 0.0f, 0.0f, r)
	{
	}

	//! Construct from three base vectors and a translation
	 Mat44(const Vec3& col0, const Vec3& col1, const Vec3& col2, const Vec3& col3)
	: column0(col0, 0), column1(col1, 0), column2(col2, 0), column3(col3, 1.0f)
	{
	}

	//! Construct from float[16]
	explicit  inline Mat44(float values[])
	: column0(values[0], values[1], values[2], values[3])
	, column1(values[4], values[5], values[6], values[7])
	, column2(values[8], values[9], values[10], values[11])
	, column3(values[12], values[13], values[14], values[15])
	{
	}

	//! Construct from a quaternion
	explicit  inline Mat44(const Quat& q)
	{
		const float x = q.x;
		const float y = q.y;
		const float z = q.z;
		const float w = q.w;

		const float x2 = x + x;
		const float y2 = y + y;
		const float z2 = z + z;

		const float xx = x2 * x;
		const float yy = y2 * y;
		const float zz = z2 * z;

		const float xy = x2 * y;
		const float xz = x2 * z;
		const float xw = x2 * w;

		const float yz = y2 * z;
		const float yw = y2 * w;
		const float zw = z2 * w;

		column0 = Vec4(1.0f - yy - zz, xy + zw, xz - yw, 0.0f);
		column1 = Vec4(xy - zw, 1.0f - xx - zz, yz + xw, 0.0f);
		column2 = Vec4(xz + yw, yz - xw, 1.0f - xx - yy, 0.0f);
		column3 = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
	}

	//! Construct from a diagonal vector
	explicit  inline Mat44(const Vec4& diagonal)
	: column0(diagonal.x, 0.0f, 0.0f, 0.0f)
	, column1(0.0f, diagonal.y, 0.0f, 0.0f)
	, column2(0.0f, 0.0f, diagonal.z, 0.0f)
	, column3(0.0f, 0.0f, 0.0f, diagonal.w)
	{
	}

	//! Construct from Mat33 and a translation
	 Mat44(const Mat33& axes, const Vec3& position)
	: column0(axes.column0, 0.0f), column1(axes.column1, 0.0f), column2(axes.column2, 0.0f), column3(position, 1.0f)
	{
	}

	 Mat44(const Transform& t)
	{
		*this = Mat44(Mat33(t.q), t.p);
	}

	/**
	\brief returns true if the two matrices are exactly equal
	*/
	 inline bool operator==(const Mat44& m) const
	{
		return column0 == m.column0 && column1 == m.column1 && column2 == m.column2 && column3 == m.column3;
	}

	//! Copy constructor
	 inline Mat44(const Mat44& other)
	: column0(other.column0), column1(other.column1), column2(other.column2), column3(other.column3)
	{
	}

	//! Assignment operator
	 inline const Mat44& operator=(const Mat44& other)
	{
		column0 = other.column0;
		column1 = other.column1;
		column2 = other.column2;
		column3 = other.column3;
		return *this;
	}

	//! Get transposed matrix
	 inline Mat44 getTranspose() const
	{
		return Mat44(
		    Vec4(column0.x, column1.x, column2.x, column3.x), Vec4(column0.y, column1.y, column2.y, column3.y),
		    Vec4(column0.z, column1.z, column2.z, column3.z), Vec4(column0.w, column1.w, column2.w, column3.w));
	}

	//! Unary minus
	 inline Mat44 operator-() const
	{
		return Mat44(-column0, -column1, -column2, -column3);
	}

	//! Add
	 inline Mat44 operator+(const Mat44& other) const
	{
		return Mat44(column0 + other.column0, column1 + other.column1, column2 + other.column2,
		               column3 + other.column3);
	}

	//! Subtract
	 inline Mat44 operator-(const Mat44& other) const
	{
		return Mat44(column0 - other.column0, column1 - other.column1, column2 - other.column2,
		               column3 - other.column3);
	}

	//! Scalar multiplication
	 inline Mat44 operator*(float scalar) const
	{
		return Mat44(column0 * scalar, column1 * scalar, column2 * scalar, column3 * scalar);
	}

	friend Mat44 operator*(float, const Mat44&);

	//! Matrix multiplication
	 inline Mat44 operator*(const Mat44& other) const
	{
		// Rows from this <dot> columns from other
		// column0 = transform(other.column0) etc
		return Mat44(transform(other.column0), transform(other.column1), transform(other.column2),
		               transform(other.column3));
	}

	// a <op>= b operators

	//! Equals-add
	 inline Mat44& operator+=(const Mat44& other)
	{
		column0 += other.column0;
		column1 += other.column1;
		column2 += other.column2;
		column3 += other.column3;
		return *this;
	}

	//! Equals-sub
	 inline Mat44& operator-=(const Mat44& other)
	{
		column0 -= other.column0;
		column1 -= other.column1;
		column2 -= other.column2;
		column3 -= other.column3;
		return *this;
	}

	//! Equals scalar multiplication
	 inline Mat44& operator*=(float scalar)
	{
		column0 *= scalar;
		column1 *= scalar;
		column2 *= scalar;
		column3 *= scalar;
		return *this;
	}

	//! Equals matrix multiplication
	 inline Mat44& operator*=(const Mat44& other)
	{
		*this = *this * other;
		return *this;
	}

	//! Transform vector by matrix, equal to v' = M*v
	 inline Vec4 transform(const Vec4& other) const
	{
		return column0 * other.x + column1 * other.y + column2 * other.z + column3 * other.w;
	}

	//! Transform vector by matrix, equal to v' = M*v
	 inline Vec3 transform(const Vec3& other) const
	{
		return transform(Vec4(other, 1.0f)).getXYZ();
	}

	//! Rotate vector by matrix, equal to v' = M*v
	 inline const Vec4 rotate(const Vec4& other) const
	{
		return column0 * other.x + column1 * other.y + column2 * other.z; // + column3*0;
	}

	//! Rotate vector by matrix, equal to v' = M*v
	 inline const Vec3 rotate(const Vec3& other) const
	{
		return rotate(Vec4(other, 1.0f)).getXYZ();
	}

	 inline Vec3 getBasis(int num) const
	{
		return (&column0)[num].getXYZ();
	}

	 inline Vec3 getPosition() const
	{
		return column3.getXYZ();
	}

	 inline void setPosition(const Vec3& position)
	{
		column3.x = position.x;
		column3.y = position.y;
		column3.z = position.z;
	}

	 inline const float* front() const
	{
		return &column0.x;
	}

	 inline void scale(const Vec4& p)
	{
		column0 *= p.x;
		column1 *= p.y;
		column2 *= p.z;
		column3 *= p.w;
	}

	 inline Mat44 inverseRT(void) const
	{
		Vec3 r0(column0.x, column1.x, column2.x), r1(column0.y, column1.y, column2.y),
		    r2(column0.z, column1.z, column2.z);

		return Mat44(r0, r1, r2, -(r0 * column3.x + r1 * column3.y + r2 * column3.z));
	}

	 inline bool isFinite() const
	{
		return column0.isFinite() && column1.isFinite() && column2.isFinite() && column3.isFinite();
	}

	// Data, see above for format!

	Vec4 column0, column1, column2, column3; // the four base vectors
};

// implementation from Transform.h
 inline Transform::Transform(const Mat44& m)
{
	Vec3 column0 = Vec3(m.column0.x, m.column0.y, m.column0.z);
	Vec3 column1 = Vec3(m.column1.x, m.column1.y, m.column1.z);
	Vec3 column2 = Vec3(m.column2.x, m.column2.y, m.column2.z);

	q = Quat(Mat33(column0, column1, column2));
	p = Vec3(m.column3.x, m.column3.y, m.column3.z);
}

#if !NV_DOXYGEN
} // namespace NvIsaac
#endif

/** @} */
#endif // #ifndef NV_NVFOUNDATION_Mat44_H
