#include "Particle.hlsli"

struct Particle {
    float32_t3 translate;
    float32_t3 scale;
    float32_t lifeTime;
    float32_t3 velocity;
    float32_t currentTime;
    float32_t4 color;
};

struct ParticleGPUParam {
    float32_t3 translate;
    float32_t3 scale;
    float32_t4 color;
};

static const uint32_t kMaxParticles = 1024;
RWStructuredBuffer<Particle> gParticles : register(u0);
ConstantBuffer<ParticleGPUParam> gParam : register(b1);

[numthreads(1024, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;
    if (particleIndex < kMaxParticles) {
        gParticles[particleIndex].translate = gParam.translate;
        gParticles[particleIndex].scale = gParam.scale;
        gParticles[particleIndex].velocity = float32_t3(0.0f, 0.0f, 0.0f);
        gParticles[particleIndex].color = gParam.color;
        gParticles[particleIndex].lifeTime = 1.0f;
        gParticles[particleIndex].currentTime = 0.0f;
    }
}
