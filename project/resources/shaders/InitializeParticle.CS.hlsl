#include "Particle.hlsli"

struct Particle {
    float32_t3 translate;
    float32_t3 scale;
    float32_t lifeTime;
    float32_t3 velocity;
    float32_t currentTime;
    float32_t4 color;
};

struct EmitterSphere {
    float32_t3 translate; // 位置
    float32_t radius; // 射出半径
    uint32_t count; // 射出数
    float32_t frequency; // 射出間隔
    float32_t frequencyTime; // 射出間隔調整用時間
    uint32_t emit; // 射出許可
};

struct PerFrame {
    float32_t time;
    float32_t deltaTime;
};

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);

static const uint32_t kMaxParticles = 1024;

[numthreads(1024, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;
    
    // パーティクルの初期化とFreeListの連番初期化
    if (particleIndex < kMaxParticles) {
        gParticles[particleIndex] = (Particle)0;
        gFreeList[particleIndex] = particleIndex;
    }
    
    // Indexが末尾を指すようにする（1スレッドだけ実行）
    if (particleIndex == 0) {
        gFreeListIndex[0] = kMaxParticles - 1;
    }
}
