#include "Missile.h"
#include "3D/Object3dCommon.h"

void Missile::Initialize(const Vector3 &position, const Vector3 &velocity, MissileType type) {
    type_ = type; // タイプを保存

    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dCommon::GetInstance());
    object_->SetModel("Sphere");

    // タイプによって見た目を変える
    if (type_ == MissileType::Normal) {
        if (object_->GetModel()) object_->GetModel()->SetColor({ 1.0f, 1.0f, 0.0f, 1.0f }); // 通常弾は黄色
        object_->SetScale({ 0.3f, 0.3f, 0.3f }); // 通常弾は小さめ
    } else {
        if (object_->GetModel()) object_->GetModel()->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f }); // ミサイルは赤
        object_->SetScale({ 0.5f, 0.5f, 0.5f });
    }

    

    // プレイヤーから渡された初期座標と速度をセット
    position_ = position;
    velocity_ = velocity;

    lifeTimer_ = kLifeTime;
    isDead_ = false;

    // ミサイルタイプのみトレイルを準備
    if (type_ == MissileType::MissileWithTrail) {
        trail_ = std::make_unique<Trail>();
        trail_->Initialize(60);
        trailObject_ = std::make_unique<Object3d>();
        trailObject_->Initialize(Object3dCommon::GetInstance());
        trailObject_->SetModel("SmokeTrail");
    }
}

void Missile::Update(Camera *camera) {
    if (isDead_) return;

    // 1. 移動処理（毎フレーム速度分だけ進む）
    position_.x += velocity_.x;
    position_.y += velocity_.y;
    position_.z += velocity_.z;

    // Bの弾（煙付き）なら、ドリル状にぐねぐね曲げる！
    if (type_ == MissileType::MissileWithTrail) {
        // 経過時間（0から増えていく数値）
        float time = 120.0f - lifeTimer_;

        // サイン波とコサイン波を使って、螺旋を描くように揺らす
        // （0.3f や 0.2f の数値を変えると、揺れの激しさが変わります）
        position_.x += std::cos(time * 0.3f) * 0.2f;
        position_.y += std::sin(time * 0.3f) * 0.2f;
    }

    // 2. 寿命の処理
    lifeTimer_--;
    if (lifeTimer_ <= 0) {
        isDead_ = true; // 寿命が尽きたら死ぬフラグを立てる
    }

    // 3. 座標をモデルに適用して更新
    object_->SetTranslate(position_);
    object_->Update();

    // ミサイルタイプのみトレイルを更新
    if (type_ == MissileType::MissileWithTrail && trail_ && trailObject_) {
        trail_->Update(position_);
        std::vector<VertexData> trailVertices = trail_->GenerateVertices(camera, 0.5f);
        if (trailObject_->GetModel()) {
            trailObject_->GetModel()->UpdateTrailVertices(trailVertices);
        }
        trailObject_->Update();
    }
}

void Missile::Draw() {
    if (isDead_) return;

    if (object_) {
        object_->Draw();
    }

    // ミサイルタイプのみトレイルを描画
    if (type_ == MissileType::MissileWithTrail && trailObject_) {
        trailObject_->Draw();
    }
}
