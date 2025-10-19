#include "LineParametric2d.h"
#include "PrimitiveUtils.h"
#include <iostream>
#include <cmath>

LineParametric2d LineParametric2d::Empty()
{
	return LineParametric2d();
}

LineParametric2d::LineParametric2d()
{
	A = nullptr;
	U = nullptr;
}

LineParametric2d::LineParametric2d(spv2d pA, spv2d pU)
{
	A = pA;
	U = pU;
}

LineParametric2d::LineParametric2d(Vector2d pA, Vector2d pU)
{
	A = std::make_shared<Vector2d>(Vector2d(pA));
	U = std::make_shared<Vector2d>(Vector2d(pU));
}

LineParametric2d::~LineParametric2d()
{
	A = nullptr;
	U = nullptr;
}

LineLinear2d LineParametric2d::CreateLinearForm()
{
	double x = A->X;
	double y = A->Y;

	double b = -U->X;
	double a = U->Y;

	double c = -(a * x + b * y);
	return LineLinear2d(a, b, c);
}

Vector2d LineParametric2d::Collide(LineParametric2d ray, LineLinear2d line, double epsilon)
{
	Vector2d collide = LineLinear2d::Collide(ray.CreateLinearForm(), line);
	if (collide.IsEmpty())
		return Vector2d::Empty();

	Vector2d collideVector = collide - *ray.A;
	return ray.U->Dot(collideVector) < epsilon ? Vector2d::Empty() : collide;
}

bool LineParametric2d::IsOnLeftSite(Vector2d point, double epsilon)
{
	Vector2d direction = point - *A;
	return PrimitiveUtils::OrthogonalRight(*U).Dot(direction) < epsilon;
}

bool LineParametric2d::IsOnRightSite(Vector2d point, double epsilon)
{
	Vector2d direction = point - *A;
	return PrimitiveUtils::OrthogonalRight(*U).Dot(direction) > -epsilon;
}

std::string LineParametric2d::ToString() const
{
	return std::string("Line [A=" + A->ToString() + " , U = " + U->ToString());
}
