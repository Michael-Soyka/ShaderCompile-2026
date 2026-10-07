//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: ShaderCompile test, compute shader with static and dynamic combos
//
//=============================================================================//

// STATIC:	"MODE"	"0..1"
// DYNAMIC:	"SCALE"	"0..1"
// SKIP: ( $MODE == 1 && $SCALE == 1 )

RWStructuredBuffer<uint> g_Data : register( u0 );

[numthreads( 64, 1, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
#if MODE
	g_Data[id.x] = ( id.x * ( 3 + SCALE ) ) + 7;
#else
	g_Data[id.x] = id.x + SCALE;
#endif
}
