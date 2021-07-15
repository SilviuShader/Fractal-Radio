struct VertexPosColor
{
    float3 Position : POSITION;
    float2 Uv       : TEXCOORD;
};

struct VertexShaderOutput
{
    float4 Position : SV_Position;
    float2 Uv       : TEXCOORD0;
};

VertexShaderOutput main(VertexPosColor IN)
{
    VertexShaderOutput OUT;

    OUT.Position = float4(IN.Position, 1.0f);
    OUT.Uv = IN.Uv;

    return OUT;
}