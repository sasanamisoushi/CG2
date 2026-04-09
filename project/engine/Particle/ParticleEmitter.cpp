#include "ParticleEmitter.h"

// コンストラクタの実装
ParticleEmitter::ParticleEmitter(std::string name, Vector3 position, ParticleManager *particleManager)
    : name_(name), position_(position), pManager_(particleManager) // メンバ変数に書き込み
{
}

void ParticleEmitter::Update() {
    //時刻を進める
    const float kDeltaTime = 1.0f / 60.0f; // 60FPS固定と仮定
	frameCount_ += kDeltaTime;

    // 発生頻度より大きいなら発生
    // 余計に過ぎた時間も加味して頻度計算する（whileループにすることで遅れを取り戻す）
    while (frameCount_ >= frequency_) {

        // 作った関数を呼ぶだけ
        Emit();

        frameCount_ -= frequency_;
    }
}

void ParticleEmitter::Emit() {
    // マネージャが設定されているか確認
    if (pManager_) {
        // pManager_を通してEmitを呼び出す
        // name -> name_ (メンバ変数)
        // transform.translate -> position_ (メンバ変数)
        // count -> 3 (ひとまず3個。変数化したい場合はヘッダーに count_ を追加)
        pManager_->Emit(name_, position_, 3);
    }
}
