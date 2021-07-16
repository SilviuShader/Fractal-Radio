struct PixelShaderInput
{
    float4 Position : SV_Position;
    float2 Uv       : TEXCOORD0;
};

Texture2D<float4> g_texture : register(t0);
SamplerState g_pointClampSampler : register(s0);

float4 main(const PixelShaderInput IN) : SV_Target
{
    return g_texture.Sample(g_pointClampSampler, IN.Uv);
}