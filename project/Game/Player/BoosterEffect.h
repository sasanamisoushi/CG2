#pragma once
#include "3D/Object3d.h"
#include "engine/math/MyMath.h"
#include <memory>
#include <vector>
#include <string>

class BoosterEffect {
public:
    // 初期化
    void Initialize();

    // 更新処理
    // position: プレイヤーの位置
    // rotation: プレイヤーの回転クォータニオン
    // playerMode: 現在のプレイヤーの形態 (0: Fighter, 1: Gerwalk, 2: Battroid)
    // speedRatio: 速度比（0.0f 〜 1.0f）
    // isAccelerating: 加速中か
    void Update(const Vector3& position, const Quaternion& rotation, int playerMode, float speedRatio, bool isAccelerating);

    // 描画処理
    void Draw();

private:
    struct Burner {
        std::unique_ptr<Object3d> object;
        Vector3 offset;       // プレイヤーの原点からのローカルオフセット位置
        Vector3 defaultScale; // 基準となる大きさ
        Vector3 currentScale; // 現在のスケール
        Vector4 color;        // 炎の色
    };

    std::vector<Burner> burners_;
    int lastMode_ = -1;
    float time_ = 0.0f;

    // 形態に合わせてバーニアノズルをセットアップする
    void SetupBurnersForMode(int playerMode);
};
