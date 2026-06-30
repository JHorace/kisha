struct VertexStageOutput
{
    float4 position : SV_Position;
};

[shader("vertex")]
VertexStageOutput vertexMain(uint vertexID : SV_VertexID)
{
    float2 positions[3] = {
        float2(0.0, -0.5),
        float2(0.5, 0.5),
        float2(-0.5, 0.5),
    };

    VertexStageOutput output;
    output.position = float4(positions[vertexID], 0.0, 1.0);
    return output;
}
