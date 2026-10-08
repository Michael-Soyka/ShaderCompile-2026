//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: ShaderCompile test, SPIR-V vertex shader in an SM3 shell
//
//=============================================================================//

const float4x4 cModelViewProj : register( c4 );

struct VS_INPUT
{
	float4 vPos : POSITION;
	float2 vTexCoord0 : TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 vProjPos : SV_Position;
	float2 vTexCoord : TEXCOORD1;
};

VS_OUTPUT main( VS_INPUT v )
{
	VS_OUTPUT o;
	o.vProjPos = mul( float4( v.vPos.xyz, 1.0f ), cModelViewProj );
	o.vTexCoord = v.vTexCoord0;
	return o;
}
