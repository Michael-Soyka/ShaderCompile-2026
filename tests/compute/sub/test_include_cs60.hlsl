//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: ShaderCompile test, includes next to the file and from the shader root
//
//=============================================================================//

// CS_MAIN: CSMain

#include "local.hlsli"
#include "common/root.hlsli"

RWStructuredBuffer<uint> g_Data : register( u0 );

[numthreads( 64, 1, 1 )]
void CSMain( uint3 id : SV_DispatchThreadID )
{
	g_Data[id.x] = LocalValue( id.x ) + RootValue( id.x );
}
