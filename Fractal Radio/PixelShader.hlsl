struct PixelShaderInput
{
    float4 Position : SV_Position;
    float2 Uv : TEXCOORD0;
};

float4 main(PixelShaderInput IN) : SV_Target
{
    return float4(IN.Uv.x, IN.Uv.y, 0.0f, 1.0f);
}