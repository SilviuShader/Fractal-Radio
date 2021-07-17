#define BLOCK_SIZE 8
#define MAX_STEPS 64
#define MINIMUM_DISTANCE 0.01f
#define NORMAL_THRESHOLD 0.1f
#define SIERPINSKI_ITERATIONS 10
#define MAX_CAMERA_DEPTH 100.0f
#define GLOW_FACTOR 0.5f
#define MAX_RAYS_DEPTH 5

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

struct TraceResult
{
    float AmbientOcclusion;
    bool Hit;
    float3 Normal;
    float3 Color;
    int NumSteps;
    bool Blocked;
};

float SphereEstimator(float3 crtPosition, float3 spherePosition, float radius)
{
    return length(crtPosition - spherePosition) - radius;
}

float SpheresEstimator(float3 crtPosition, float3 spherePosition)
{
    float3 z = crtPosition - spherePosition;
    z.xz = fmod((z.xz), 1.0f) - float2(0.5f, 0.5f);
    return length(z) - 0.3f;
}


float Sierpinski(float3 crtPosition, float3 tetrahedronPosition)
{
    float3 z = crtPosition - tetrahedronPosition;

    float r;
    int n = 0;
    float scale = 2.0f;
    while (n < SIERPINSKI_ITERATIONS) {
        if (z.x + z.y < 0) z.xy = -z.yx; // fold 1
        if (z.x + z.z < 0) z.xz = -z.zx; // fold 2
        if (z.y + z.z < 0) z.zy = -z.yz; // fold 3	
        z = z * scale - float3(1.0f, 1.0f, 1.0f) * (scale - 1.0);
        n++;
    }
    return (length(z)) * pow(scale, -float(n));
}

float YPlane(float3 crtPosition, float y)
{
    return crtPosition.y - y;
}

float DistanceEstimator(float3 position)
{
    return min(SpheresEstimator(position, float3(0.0f, 1.0f, 3.0f)), YPlane(position, -1.0f));
}

float3 Reflect(const float3 I, const float3 N)
{
    return I - 2 * dot(I, N) * N;
}

bool IsBlocked(float3 from, float3 direction)
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
            return true;
        if (distance > MAX_CAMERA_DEPTH)
            return false;
    }

    return false;
}

TraceResult IterativeTrace(float3 from, float3 direction)
{
    TraceResult intersectionsStack[MAX_RAYS_DEPTH];
    int stackLength = 0;
    bool stillGoing = true;

    for (int depth = 0; depth < MAX_RAYS_DEPTH && stillGoing; depth++)
    {
        int steps;
        float totalDistance = 0.0f;
        bool stopped = false;

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

                TraceResult crtResult;
                crtResult.Hit = true;
                crtResult.Normal = normal;
                crtResult.AmbientOcclusion = ambientOcclusion;
                crtResult.Color = float3(1.0f, 1.0f, 1.0f);
                crtResult.NumSteps = steps;

                
                float3 reflected = Reflect(direction, normal);
                from = crtPoint + reflected * 0.1f;
                direction = reflected;
                float lightDirection = normalize(-LIGHT_DIRECTION);
                float3 toLight = crtPoint + lightDirection * 1.0f;
                crtResult.Blocked = IsBlocked(toLight, lightDirection);

                intersectionsStack[stackLength++] = crtResult;

                stopped = true;

                break;
            }

            if (distance > MAX_CAMERA_DEPTH)
            {
                TraceResult crtResult;
                crtResult.Hit = false;
                crtResult.AmbientOcclusion = 0.0f;
                crtResult.Normal = direction;
                crtResult.Color = float3(0.0f, 0.0f, 0.0f);
                crtResult.NumSteps = steps;
                crtResult.Blocked = false;
                intersectionsStack[stackLength++] = crtResult;
                stillGoing = false;

                stopped = true;

                break;
            }
        }

        if (!stopped)
        {
            TraceResult crtResult;
            crtResult.Hit = false;
            crtResult.AmbientOcclusion = 0.0f;
            crtResult.Normal = direction;
            crtResult.Color = float3(0.0f, 0.0f, 0.0f);
            crtResult.NumSteps = steps;
            crtResult.Blocked = false;
            intersectionsStack[stackLength++] = crtResult;
            stillGoing = false;
        }
    }

    TraceResult finalResult;
    finalResult.Color = float3(0.0f, 0.0f, 0.0f);

    for (int i = 0; i >= 0; i--)
    {
        TraceResult crtResult = intersectionsStack[i];

        float3 lightDirection = normalize(-LIGHT_DIRECTION);
        float lightIntensity = max(0.1f, dot(crtResult.Normal, lightDirection));
        float color = crtResult.AmbientOcclusion * lightIntensity;

        crtResult.Color = color;

        if (crtResult.Blocked)
            crtResult.Color *= 0.5f;

        finalResult.Color += crtResult.Color;
        finalResult.Hit = crtResult.Hit;
        finalResult.Normal = crtResult.Normal;
        finalResult.AmbientOcclusion = crtResult.AmbientOcclusion;
        finalResult.NumSteps = crtResult.NumSteps;

        intersectionsStack[i] = crtResult;
    }

    //if (intersectionsStack[1].Hit)
        //finalResult.Color = float3(0.0f, 0.0f, 1.0f);

    return finalResult;
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

    TraceResult result = IterativeTrace(onCameraPoint, rayDirection);

    g_outputTexture[IN.DispatchThreadId.xy] = float4(result.Color, 1.0f);
}