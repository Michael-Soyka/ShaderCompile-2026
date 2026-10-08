//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: ShaderCompile test, SPIR-V pixel shader larger than the engine unpack block
//
//=============================================================================//

const float4 g_Probe : register( c5 );

float4 main( float2 vTexCoord : TEXCOORD0 ) : SV_Target0
{
	float4 r = float4( vTexCoord, g_Probe.xy );

	[unroll]
	for ( int k = 0; k < 2000; k++ )
	{
		r = sin( r * ( 1.0f + ( k * 0.001f ) ) ) + g_Probe.zwxy;
	}

	return r;
}
