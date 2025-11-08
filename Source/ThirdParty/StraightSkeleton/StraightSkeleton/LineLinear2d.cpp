#include "LineLinear2d.h"
#include <limits>
#include <float.h>

LineLinear2d::LineLinear2d()
{
	A = std::numeric_limits<double>::quiet_NaN();
	B = std::numeric_limits<double>::quiet_NaN();
	C = std::numeric_limits<double>::quiet_NaN();
}

LineLinear2d::LineLinear2d(const LineLinear2d& other)
{
	A = other.A;
	B = other.B;
	C = other.C;
}

LineLinear2d::LineLinear2d(Vector2d pP1, Vector2d pP2)
{
	A = pP1.Y - pP2.Y;
	B = pP2.X - pP1.X;
	C = (pP1.X * pP2.Y) - (pP2.X * pP1.Y);
}

LineLinear2d::LineLinear2d(double pA, double pB, double pC)
{
	A = pA;
	B = pB;
	C = pC;
}

Vector2d LineLinear2d::Collide(double A1, double B1, double C1, double A2, double B2, double C2)
{
	double WAB = A1 * B2 - A2 * B1;
	double WBC = B1 * C2 - B2 * C1;
	double WCA = C1 * A2 - C2 * A1;
	return WAB == 0 ? Vector2d::Empty() : Vector2d(WBC / WAB, WCA / WAB);
}

Vector2d LineLinear2d::Collide(LineLinear2d pLine1, LineLinear2d pLine2)
{
	return Collide(pLine1.A, pLine1.B, pLine1.C, pLine2.A, pLine2.B, pLine2.C);
}

Vector2d LineLinear2d::Collide(LineLinear2d pLine)
{
	return Collide(*this, pLine);
}

bool LineLinear2d::Contains(Vector2d point)
{
	return fabs((point.X * A + point.Y * B + C)) < DBL_EPSILON;
	// return fabs((point.X * A + point.Y * B + C)) < 1;
}

std::string LineLinear2d::ToString() const
{
	return std::string("Line [A=" + std::to_string(A) + " , B = " + std::to_string(B) + " , C = " + std::to_string(C));
}
