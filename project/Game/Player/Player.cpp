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
	object_ = std::make_unique<Object3d>();
	object_->Initialize(Object3dCommon::GetInstance());
	object_->SetModel(modelName);

	if (object_->GetModel()) {
		object_->GetModel()->SetColor({ 0.2f, 0.5f, 1.0f, 1.0f }); // 青色
	}


	object_->SetScale({ 0.2f, 0.2f, 0.2f });

	position_ = { 0.0f, 0.0f, 0.0f };
	velocity_ = { 0.0f, 0.0f, 0.0f };
	quaternion_ = { 0.0f, 0.0f, 0.0f, 1.0f };
	hp_ = 3;
	isDead_ = false;
}

void Player::Update(const std::list<std::unique_ptr<Obstacle>> &obstacles) {
	// 死んでいたら何もしない（モデルがないのでアクセス禁止）
	if (isDead_ || !object_) return;

	// 1. 移動と回転
	Move();

	// 2. 当たり判定（押し出し）
	CheckCollision(obstacles);

	// 3. Object3dに座標と回転を適用
	object_->SetTranslate(position_);
	object_->SetQuaternionRotate(quaternion_);
	object_->Update();

}

void Player::Draw() {
	if (object_) {
		object_->Draw();
	}
}

void Player::UpdateCamera(Camera *camera) {
	// 機体から見たカメラの相対位置（真後ろ、少し上）
	Vector3 offset = { 0.0f, 2.0f, -12.0f };

	// このオフセットを、機体の傾きに合わせて回転させる
	Vector3 rotatedOffset = MyMath::RotateVector(offset, quaternion_);

	// カメラの最終的な座標 ＝ プレイヤーの座標 ＋ 回転させたオフセット
	Vector3 camPos = {
		position_.x + rotatedOffset.x,
		position_.y + rotatedOffset.y,
		position_.z + rotatedOffset.z
	};

	camera->SetTranslate(camPos);

	// カメラの向きも機体と全く同じにする
	camera->SetRotate({ 0,0,0 }); // オイラー角のリセット(ハイブリッド対応のため)
	camera->SetQuaternion(quaternion_); // ※Camera.hに実装した関数
}

Vector3 Player::GetForwardVector() const {
	return MyMath::RotateVector({ 0.0f, 0.0f, 1.0f }, quaternion_);
}

void Player::SetRotation(const Vector3 &eulerRotation) {
	// X, Y, Z の各軸の回転クォータニオンを作成
	Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, eulerRotation.x);
	Quaternion qYaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, eulerRotation.y);
	Quaternion qRoll = MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, eulerRotation.z);

	// Update()と同じ順番で掛け合わせて合成する
	quaternion_ = { 0.0f, 0.0f, 0.0f, 1.0f }; // 一旦リセット
	quaternion_ = MyMath::Multiply(quaternion_, qPitch);
	quaternion_ = MyMath::Multiply(quaternion_, qYaw);
	quaternion_ = MyMath::Multiply(quaternion_, qRoll);
	quaternion_ = MyMath::Normalize(quaternion_);

	if (object_) {
		object_->SetQuaternionRotate(quaternion_);
		object_->Update();
	}
}

void Player::OnCollision() {
	isDead_ = true;

	// 3Dモデルのリソースを解放して「消去」する！
	if (object_) {
		// オブジェクトのスマートポインタを nullptr にする
		// これで裏側で描画リソースが解放され、二度と描画されなくなります
		object_.reset();
	}
}

void Player::TakeDamage(int damage) {
	if (isDead_) {
		return;
	}

	hp_ -= damage;
	if (hp_ <= 0) {
		hp_ = 0;
		OnCollision();
	}
}

void Player::Move() {
	auto input = Input::GetInstance();

	const bool moveForward = input->PushKey(DIK_W);
	const bool moveBackward = input->PushKey(DIK_S);
	const bool moveRight = input->PushKey(DIK_D);
	const bool moveLeft = input->PushKey(DIK_A);
	const bool moveUp = input->PushKey(DIK_SPACE);
	const bool moveDown = input->PushKey(DIK_LSHIFT);

	// ==========================================
	// 1. 向き調整。移動とは分けて、スクランブル系の自由移動に寄せる。
	// ==========================================
	float pitch = 0.0f;
	float yaw = 0.0f;
	float roll = 0.0f;

	if (input->PushKey(DIK_UP)) {
		pitch -= pitchSpeed_; // 機首下げ
	}
	if (input->PushKey(DIK_DOWN)) {
		pitch += pitchSpeed_; // 機首上げ
	}
	if (input->PushKey(DIK_RIGHT)) {
		yaw += yawSpeed_;
	}
	if (input->PushKey(DIK_LEFT)) {
		yaw -= yawSpeed_;
	}
	if (moveRight && !moveLeft) {
		yaw += yawSpeed_ * 0.75f;
		roll -= rollSpeed_ * 0.35f;
	}
	if (moveLeft && !moveRight) {
		yaw -= yawSpeed_ * 0.75f;
		roll += rollSpeed_ * 0.35f;
	}
	if (moveUp && !moveDown) {
		pitch += pitchSpeed_ * 0.75f;
	}
	if (moveDown && !moveUp) {
		pitch -= pitchSpeed_ * 0.75f;
	}
	if (input->PushKey(DIK_E)) {
		roll -= rollSpeed_; // 右ロール
	}
	if (input->PushKey(DIK_Q)) {
		roll += rollSpeed_; // 左ロール
	}


	// 各軸ごとの回転クォータニオンを作成して合成
	Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, pitch);
	Quaternion qYaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw);
	Quaternion qRoll = MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, roll);

	quaternion_ = MyMath::Multiply(quaternion_, qPitch);
	quaternion_ = MyMath::Multiply(quaternion_, qYaw);
	quaternion_ = MyMath::Multiply(quaternion_, qRoll);
	quaternion_ = MyMath::Normalize(quaternion_);

	// ==========================================
	// 2. 3Dアクション寄りの慣性移動。押した方向へ動き、離すと減速して止まる。
	// ==========================================
	Vector3 localMove = { 0.0f, 0.0f, 0.0f };
	if (moveForward) {
		localMove.z += 1.0f;
	}
	if (moveBackward) {
		localMove.z -= 1.0f;
	}
	if (moveRight) {
		localMove.x += 1.0f;
	}
	if (moveLeft) {
		localMove.x -= 1.0f;
	}
	if (moveUp) {
		localMove.y += 1.0f;
	}
	if (moveDown) {
		localMove.y -= 1.0f;
	}

	if (LengthSq(localMove) > 0.0001f) {
		localMove = NormalizeOrZero(localMove);
		Vector3 worldMove = MyMath::RotateVector(localMove, quaternion_);
		velocity_ = Add(velocity_, Scale(worldMove, moveAcceleration_));
		velocity_ = ClampLength(velocity_, maxMoveSpeed_);
	} else {
		velocity_ = Scale(velocity_, moveDamping_);
		if (LengthSq(velocity_) <= 0.00001f) {
			velocity_ = { 0.0f, 0.0f, 0.0f };
		}
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
			
			// ロックオン対象へ強制的に向かせる (1フレームあたり20%の補間)
			Quaternion qDiff = MyMath::MakeAxisAngle(axis, angle * 0.20f);
			quaternion_ = MyMath::Multiply(quaternion_, qDiff);
			quaternion_ = MyMath::Normalize(quaternion_);
			
			if (object_) {
				object_->SetQuaternionRotate(quaternion_);
				object_->Update();
			}
		}
	}
}

void Player::CheckCollision(const std::list<std::unique_ptr<Obstacle>> &obstacles) {

	// =========================================================
	   // Blenderと完全連動する障宴物との当たり判定（AABB押し出し）
	   // =========================================================

	// プレイヤーの実際の当たり判定サイズ (extents)
	Vector3 playerHalf = { 0.2f, 0.2f, 0.2f }; // 0.4 x 0.4 x 0.4 の半分

	for (const auto &obstacle : obstacles) {
		Vector3 obsPos = obstacle->GetPosition();
		Vector3 obsRot = obstacle->GetRotation();
		Vector3 obsHalf = obstacle->GetWorldHalfExtents();

		// 障害物の回転行列を作成
		Matrix4x4 rotMat = MyMath::Multiply(MyMath::Multiply(MyMath::MakeRoteXMatrix(obsRot.x), MyMath::MakeRotateYMatrix(obsRot.y)), MyMath::MakeRotateZMatrix(obsRot.z));
		// 逆回転行列（転置行列）
		Matrix4x4 invRotMat = MyMath::Transpose(rotMat);

		// プレイヤーから障害物への相対ベクトル（ワールド空間）
		Vector3 diff = { position_.x - obsPos.x, position_.y - obsPos.y, position_.z - obsPos.z };

		// 相対ベクトルを障害物のローカル空間に変換
		Vector3 localDiff = MyMath::Transform(diff, invRotMat);

		// お互いの中心の距離 (ローカル空間)
		float dx = localDiff.x;
		float dy = localDiff.y;
		float dz = localDiff.z;

		// 衝突判定の基準値（お互いのサイズの半分を足したもの）
		float halfWidth = playerHalf.x + obsHalf.x;
		float halfHeight = playerHalf.y + obsHalf.y;
		float halfDepth = playerHalf.z + obsHalf.z;

		// StageBoundsの場合は内側に留める（反転当たり判定）
		if (obstacle->IsStageBounds()) {
			Vector3 localPushOut = { 0.0f, 0.0f, 0.0f };

			// X軸の境界チェック
			if (dx > obsHalf.x - playerHalf.x) {
				localPushOut.x = (obsHalf.x - playerHalf.x) - dx;
			} else if (dx < -(obsHalf.x - playerHalf.x)) {
				localPushOut.x = -(obsHalf.x - playerHalf.x) - dx;
			}

			// Y軸の境界チェック
			if (dy > obsHalf.y - playerHalf.y) {
				localPushOut.y = (obsHalf.y - playerHalf.y) - dy;
			} else if (dy < -(obsHalf.y - playerHalf.y)) {
				localPushOut.y = -(obsHalf.y - playerHalf.y) - dy;
			}

			// Z軸の境界チェック
			if (dz > obsHalf.z - playerHalf.z) {
				localPushOut.z = (obsHalf.z - playerHalf.z) - dz;
			} else if (dz < -(obsHalf.z - playerHalf.z)) {
				localPushOut.z = -(obsHalf.z - playerHalf.z) - dz;
			}

			// 押し出しベクトルをワールド空間に戻す（移動が発生した場合のみ）
			if (localPushOut.x != 0.0f || localPushOut.y != 0.0f || localPushOut.z != 0.0f) {
				Vector3 worldPushOut = MyMath::Transform(localPushOut, rotMat);
				position_.x += worldPushOut.x;
				position_.y += worldPushOut.y;
				position_.z += worldPushOut.z;
			}
		} 
		// 通常の障害物の場合は外側に押し出す（既存の処理）
		else {
			// どれくらい食い込んでいるか（オーバーラップ量）
			float overlapX = halfWidth - std::abs(dx);
			float overlapY = halfHeight - std::abs(dy);
			float overlapZ = halfDepth - std::abs(dz);

			// 3軸全てが重なっていたら「衝突している」
			if (overlapX > 0.0f && overlapY > 0.0f && overlapZ > 0.0f) {
				Vector3 localPushOut = { 0.0f, 0.0f, 0.0f };

				// 一番食い込みが「浅い」方向へプレイヤーを押し出す（壁めり込み防止）
				if (overlapX < overlapY && overlapX < overlapZ) {
					localPushOut.x = (dx > 0.0f) ? overlapX : -overlapX;
				} else if (overlapY < overlapX && overlapY < overlapZ) {
					localPushOut.y = (dy > 0.0f) ? overlapY : -overlapY;
				} else {
					localPushOut.z = (dz > 0.0f) ? overlapZ : -overlapZ;
				}

				// 押し出しベクトルをワールド空間に戻す
				Vector3 worldPushOut = MyMath::Transform(localPushOut, rotMat);

				// プレイヤーの座標に適用
				position_.x += worldPushOut.x;
				position_.y += worldPushOut.y;
				position_.z += worldPushOut.z;
			}
		}
	}

}
