//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: ShaderCompile test, compute shader that must fail to compile
//
//=============================================================================//

[numthreads( 64, 1, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	g_Undefined[id.x] = 1;
}
