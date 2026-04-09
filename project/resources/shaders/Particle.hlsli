typedef float4 float32_t4;
typedef float3 float32_t3;
typedef float2 float32_t2;
typedef float4x4 float32_t4x4;
typedef int int32_t;
typedef uint uint32_t;

struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t4 color : COLOR0;
};