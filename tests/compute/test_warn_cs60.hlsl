//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: ShaderCompile test, compute shader that compiles with a warning only
//
//=============================================================================//

RWStructuredBuffer<uint> g_Data : register( u0 );

[numthreads( 64, 1, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	float2 v = float3( 1.0f, 2.0f, 3.0f );
	g_Data[id.x] = uint( v.x );
}
