#pragma once

#include "math.h"
#include "types.h"

inline float
Sin(float Angle)
{
    float Result = sinf(Angle);
    return(Result);
}

inline float
Cos(float Angle)
{
    float Result = cosf(Angle);
    return(Result);
}

struct mat4x4
{
	float m[4][4] = { 0 };
};

struct Pair
{
	int x, y;
};

struct Vector3
{
	float x, y, z;
};

struct Triangle
{
	Vector3 points[3];
};


inline mat4x4 MakeIdentity()
{
	mat4x4 m = {};
	m.m[0][0] = 1;
	m.m[1][1] = 1;
	m.m[2][2] = 1;
	m.m[3][3] = 1;
	return m;
}

inline mat4x4 MakeRotationZ(float angle)
{
	mat4x4 m = MakeIdentity();
	float c = Cos(angle);
	float s = Sin(angle);

	m.m[0][0] = c;
	m.m[0][1] = s;
	m.m[1][0] = -s;
	m.m[1][1] = c;
	return m;
}

inline mat4x4 MakeRotationX(float angle)
{
	mat4x4 m = MakeIdentity();
	float c = Cos(angle);
	float s = Sin(angle);

	m.m[1][1] = c;
	m.m[1][2] = s;
	m.m[2][1] = -s;
	m.m[2][2] = c;
	return m;
}

inline mat4x4 Multiply(const mat4x4& a, const mat4x4& b)
{
	mat4x4 r = {};

	for (int c = 0; c < 4; c++)
		for (int r2 = 0; r2 < 4; r2++)
			r.m[r2][c] =
			a.m[r2][0] * b.m[0][c] +
			a.m[r2][1] * b.m[1][c] +
			a.m[r2][2] * b.m[2][c] +
			a.m[r2][3] * b.m[3][c];

	return r;
}

internal inline void MultiplyMatrixVector(Vector3& input, Vector3& output, mat4x4& m)
{
	output.x = input.x * m.m[0][0] + input.y * m.m[1][0] + input.z * m.m[2][0] + m.m[3][0];
	output.y = input.x * m.m[0][1] + input.y * m.m[1][1] + input.z * m.m[2][1] + m.m[3][1];
	output.z = input.x * m.m[0][2] + input.y * m.m[1][2] + input.z * m.m[2][2] + m.m[3][2];
	float w = input.x * m.m[0][3] + input.y * m.m[1][3] + input.z * m.m[2][3] + m.m[3][3];

	if (w != 0)
	{
		output.x /= w;
		output.y /= w;
		output.z /= w;
	}
}
