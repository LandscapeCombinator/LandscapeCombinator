#pragma once
#include <string>
#include "Vector2d.h"
#include "LineLinear2d.h"

/// <summary>
///     Geometry line in linear form. General form:
///     Ax + By + C = 0;
///     <see href="http://en.wikipedia.org/wiki/Linear_equation"/>
/// </summary>
class LineLinear2d
{
public:
	double A;
	double B;
	double C;
	LineLinear2d();
	LineLinear2d(const LineLinear2d& other);
	/// <summary> Linear line from two points on line. </summary>
	LineLinear2d(Vector2d pP1, Vector2d pP2);
	/// <summary> Linear line. </summary>
	LineLinear2d(double pA, double pB, double pC);
	/// <summary> Collision point of two lines. </summary>
	static Vector2d Collide(double A1, double B1, double C1, double A2, double B2, double C2);
	/// <summary> Collision point of two lines. </summary>
	static Vector2d Collide(LineLinear2d pLine1, LineLinear2d pLine2);
	/// <summary> Collision point of two lines. </summary>
	/// <param name="pLine">Line to collision.</param>
	/// <returns>Collision point.</returns>
	Vector2d Collide(LineLinear2d pLine);	
	/// <summary> Check whether point belongs to line. </summary>
	bool Contains(Vector2d point);
	std::string ToString() const;
};

