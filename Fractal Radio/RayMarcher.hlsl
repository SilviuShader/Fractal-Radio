#define BLOCK_SIZE 8

struct ComputeShaderInput
{
    uint3 GroupId           : SV_GroupID;
    uint3 GroupThreadId     : SV_GroupThreadID;
    uint3 DispatchThreadId  : SV_DispatchThreadID;
    uint  GroupIndex        : SV_GroupIndex;
};

RWTexture2D<float4> g_outputTexture : register(u0);

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(ComputeShaderInput IN)
{
    g_outputTexture[IN.DispatchThreadId.xy] = float4(IN.DispatchThreadId.x * 0.1f, IN.DispatchThreadId.y * 0.1f, 1.0f, 1.0f);
}