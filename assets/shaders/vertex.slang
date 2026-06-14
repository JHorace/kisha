struct ModelTransformParameters
{
    float4x4 modelTransform;
    float4x4 inverseTransposeModelTransform;
}

ParameterBlock<ModelTransformParameters> gModelTransform;

struct VertexStageInput
{
    float3 position;
    float3 normal;
    float2 uv;
};

struct VertexStageOutput
{
    float3 position;
    float3 normal;
    float2 uv;
};

[shader("vertex")]
VertexStageOutput vertexMain(VertexStageInput input)
{
    float3 position = input.position;
    float3 normal = input.normal;
    float2 uv = input.uv;

    float3 worldPosition = mul(gModelTransform.modelTransform, float4(position, 1.0)).xyz;
    float3 worldNormal = mul(gModelTransform.inverseTransposeModelTransform, float4(normal, 0.0)).xyz;

    VertexStageOutput output;
    output.position = worldPosition;
    output.normal = worldNormal;
    output.uv = uv;

    return output;
}
