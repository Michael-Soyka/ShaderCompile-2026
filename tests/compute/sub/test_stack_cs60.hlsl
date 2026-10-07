//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: ShaderCompile test, nested include must not pick a header from the includer stack
//
//=============================================================================//

#include "u.hlsli"
#include "common/c.hlsli"

RWStructuredBuffer<uint> g_Data : register( u0 );

[numthreads( 64, 1, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	g_Data[id.x] = id.x;
}
