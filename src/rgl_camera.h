//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Camera defs)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2004-2005  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------

#ifndef __RGL_CAMERA__
#define __RGL_CAMERA__

#include "epi/math_angle.h"
#include "epi/math_bbox.h"
#include "epi/math_vector.h"

class camera_c
{
private:
	epi::angle_c FOV;  // field of view

	epi::vec3_c pos;   // position
	epi::vec3_c dir;   // face direction (a unit vector)

	epi::angle_c turn;
	epi::angle_c mlook;

	float horiz_slope;
	float vert_slope;

	// vectors for the planes that make up the view frustum.  The vectors
	// face towards the inside of the frustum.  The near plane vector is
	// already known (same as camera direction).

	epi::vec3_c left_v;
	epi::vec3_c right_v;
	epi::vec3_c top_v;
	epi::vec3_c bottom_v;

#if 0
	// equivalent for the 2D plane.  Both are equal to zero when this
	// isn't possible (camera looking too far up/down)
	epi::angle_c left_2D;
	epi::angle_c right_2D;
#endif

	// information for Sphere-Cone intersection test
	float cone_cos;
	float cone_tan;

	void FindAngles(epi::vec3_c _dir);
	void Recalculate();
	void AdjustPlane(epi::vec3_c *v);

public:
	camera_c();
	camera_c(const camera_c& other);
	~camera_c();

	void SetFOV(epi::angle_c _FOV);

	void SetPos(epi::vec3_c _pos);
	void SetDir(epi::vec3_c _dir);
	void SetDir(epi::angle_c _turn, epi::angle_c _mlook);

	// combined versions (avoids recalculating internate state)
	void SetPosDir(epi::vec3_c _pos, epi::vec3_c _dir);
	void SetPosDir(epi::vec3_c _pos, epi::angle_c _turn, epi::angle_c _mlook);

	void LoadGLMatrices(bool translate = true) const;
	// set the OpenGL projection and model matrices to match this camera.

	enum
	{
		HIT_OUTSIDE = 0,  // fully outside the cone/frustum
		HIT_PARTIAL,      // intersects the edge of the cone/frustum
		HIT_INSIDE,       // fully inside the cone/frustum
	};

	int TestSphere(epi::vec3_c mid, float R) const;
	// perform two tests: firstly that the sphere is behind the near
	// plane -- return HIT_OUTSIDE if true.  Then test the sphere
	// against the view cone, and return a HIT_XXX value.
	// HIT_PARTIAL is returned when the sphere surrounds the camera.

	int TestBBox(const epi::bbox3_c *bb) const;
	// perform a bounding box test against the sides of the view
	// frustum, returning one of the HIT_XXX values.  This test is
	// a lot slower than TestSphere(), which should be used first.
	// HIT_PARTIAL is returned when the bbox surrounds the camera.
};

//----------------------------------------------------------------------------
//  IMPLEMENTATION
//----------------------------------------------------------------------------

inline void camera_c::SetFOV(epi::angle_c _FOV)
{
	FOV = _FOV; Recalculate();
}

inline void camera_c::SetPos(epi::vec3_c _pos)
{
	pos = _pos; Recalculate();
}

inline void camera_c::SetDir(epi::vec3_c _dir)
{
	FindAngles(_dir); Recalculate();
}

inline void camera_c::SetDir(epi::angle_c _turn, epi::angle_c _mlook)
{
	turn = _turn; mlook = _mlook;
	Recalculate();
}

inline void camera_c::SetPosDir(epi::vec3_c _pos, epi::vec3_c _dir)
{
	pos = _pos;
	FindAngles(_dir); Recalculate();
}

inline void camera_c::SetPosDir(epi::vec3_c _pos, epi::angle_c _turn, epi::angle_c _mlook)
{
	pos = _pos; turn = _turn; mlook = _mlook;
	Recalculate();
}

#endif  /* __RGL_CAMERA__ */
