//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: ShaderCompile test, VS with hull and domain stages in one shell
//
//=============================================================================//

// STATIC: "TESS" "0..1"
// TESSELLATION: "HullMain" "DomainMain" "TESS"

const float4x4 cViewProj : register( c8 );
const float4 g_Tess : register( c50 );

struct VS_INPUT
{
	float4 vPos : POSITION;
	float2 vTexCoord0 : TEXCOORD0;
};

#if TESS

struct CONTROL_POINT
{
	float3 f3Pos : TEXCOORD0;
	float2 f2Uv : TEXCOORD1;
};

struct PATCH_CONSTANTS
{
	float f1Edges[3] : SV_TessFactor;
	float f1Inside : SV_InsideTessFactor;
};

struct DS_OUTPUT
{
	float4 vProjPos : SV_Position;
	float2 f2Uv : TEXCOORD1;
};

CONTROL_POINT main( VS_INPUT v )
{
	CONTROL_POINT o;
	o.f3Pos = v.vPos.xyz;
	o.f2Uv = v.vTexCoord0;
	return o;
}

PATCH_CONSTANTS PatchMain( InputPatch<CONTROL_POINT, 3> patch )
{
	PATCH_CONSTANTS o;
	o.f1Edges[0] = g_Tess.x;
	o.f1Edges[1] = g_Tess.x;
	o.f1Edges[2] = g_Tess.x;
	o.f1Inside = g_Tess.x;
	return o;
}

[domain( "tri" )]
[partitioning( "fractional_odd" )]
[outputtopology( "triangle_cw" )]
[outputcontrolpoints( 3 )]
[patchconstantfunc( "PatchMain" )]
CONTROL_POINT HullMain( InputPatch<CONTROL_POINT, 3> patch, uint nId : SV_OutputControlPointID )
{
	return patch[nId];
}

[domain( "tri" )]
DS_OUTPUT DomainMain( PATCH_CONSTANTS constants, float3 f3Bary : SV_DomainLocation, const OutputPatch<CONTROL_POINT, 3> patch )
{
	const float3 f3Pos = ( patch[0].f3Pos * f3Bary.x ) + ( patch[1].f3Pos * f3Bary.y ) + ( patch[2].f3Pos * f3Bary.z );

	DS_OUTPUT o;
	o.vProjPos = mul( float4( f3Pos, 1.0f ), cViewProj );
	o.f2Uv = ( patch[0].f2Uv * f3Bary.x ) + ( patch[1].f2Uv * f3Bary.y ) + ( patch[2].f2Uv * f3Bary.z );
	return o;
}

#else

struct VS_OUTPUT
{
	float4 vProjPos : SV_Position;
	float2 f2Uv : TEXCOORD1;
};

VS_OUTPUT main( VS_INPUT v )
{
	VS_OUTPUT o;
	o.vProjPos = mul( float4( v.vPos.xyz, 1.0f ), cViewProj );
	o.f2Uv = v.vTexCoord0;
	return o;
}

#endif
