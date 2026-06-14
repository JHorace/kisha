import vertex;

struct BasicLightParameters
{
    float3 color;
    float3 lightPos;
};

ParameterBlock<BasicLightParameters> gBasicLightParameters;

[shader("fragment")]
float4 fragmentMain(VertexStageOutput input) : SV_Target
{
    // Create a vector going heading towards the light
    float3 surfaceToLight = gBasicLightParameters.lightPos - input.position;

    // Project the light angle to surface normal.
    float lightProjection = dot(input.normal, surfaceToLight);

    // Color the face based on how the angle is reletive to the light
    float3 color = gBasicLightParameters.color * lightProjection;

    return float4(color, 1.0);
}