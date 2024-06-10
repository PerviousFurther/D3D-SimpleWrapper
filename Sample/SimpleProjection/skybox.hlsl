TextureCube SkyBox : register(t1);
SamplerState Sampler : register(s0);

struct Projections
{
	float4x4 ViewProjection;
	float4 EyePosition;
};

ConstantBuffer<Projections> Projection : register(b0);

struct VSout
{
	float4 Position : SV_Position;
	float3 PosLocal : POSITION;
};
VSout VSmain(float4 pos : POSITION)
{
	VSout value;
	value.PosLocal = pos.xyz;
	value.Position = mul(Projection.ViewProjection, (pos + Projection.EyePosition)).xyww;
	return value;
}

float4 PSmain(in VSout value) : SV_Target
{
	return SkyBox.Sample(Sampler, value.PosLocal);
}