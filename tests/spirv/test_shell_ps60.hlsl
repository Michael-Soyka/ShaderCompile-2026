//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: ShaderCompile test, SPIR-V pixel shader with slot 0 and bank slot 20
//
//=============================================================================//

// STATIC:	"MODE"	"0..1"
// DYNAMIC:	"FLIP"	"0..1"

const float4 g_Probe : register( c5 );

[[vk::combinedImageSampler]][[vk::binding( 13, 0 )]] Texture2D g_Base : register( t0 );
[[vk::combinedImageSampler]][[vk::binding( 13, 0 )]] SamplerState g_BaseSampler : register( s0 );
[[vk::combinedImageSampler]][[vk::binding( 36, 0 )]] Texture2D g_Ex : register( t20 );
[[vk::combinedImageSampler]][[vk::binding( 36, 0 )]] SamplerState g_ExSampler : register( s20 );

struct PS_INPUT
{
	float2 vTexCoord : TEXCOORD1;
	float4 vColor : COLOR0;
};

float4 main( PS_INPUT i ) : SV_Target0
{
	float2 uv = FLIP ? ( 1.0f - i.vTexCoord ) : i.vTexCoord;
	float4 base = g_Base.Sample( g_BaseSampler, uv );
	float4 ex = g_Ex.Sample( g_ExSampler, uv );
	float4 result = MODE ? ( base * ex ) : lerp( base, ex, g_Probe.x );
	// g_Probe.y is runtime data, not a compile-time constant: this keeps $Globals referenced
	// (and its binding alive) in every MODE combo, matching the fixed binding OTM DXVK expects
	result.a *= saturate( g_Probe.y + 1.0f );
	return result * i.vColor;
}
