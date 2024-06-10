
struct MatrixSet
{
	float4x4 ViewProjection;
	float3 LightPosition;
};

ConstantBuffer<MatrixSet> Projection : register(b0);
Texture2D NormalTexture				 : register(t0);
SamplerState NormalSampler			 : register(s0);

struct VSout
{
	float4 _ : SV_Position;
	
	float3 Position : POSITION;
	float2 UVCoord : UV_COORD;
};

VSout VSmain(float4 position : POSITION, float4 uv : UV_COORD)
{
	VSout result;
	
	result._ = mul(Projection.ViewProjection, float4(position.xyz, 1.0f));
	result.Position = position.xyz;
	result.UVCoord = uv.xy;
	
	return result;
}

float4 PSmain(VSout param) : SV_Target
{
	return NormalTexture.Sample(NormalSampler, param.UVCoord);
	//return float4(param.UVCoord, 0.0f, 1.0f);

}