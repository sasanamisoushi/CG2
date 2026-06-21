#pragma once
#include "3D/Object3d.h"
#include "engine/Input/Input.h"
#include "engine/Camera/Camera.h"
#include "engine/math/MyMath.h"
#include "3D/Animation.h"
#include <memory>
#include "BoosterEffect.h"
#include <string>
#include <list>

class Obstacle;

enum class PlayerMode {
    Fighter,  // 高速飛行
    Gerwalk,  // ホバリング
    Battroid  // 人型
};

struct PlayerModeParams {
    float maxMoveSpeed = 0.22f;
    float moveAcceleration = 0.018f;
    float moveDamping = 0.90f;
    float pitchSpeed = 0.015f;
    float yawSpeed = 0.014f;
    float rollSpeed = 0.025f;
};

class Player {
public:
    // 初期化（読み込むモデルの名前を渡す）
    void Initialize(const std::string &modelName);

    // 毎フレームの更新（キーボード入力による移動と回転）
    void Update(const std::list<std::unique_ptr<Obstacle>> &obstacles);

    // 描画
    void Draw();

    // 更新だけしてロジックを動かさない処理（シミュレーション時など用）
    void UpdateModel();

    // カメラへの追従（Debug用のカメラではなく、本番用カメラをプレイヤーの後ろに置く処理）
    void UpdateCamera(Camera *camera, const Vector3 *targetPos = nullptr);

    // ゲッター
    Vector3 GetPosition() const { return position_; }
    Quaternion GetQuaternion() const { return quaternion_; }
    Vector3 GetWorldHalfExtents() const;
    float GetCollisionRadius() const;
    OBB GetOBB() const;
    const std::string& GetModelName() const { return modelName_; }
    Vector3 GetForwardVector() const; // 今向いている方向（ミサイル発射などに使う）

    // セッター
    void SetPosition(const Vector3 &position) {
        position_ = position;
        if (object_) {
            object_->SetTranslate(position_);
            object_->Update();
        }
    }
    void SetScale(const Vector3 &scale) { 
        modelScale_ = scale;
        if (object_) object_->SetScale(scale); 
    }
    void SetRotation(const Vector3 &eulerRotation);

    void OnCollision();
    void TakeDamage(int damage);
    bool IsDead() const { return isDead_; }
    int GetHP() const { return hp_; }

    void Move(); // 移動と回転の処理
    void CheckCollision(const std::list<std::unique_ptr<Obstacle>> &obstacles); // 当たり判定の処理
    void UpdateLockOnRotation(const Vector3& targetPos); // ロックオン時の強制回転

    // モード関連
    void ChangeMode(PlayerMode newMode);
    PlayerMode GetCurrentMode() const { return currentMode_; }
    PlayerModeParams& GetModeParams(PlayerMode mode) { return modeParams_[static_cast<int>(mode)]; }

    // アニメーション関連デバッグ用ゲッターセッター
    float GetAnimationTime() const { return animationTime_; }
    void SetAnimationTime(float time) { animationTime_ = time; }
    float GetTargetAnimationTime() const { return targetAnimationTime_; }
    void SetTargetAnimationTime(float time) { targetAnimationTime_ = time; }
    float GetAnimationDuration() const { return animationData_.duration; }
    bool IsAnimDebugActive() const { return isAnimDebugActive_; }
    void SetAnimDebugActive(bool active) { isAnimDebugActive_ = active; }

private:
	std::unique_ptr<Object3d> object_;
	std::string modelName_;
	Vector3 modelScale_ = { 1.0f, 1.0f, 1.0f };
    Vector3 currentDrawScale_ = { 1.0f, 1.0f, 1.0f };
    Vector3 targetDrawScale_ = { 1.0f, 1.0f, 1.0f };

    PlayerMode currentMode_ = PlayerMode::Fighter;
    PlayerModeParams modeParams_[3];

    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
    Quaternion quaternion_ = { 0.0f, 0.0f, 0.0f, 1.0f }; // 単位クォータニオン（無回転）
    float cameraPitch_ = 0.0f;

    bool isDead_ = false;
    int hp_ = 3;

    Animation animationData_;
    Skeleton skeleton_;
    float animationTime_ = 0.0f;
    float targetAnimationTime_ = 0.0f;
    bool enableSkinning_ = false;
    bool isAnimDebugActive_ = false;

    std::unique_ptr<BoosterEffect> boosterEffect_;
};


