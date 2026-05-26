#pragma once
#include "3D/Object3d.h"
#include "engine/Input/Input.h"
#include "engine/Camera/Camera.h"
#include "engine/math/MyMath.h"
#include <memory>
#include <string>
#include <list>


class Obstacle;

class Player {
public:
    // 初期化（読み込むモデルの名前を渡す）
    void Initialize(const std::string &modelName);

    // 毎フレームの更新（キーボード入力による移動と回転）
    void Update(const std::list<std::unique_ptr<Obstacle>> &obstacles);

    // 描画
    void Draw();

    // カメラを自機の背後に追従させる
    void UpdateCamera(Camera *camera);

    // ゲッター
    Vector3 GetPosition() const { return position_; }
    Quaternion GetQuaternion() const { return quaternion_; }
    Vector3 GetForwardVector() const; // 今向いている方向（ミサイル発射などに使う）

    //セッター
    void SetPosition(const Vector3 &position) { position_ = position; }
    void SetScale(const Vector3 &scale) { if (object_) object_->SetScale(scale); }
    void SetRotation(const Vector3 &eulerRotation);

    void OnCollision();
    void TakeDamage(int damage);
    bool IsDead() const { return isDead_; }
    int GetHP() const { return hp_; }

    void Move(); // 移動と回転の処理
    void CheckCollision(const std::list<std::unique_ptr<Obstacle>> &obstacles); // 当たり判定の処理

private:
    std::unique_ptr<Object3d> object_;

    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Quaternion quaternion_ = { 0.0f, 0.0f, 0.0f, 1.0f }; // 単位クォータニオン（無回転）

    float speed_ = 0.02f;       // 前進スピード
    float rotSpeed_ = 0.05f;   // 旋回スピード

    bool isDead_ = false;
    int hp_ = 3;
};

