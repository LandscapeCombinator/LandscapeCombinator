#pragma once

#include "LandscapeEditorObject.h"

namespace EngineCode {
	ALandscape* OnCreateButtonClicked();
	ULandscapeSplineControlPoint* AddControlPoint(ULandscapeSplinesComponent* SplinesComponent, const FVector& LocalLocation);
	ULandscapeSplineSegment* AddSegment(ULandscapeSplineControlPoint* Start, ULandscapeSplineControlPoint* End, bool bAutoRotateStart, bool bAutoRotateEnd);
};