#pragma once
#include <string>
#include "MyMath.h"
#include "ParticleManager.h"

class ParticleEmitter {
public:
    // コンストラクタ：必要な情報を引数で受け取る
    ParticleEmitter(std::string name, Vector3 position, ParticleManager *particleManager);

    // 毎フレーム呼び出す更新関数（ここで Emit を呼ぶ）
    void Update();

   
    void Emit();

    uint32_t count = 8;          // 1回の発生数
    float frequency_ = 0.1f;      // 発生間隔
    float radius = 1.0f;         // ※今回は使わなくても一応置いておく

private:
    std::string name_;              // 放出するパーティクルグループの名前
    Vector3 position_;             // 放出地点の中心座標
    ParticleManager *pManager_ = nullptr; // パーティクルマネージャへのポインタ

    // タイマーなどの追加変数（例：0.5秒ごとに放出するなど）
    float frameCount_ = 0.0f;
};

