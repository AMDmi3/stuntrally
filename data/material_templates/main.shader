
#include "core.h"

#ifdef SH_VERTEX_SHADER

	SH_BEGIN_PROGRAM
		shUniform(float4x4 wvp) @shAutoConstant(wvp, worldviewproj_matrix)
	SH_START_PROGRAM
	{
		shOutputPosition = shMatrixMult(wvp, shInputPosition);
	}

#else

	SH_BEGIN_PROGRAM
	SH_START_PROGRAM
	{
		shOutputColor = float4(1.0, 0.0, 0.0, 1.0);
	}

#endif
