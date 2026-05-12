#include "Particle.hlsli"

struct Particle {
    float32_t3 translate;
    float32_t3 scale;
    float32_t lifeTime;
    float32_t3 velocity;
    float32_t currentTime;
    float32_t4 color;
};
StructuredBuffer<Particle> gParticles : register(t0);

struct CameraData {
    float32_t4x4 viewProjection;
    float32_t4x4 billboard;
};
ConstantBuffer<CameraData> gCamera : register(b0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t4 texcoord : TEXCOORD0;
    float32_t4 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    Particle particle = gParticles[instanceId];
    VertexShaderOutput output;

    // ワールド行列の計算 (Scale * Billboard * Translate)
    float32_t4x4 world = gCamera.billboard;
    // スケール適用
    world[0].xyz *= particle.scale.x;
    world[1].xyz *= particle.scale.y;
    world[2].xyz *= particle.scale.z;
    // 平行移動適用
    world[3].xyz = particle.translate;

    output.position = mul(input.position, mul(world, gCamera.viewProjection));
    output.texcoord = input.texcoord.xy;
    output.normal = normalize(mul(input.normal.xyz, (float32_t3x3) world));
    output.color = particle.color;
    
    // 寿命によるフェードアウト
    float alpha = 1.0f - (particle.currentTime / particle.lifeTime);
    output.color.a *= saturate(alpha);
    
    // 寿命が尽きていたら描画されないように座標を飛ばす
    if (particle.currentTime >= particle.lifeTime) {
        output.position = float32_t4(0, 0, 0, 0);
    }

    return output;
}