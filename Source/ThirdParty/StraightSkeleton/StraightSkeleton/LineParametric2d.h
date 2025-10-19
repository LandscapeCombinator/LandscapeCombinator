#pragma once
#include <string>
#include "LineLinear2d.h"
#include <memory>
class LineParametric2d
{
private:
	using spv2d = std::shared_ptr<Vector2d>;
public:
	spv2d A = nullptr;
	spv2d U = nullptr;
	static LineParametric2d Empty();
	LineParametric2d();
	LineParametric2d(spv2d pA, spv2d pU);
	LineParametric2d(Vector2d pA, Vector2d pU);
	~LineParametric2d();
	LineLinear2d CreateLinearForm();
	static Vector2d Collide(LineParametric2d ray, LineLinear2d line, double epsilon);
	bool IsOnLeftSite(Vector2d point, double epsilon);
	bool IsOnRightSite(Vector2d point, double epsilon);
	std::string ToString() const;
};

