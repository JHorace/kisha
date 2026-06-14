module vertex;

public struct VertexStageInput
{
    public float3 position;
    public float3 normal;
    public float2 uv;
};

public struct VertexStageOutput
{
    public float3 position : SV_Position;
    public float3 normal;
    public float2 uv;
};
