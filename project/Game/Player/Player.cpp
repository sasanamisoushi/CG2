#include "Player.h"
#include "3D/Object3dCommon.h"
#include "Game/obstacle/Obstacle.h"
#include <algorithm>
#include <cmath>

namespace {
	float LengthSq(const Vector3 &value) {
		return value.x * value.x + value.y * value.y + value.z * value.z;
	}

	float Length(const Vector3 &value) {
		return std::sqrt(LengthSq(value));
	}

	Vector3 Add(const Vector3 &a, const Vector3 &b) {
		return { a.x + b.x, a.y + b.y, a.z + b.z };
	}

	Vector3 Scale(const Vector3 &value, float scalar) {
		return { value.x * scalar, value.y * scalar, value.z * scalar };
	}

	Vector3 NormalizeOrZero(const Vector3 &value) {
		const float length = Length(value);
		if (length <= 0.0001f) {
			return { 0.0f, 0.0f, 0.0f };
		}
		return Scale(value, 1.0f / length);
	}

	Vector3 ClampLength(const Vector3 &value, float maxLength) {
		const float length = Length(value);
		if (length <= maxLength || length <= 0.0001f) {
			return value;
		}
		return Scale(value, maxLength / length);
	}
}

void Player::Initialize(const std::string &modelName) {
    for (int i = 0; i < 3; ++i) {
	    objects_[i] = std::make_unique<Object3d>();
	    objects_[i]->Initialize(Object3dCommon::GetInstance());
	    objects_[i]->SetModel(modelName);

	    if (objects_[i]->GetModel()) {
		    objects_[i]->GetModel()->SetColor({ 0.2f, 0.5f, 1.0f, 1.0f }); // 青色
	    }
	    objects_[i]->SetScale({ 0.2f, 0.2f, 0.2f });
    }

	position_ = { 0.0f, 0.0f, 0.0f };
	velocity_ = { 0.0f, 0.0f, 0.0f };
	quaternion_ = { 0.0f, 0.0f, 0.0f, 1.0f };
	hp_ = 3;
	isDead_ = false;

    // 形態ごとの初期パラメータ設定
    // 0: Fighter
    modeParams_[0].maxMoveSpeed = 0.6f;
    modeParams_[0].moveAcceleration = 0.05f;
    modeParams_[0].moveDamping = 0.98f;
    modeParams_[0].pitchSpeed = 0.03f;
    modeParams_[0].yawSpeed = 0.03f;
    modeParams_[0].rollSpeed = 0.05f;

    // 1: Gerwalk
    modeParams_[1].maxMoveSpeed = 0.22f;
    modeParams_[1].moveAcceleration = 0.018f;
    modeParams_[1].moveDamping = 0.90f;
    modeParams_[1].pitchSpeed = 0.015f;
    modeParams_[1].yawSpeed = 0.014f;
    modeParams_[1].rollSpeed = 0.025f;

    // 2: Battroid
    modeParams_[2].maxMoveSpeed = 0.12f;
    modeParams_[2].moveAcceleration = 0.025f;
    modeParams_[2].moveDamping = 0.80f;
    modeParams_[2].pitchSpeed = 0.02f;
    modeParams_[2].yawSpeed = 0.02f;
    modeParams_[2].rollSpeed = 0.02f;

    // 初期形態
    ChangeMode(PlayerMode::Fighter);
}

void Player::ChangeMode(PlayerMode newMode) {
    currentMode_ = newMode;
    // 形態ごとの見た目の変更（スケールを変えて仮表現する）
    for (int i = 0; i < 3; ++i) {
        if (objects_[i]) {
            if (i == 0) { // Fighter: 平べったく
                objects_[i]->SetScale({0.4f, 0.1f, 0.5f});
            } else if (i == 1) { // Gerwalk: 中間
                objects_[i]->SetScale({0.2f, 0.2f, 0.2f});
            } else { // Battroid: 縦長
                objects_[i]->SetScale({0.15f, 0.4f, 0.15f});
            }
        }
    }
}

void Player::Update(const std::list<std::unique_ptr<Obstacle>> &obstacles) {
	if (isDead_) return;

	Move();
	CheckCollision(obstacles);

    int idx = static_cast<int>(currentMode_);
    if (objects_[idx]) {
	    objects_[idx]->SetTranslate(position_);
	    objects_[idx]->SetQuaternionRotate(quaternion_);
	    objects_[idx]->Update();
    }
}

void Player::Draw() {
    int idx = static_cast<int>(currentMode_);
	if (objects_[idx]) {
		objects_[idx]->Draw();
	}
}

void Player::UpdateCamera(Camera *camera) {
	Vector3 offset = { 0.0f, 2.0f, -12.0f };
	Vector3 rotatedOffset = MyMath::RotateVector(offset, quaternion_);
	Vector3 camPos = {
		position_.x + rotatedOffset.x,
		position_.y + rotatedOffset.y,
		position_.z + rotatedOffset.z
	};

	camera->SetTranslate(camPos);
	camera->SetRotate({ 0,0,0 });
	camera->SetQuaternion(quaternion_);
}

Vector3 Player::GetForwardVector() const {
	return MyMath::RotateVector({ 0.0f, 0.0f, 1.0f }, quaternion_);
}

void Player::SetRotation(const Vector3 &eulerRotation) {
	Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, eulerRotation.x);
	Quaternion qYaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, eulerRotation.y);
	Quaternion qRoll = MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, eulerRotation.z);

	quaternion_ = { 0.0f, 0.0f, 0.0f, 1.0f };
	quaternion_ = MyMath::Multiply(quaternion_, qPitch);
	quaternion_ = MyMath::Multiply(quaternion_, qYaw);
	quaternion_ = MyMath::Multiply(quaternion_, qRoll);
	quaternion_ = MyMath::Normalize(quaternion_);

    for (int i = 0; i < 3; ++i) {
	    if (objects_[i]) {
		    objects_[i]->SetQuaternionRotate(quaternion_);
		    objects_[i]->Update();
	    }
    }
}

void Player::OnCollision() {
	isDead_ = true;
    for (int i = 0; i < 3; ++i) {
	    if (objects_[i]) {
		    objects_[i].reset();
	    }
    }
}

void Player::TakeDamage(int damage) {
	if (isDead_) return;
	hp_ -= damage;
	if (hp_ <= 0) {
		hp_ = 0;
		OnCollision();
	}
}

void Player::Move() {
	auto input = Input::GetInstance();

    if (input->TriggerKey(DIK_1)) ChangeMode(PlayerMode::Fighter);
    if (input->TriggerKey(DIK_2)) ChangeMode(PlayerMode::Gerwalk);
    if (input->TriggerKey(DIK_3)) ChangeMode(PlayerMode::Battroid);

    PlayerModeParams& p = modeParams_[static_cast<int>(currentMode_)];

	const bool moveForward = input->PushKey(DIK_W);
	const bool moveBackward = input->PushKey(DIK_S);
	const bool moveRight = input->PushKey(DIK_D);
	const bool moveLeft = input->PushKey(DIK_A);
	const bool moveUp = input->PushKey(DIK_SPACE);
	const bool moveDown = input->PushKey(DIK_LSHIFT);

	float pitch = 0.0f;
	float yaw = 0.0f;
	float roll = 0.0f;

	// 姿勢制御（共通）
	if (input->PushKey(DIK_UP)) pitch -= p.pitchSpeed;
	if (input->PushKey(DIK_DOWN)) pitch += p.pitchSpeed;
	if (input->PushKey(DIK_RIGHT)) yaw += p.yawSpeed;
	if (input->PushKey(DIK_LEFT)) yaw -= p.yawSpeed;
	if (input->PushKey(DIK_E)) roll -= p.rollSpeed;
	if (input->PushKey(DIK_Q)) roll += p.rollSpeed;

	// マウス入力による回転（視点・機体回転）
	long mouseDX = input->GetMouseDeltaX();
	long mouseDY = input->GetMouseDeltaY();
	float mouseSensitivity = 0.002f; // マウス感度
	if (mouseDX != 0) {
		yaw += mouseDX * mouseSensitivity;
	}
	if (mouseDY != 0) {
		pitch += mouseDY * mouseSensitivity;
	}

	// --- ピッチ制限（上下180度制限：FPS視点風のジンバルロック） ---
	if (std::abs(pitch) > 0.0001f) {
		Quaternion qTestPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, pitch);
		Quaternion testQuat = MyMath::Multiply(quaternion_, qTestPitch);
		Vector3 testForward = MyMath::RotateVector({ 0.0f, 0.0f, 1.0f }, testQuat);
		// Y成分が±0.99f (約82度) を超えたらピッチ回転をキャンセルする
		if (testForward.y > 0.99f || testForward.y < -0.99f) {
			pitch = 0.0f;
		}
	}

	Vector3 localMove = { 0.0f, 0.0f, 0.0f };

	if (currentMode_ == PlayerMode::Fighter) {
		// ファイター形態：常に前進。平行移動はできない。旋回はピッチ/ロールがメイン
		localMove.z += 0.5f; // 基本前進推力
		if (moveForward) localMove.z += 1.0f; // 加速
		if (moveBackward) localMove.z -= 0.3f; // 減速
		
		// 旋回（A/Dキーでロール＋ヨー）
		if (moveRight) { yaw += p.yawSpeed * 0.5f; roll -= p.rollSpeed; }
		if (moveLeft) { yaw -= p.yawSpeed * 0.5f; roll += p.rollSpeed; }
		if (moveUp) pitch -= p.pitchSpeed;
		if (moveDown) pitch += p.pitchSpeed;

	} else if (currentMode_ == PlayerMode::Gerwalk) {
		// ガウォーク形態：全方位へのカニ歩き、ホバリング
		if (moveForward) localMove.z += 1.0f;
		if (moveBackward) localMove.z -= 1.0f;
		if (moveRight) localMove.x += 1.0f;
		if (moveLeft) localMove.x -= 1.0f;
		if (moveUp) localMove.y += 1.0f;
		if (moveDown) localMove.y -= 1.0f;
		
	} else if (currentMode_ == PlayerMode::Battroid) {
		// バトロイド形態：重力あり。上空へは上を向いて前進（加速）して飛ぶ。
		if (moveForward) localMove.z += 1.0f;
		if (moveBackward) localMove.z -= 1.0f;
		if (moveRight) localMove.x += 1.0f;
		if (moveLeft) localMove.x -= 1.0f;
		// カニ歩き的な上下移動（moveUp, moveDown）は廃止
	}

	Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, pitch);
	Quaternion qYaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw);
	Quaternion qRoll = MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, roll);

	quaternion_ = MyMath::Multiply(quaternion_, qPitch);
	quaternion_ = MyMath::Multiply(quaternion_, qYaw);
	quaternion_ = MyMath::Multiply(quaternion_, qRoll);
	quaternion_ = MyMath::Normalize(quaternion_);

	// ======== 自動水平復元処理 ========
	bool isInputRotation = (std::abs(pitch) > 0.0001f || std::abs(yaw) > 0.0001f || std::abs(roll) > 0.0001f);
	if (currentMode_ == PlayerMode::Fighter && !isInputRotation) {
		Vector3 fwd = GetForwardVector();
		fwd.y = 0.0f; // Y成分を0にして水平にする
		if (LengthSq(fwd) > 0.0001f) {
			fwd = NormalizeOrZero(fwd);
			Vector3 defaultFwd = { 0.0f, 0.0f, 1.0f };
			float dot = MyMath::Dot(defaultFwd, fwd);
			dot = std::clamp(dot, -1.0f, 1.0f);
			
			Quaternion targetQ;
			if (dot > 0.999f) {
				targetQ = { 0.0f, 0.0f, 0.0f, 1.0f };
			} else if (dot < -0.999f) {
				targetQ = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, 3.14159265f);
			} else {
				Vector3 axis = MyMath::Cross(defaultFwd, fwd);
				if (LengthSq(axis) > 0.000001f) {
					axis = NormalizeOrZero(axis);
					targetQ = MyMath::MakeAxisAngle(axis, std::acos(dot));
				} else {
					targetQ = quaternion_; // 念のため
				}
			}
			// 現在の姿勢から水平な姿勢へ補間（1フレームで2%近づく）
			quaternion_ = MyMath::Slerp(quaternion_, targetQ, 0.02f);
			quaternion_ = MyMath::Normalize(quaternion_);
		}
	}
	// ==================================

	if (LengthSq(localMove) > 0.0001f) {
		localMove = NormalizeOrZero(localMove);
		Vector3 worldMove = MyMath::RotateVector(localMove, quaternion_);
		velocity_ = Add(velocity_, Scale(worldMove, p.moveAcceleration));
		velocity_ = ClampLength(velocity_, p.maxMoveSpeed);
	} else {
		velocity_ = Scale(velocity_, p.moveDamping);
		if (LengthSq(velocity_) <= 0.00001f) {
			velocity_ = { 0.0f, 0.0f, 0.0f };
		}
	}

	// 重力処理（バトロイド形態のみ）
	if (currentMode_ == PlayerMode::Battroid) {
		float gravity = 0.008f; // 重力加速度
		velocity_.y -= gravity;
	}

	position_ = Add(position_, velocity_);
}

void Player::UpdateLockOnRotation(const Vector3& targetPos) {
	Vector3 toTarget = { targetPos.x - position_.x, targetPos.y - position_.y, targetPos.z - position_.z };
	if (LengthSq(toTarget) < 0.0001f) return;

	Vector3 targetForward = NormalizeOrZero(toTarget);
	Vector3 currentForward = GetForwardVector();

	float dot = MyMath::Dot(currentForward, targetForward);
	dot = std::clamp(dot, -1.0f, 1.0f);

	if (dot < 0.999f) {
		Vector3 axis = MyMath::Cross(currentForward, targetForward);
		if (LengthSq(axis) > 0.000001f) {
			axis = NormalizeOrZero(axis);
			float angle = std::acos(dot);
			
            // 形態ごとにロックオンの追従速度を変える
            float speed = 0.20f;
            if (currentMode_ == PlayerMode::Fighter) speed = 0.05f; // ファイターはあまり追尾しない
            if (currentMode_ == PlayerMode::Battroid) speed = 0.40f; // バトロイドは素早く正対する

			Quaternion qDiff = MyMath::MakeAxisAngle(axis, angle * speed);
			quaternion_ = MyMath::Multiply(quaternion_, qDiff);
			quaternion_ = MyMath::Normalize(quaternion_);
			
            for (int i = 0; i < 3; ++i) {
			    if (objects_[i]) {
				    objects_[i]->SetQuaternionRotate(quaternion_);
				    objects_[i]->Update();
			    }
            }
		}
	}
}

void Player::CheckCollision(const std::list<std::unique_ptr<Obstacle>> &obstacles) {
	Vector3 playerHalf = { 0.2f, 0.2f, 0.2f };

	for (const auto &obstacle : obstacles) {
		Vector3 obsPos = obstacle->GetPosition();
		Vector3 obsRot = obstacle->GetRotation();
		Vector3 obsHalf = obstacle->GetWorldHalfExtents();

		Matrix4x4 rotMat = MyMath::Multiply(MyMath::Multiply(MyMath::MakeRoteXMatrix(obsRot.x), MyMath::MakeRotateYMatrix(obsRot.y)), MyMath::MakeRotateZMatrix(obsRot.z));
		Matrix4x4 invRotMat = MyMath::Transpose(rotMat);

		Vector3 diff = { position_.x - obsPos.x, position_.y - obsPos.y, position_.z - obsPos.z };
		Vector3 localDiff = MyMath::Transform(diff, invRotMat);

		float dx = localDiff.x;
		float dy = localDiff.y;
		float dz = localDiff.z;

		float halfWidth = playerHalf.x + obsHalf.x;
		float halfHeight = playerHalf.y + obsHalf.y;
		float halfDepth = playerHalf.z + obsHalf.z;

		if (obstacle->IsStageBounds()) {
			Vector3 localPushOut = { 0.0f, 0.0f, 0.0f };

			if (dx > obsHalf.x - playerHalf.x) {
				localPushOut.x = (obsHalf.x - playerHalf.x) - dx;
			} else if (dx < -(obsHalf.x - playerHalf.x)) {
				localPushOut.x = -(obsHalf.x - playerHalf.x) - dx;
			}

			if (dy > obsHalf.y - playerHalf.y) {
				localPushOut.y = (obsHalf.y - playerHalf.y) - dy;
			} else if (dy < -(obsHalf.y - playerHalf.y)) {
				localPushOut.y = -(obsHalf.y - playerHalf.y) - dy;
			}

			if (dz > obsHalf.z - playerHalf.z) {
				localPushOut.z = (obsHalf.z - playerHalf.z) - dz;
			} else if (dz < -(obsHalf.z - playerHalf.z)) {
				localPushOut.z = -(obsHalf.z - playerHalf.z) - dz;
			}

			if (localPushOut.x != 0.0f || localPushOut.y != 0.0f || localPushOut.z != 0.0f) {
				Vector3 worldPushOut = MyMath::Transform(localPushOut, rotMat);
				position_.x += worldPushOut.x;
				position_.y += worldPushOut.y;
				position_.z += worldPushOut.z;
			}
		} 
		else {
			float overlapX = halfWidth - std::abs(dx);
			float overlapY = halfHeight - std::abs(dy);
			float overlapZ = halfDepth - std::abs(dz);

			if (overlapX > 0.0f && overlapY > 0.0f && overlapZ > 0.0f) {
				Vector3 localPushOut = { 0.0f, 0.0f, 0.0f };

				if (overlapX < overlapY && overlapX < overlapZ) {
					localPushOut.x = (dx > 0.0f) ? overlapX : -overlapX;
				} else if (overlapY < overlapX && overlapY < overlapZ) {
					localPushOut.y = (dy > 0.0f) ? overlapY : -overlapY;
				} else {
					localPushOut.z = (dz > 0.0f) ? overlapZ : -overlapZ;
				}

				Vector3 worldPushOut = MyMath::Transform(localPushOut, rotMat);
				position_.x += worldPushOut.x;
				position_.y += worldPushOut.y;
				position_.z += worldPushOut.z;
			}
		}
	}
}
