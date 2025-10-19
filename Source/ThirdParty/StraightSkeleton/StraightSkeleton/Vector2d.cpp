#include "Vector2d.h"

Vector2d::Vector2d()
{
	X = std::numeric_limits<double>::quiet_NaN();
	Y = std::numeric_limits<double>::quiet_NaN();
}

Vector2d::Vector2d(const Vector2d& val)
{
	X = val.X;
	Y = val.Y;
}

Vector2d::Vector2d(double xVal, double yVal)
{
	X = xVal;
	Y = yVal;
}

void Vector2d::Negate()
{
	X = -X;
	Y = -Y;
}

double Vector2d::DistanceTo(Vector2d& val) const
{
	double var2 = X - val.X;
	double var4 = Y - val.Y;
	return sqrt(var2 * var2 + var4 * var4);
}

void Vector2d::Normalize()
{
	double val = 1.0f / sqrt(X * X + Y * Y);
	X *= val;
	Y *= val;
}

Vector2d Vector2d::Normalized() const
{
	double val = 1.0f / sqrt(X * X + Y * Y);
	double x = X * val;
	double y = Y * val;
	return Vector2d(x, y);
}

double Vector2d::Dot(Vector2d val) const
{
	return X * val.X + Y * val.Y;
}

double Vector2d::DistanceSquared(Vector2d val) const
{
	double var1 = X - val.X;
	double var2 = Y - val.Y;
	return var1 * var1 + var2 * var2;
}

bool Vector2d::Equals(Vector2d val) const
{
	return X == val.X && Y == val.Y;
}

std::string Vector2d::ToString() const
{
	return std::string(std::to_string(X) + " " + std::to_string(Y));
}

Vector2d Vector2d::Empty()
{
	return Vector2d();
}

Vector2d Vector2d::operator - (Vector2d right)
{
	return Vector2d(X - right.X, Y - right.Y);
}
Vector2d Vector2d::operator + (Vector2d right)
{
	return Vector2d(X + right.X, Y + right.Y);
}
Vector2d Vector2d::operator * (Vector2d right)
{
	return Vector2d(X * right.X, Y * right.Y);
}
bool Vector2d::operator == (Vector2d right)
{
	return this->Equals(right);
}
bool Vector2d::operator != (Vector2d right)
{
	return !(this->Equals(right));
}
Vector2d& Vector2d::operator += (Vector2d right)
{
	this->X += right.X;
	this->Y += right.Y;
	return *this;
}
Vector2d& Vector2d::operator *= (Vector2d right)
{
	this->X *= right.X;
	this->Y *= right.Y;
	return *this;
}
Vector2d& Vector2d::operator *= (double right)
{
	this->X *= right;
	this->Y *= right;
	return *this;
}
Vector2d& Vector2d::operator = (const Vector2d& right)
{
	this->X = right.X;
	this->Y = right.Y;
	return *this;
}

size_t Vector2d::HashFunction::operator()(const Vector2d& vector) const
{
	return (std::hash<double>()(vector.X) * 397) ^ std::hash<double>()(vector.Y);
}

size_t Vector2d::hash() const
{
	return (std::hash<double>()(X) * 397) ^ std::hash<double>()(Y);
}

bool Vector2d::IsEmpty() const
{
	return isnan(X) && isnan(Y);
}

bool Vector2d::operator < (const Vector2d& right) const
{
	return this->hash() < right.hash();
}
