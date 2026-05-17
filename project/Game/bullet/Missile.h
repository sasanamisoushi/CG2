#pragma once
#include "3D/Object3d.h"
#include "engine/math/MyMath.h"
#include "3D/Trail.h" 
#include "engine/Camera/Camera.h"
#include <memory>
#include <string>


class Enemy;

// 弾の種類を定義
enum class MissileType {
    Normal,         // 真っ直ぐ飛ぶ弾（煙なし）
    MissileWithTrail // 煙を引くミサイル
};

class Missile {
public:
    // 初期化（発生位置と飛んでいく方向を渡す）
    void Initialize(const Vector3 &position, const Vector3 &velocity, MissileType type);

    // 毎フレームの更新
    void Update(Camera *camera, Enemy *enemy);

    // 描画
    void Draw();

    // 寿命が尽きたかどうか（外部から削除タイミングを判定するため）
    bool IsDead() const { return isDead_; }

    // 当たった時に呼ばれる関数
    void OnCollision();

    // 自分の座標を外に教えるゲッター
    Vector3 GetPosition() const { return position_; }

private:
    std::unique_ptr<Object3d> object_;
    MissileType type_;     // 自分のタイプを保持

    // このミサイル専用の軌跡（煙）オブジェクト
    std::unique_ptr<Trail> trail_;
    std::unique_ptr<Object3d> trailObject_;

    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f }; // 1フレームに進む量

    // 寿命（画面外に飛び去った弾をメモリから消すため）
    int lifeTimer_ = 0;
    static const int kLifeTime = 120; // 120フレーム（約2秒）で消滅
    bool isDead_ = false;
};

