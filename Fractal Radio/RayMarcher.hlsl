#define BLOCK_SIZE 8
#define MAX_STEPS 16
#define MINIMUM_DISTANCE 0.05f
#define NORMAL_THRESHOLD 1.0f

#define LIGHT_DIRECTION float3(-0.5f, -0.5f, 0.5f)

struct ComputeShaderInput
{
    uint3 GroupId           : SV_GroupID;
    uint3 GroupThreadId     : SV_GroupThreadID;
    uint3 DispatchThreadId  : SV_DispatchThreadID;
    uint  GroupIndex        : SV_GroupIndex;
};

cbuffer RayMarcherContrantBuffer : register(b0)
{
    float2 g_windowSize;
    matrix g_cameraMatrix;
}

RWTexture2D<float4> g_outputTexture : register(u0);

float SphereEstimator(float3 crtPosition, float3 spherePosition)
{
    return length(crtPosition - spherePosition) - 1.0f;
}

float DistanceEstimator(float3 position)
{
    return SphereEstimator(position, float3(0.0f, 0.0f, 20.0f));
}

float Trace(float3 from, float3 direction)
{
    float totalDistance = 0.0f;
    int steps;
    float resultTone = 0.0f;
    for (steps = 0; steps < MAX_STEPS; steps++)
    {
        float3 crtPoint = from + totalDistance * direction;
        float distance = DistanceEstimator(crtPoint);
        totalDistance += distance;
        if (distance < MINIMUM_DISTANCE)
        {
            const float ambientOcclusion = 1.0 - float(steps) / float(MAX_STEPS);

            float3 xyy = float3(1.0f, -1.0f, -1.0f);
            float3 xyx = float3(-1.0f, 1.0f, -1.0f);
            float3 yyx = float3(-1.0f, -1.0f, 1.0f);
            float3 xxx = float3(1.0f, 1.0f, 1.0f);
            
            float3 normal = xyy * DistanceEstimator(crtPoint + xyy * NORMAL_THRESHOLD) + 
                            xyx * DistanceEstimator(crtPoint + xyx * NORMAL_THRESHOLD) + 
                            yyx * DistanceEstimator(crtPoint + yyx * NORMAL_THRESHOLD) +
                            xxx * DistanceEstimator(crtPoint + xxx * NORMAL_THRESHOLD);

            normal = normalize(normal);

            float3 lightDirection = normalize(-LIGHT_DIRECTION);
            float lightIntensity = dot(normal, lightDirection);

            resultTone = lightIntensity * ambientOcclusion;
            break;
        }
    }

    return resultTone;
}

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(ComputeShaderInput IN)
{
    float2 normalizedCoords = ((IN.DispatchThreadId.xy / g_windowSize) * 2.0f) - float2(1.0f, 1.0f);
    normalizedCoords.x *= g_windowSize.x / g_windowSize.y;
    normalizedCoords.y *= -1.0f;
    
    float3 onCameraPoint = float3(normalizedCoords.x, normalizedCoords.y, 5.0f);
    float3 eye = float3(0.0f, 0.0f, 0.0f);

    float3 rayDirection = onCameraPoint - eye;

    eye = mul(g_cameraMatrix, float4(eye, 1.0f)).xyz;
    rayDirection = mul(g_cameraMatrix, float4(rayDirection, 0.0f)).xyz;
    onCameraPoint = eye + rayDirection;

    rayDirection = normalize(rayDirection);

    float resultTone = Trace(onCameraPoint, rayDirection);


    g_outputTexture[IN.DispatchThreadId.xy] = float4(resultTone, resultTone, resultTone, 1.0f);
}