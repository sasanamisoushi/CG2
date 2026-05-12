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
RWStructuredBuffer<int32_t> gFreeCounter : register(u1);

static const uint32_t kMaxParticles = 1024;

// よくあるHash関数
float32_t rand3dTo1d(float32_t3 value, float32_t3 dotDir = float32_t3(12.9898, 78.233, 37.719)) {
    float32_t3 smallValue = sin(value);
    float32_t random = dot(smallValue, dotDir);
    random = frac(sin(random) * 143758.5453);
    return random;
}

float32_t3 rand3dTo3d(float32_t3 value) {
    return float32_t3(
        rand3dTo1d(value, float32_t3(12.989, 78.233, 37.719)),
        rand3dTo1d(value, float32_t3(39.346, 11.135, 83.155)),
        rand3dTo1d(value, float32_t3(73.156, 52.235, 09.151))
    );
}

class RandomGenerator {
    float32_t3 seed;
    
    float32_t3 Generate3d() {
        seed = rand3dTo3d(seed);
        return seed;
    }
    
    float32_t Generate1d() {
        float32_t result = rand3dTo1d(seed);
        seed.x = result;
        return result;
    }
};

[numthreads(1, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID) {
    if (gEmitter.emit != 0) { // 射出許可が出たので射出
        RandomGenerator generator;
        generator.seed = (DTid + gPerFrame.time) * gPerFrame.time;
        
        for (uint32_t countIndex = 0; countIndex < gEmitter.count; ++countIndex) {
            int32_t particleIndex;
            InterlockedAdd(gFreeCounter[0], 1, particleIndex);
            
            if (particleIndex < kMaxParticles) {
                // 乱数でParticleを初期化する
                gParticles[particleIndex].scale = generator.Generate3d() * 0.3f; // 少し小さめにする
                
                // translate はエミッターの位置を中心にランダムに散らす
                float32_t3 offset = (generator.Generate3d() - 0.5f) * 2.0f * gEmitter.radius;
                gParticles[particleIndex].translate = gEmitter.translate + offset;
                
                // 色はランダムにするがアルファは1.0
                gParticles[particleIndex].color.rgb = generator.Generate3d();
                gParticles[particleIndex].color.a = 1.0f;
                
                gParticles[particleIndex].lifeTime = 1.0f;
                gParticles[particleIndex].currentTime = 0.0f;
                
                // 少し上に飛ぶようなランダムな速度を加える
                float32_t3 velocity = (generator.Generate3d() - 0.5f) * 0.1f;
                velocity.y = abs(velocity.y); // 上方向に限定
                gParticles[particleIndex].velocity = velocity;
            }
        }
    }
}
