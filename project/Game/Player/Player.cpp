#include "Player.h"
#include "3D/Object3dCommon.h"
#include "Game/obstacle/Obstacle.h"

void Player::Initialize(const std::string &modelName) {
	object_ = std::make_unique<Object3d>();
	object_->Initialize(Object3dCommon::GetInstance());
	object_->SetModel(modelName);

	if (object_->GetModel()) {
		object_->GetModel()->SetColor({ 0.2f, 0.5f, 1.0f, 1.0f }); // 青色
	}


	object_->SetScale({ 0.2f, 0.2f, 0.2f });

	position_ = { 0.0f, 0.0f, 0.0f };
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
	// 機体から見たカメラの相対位置（真後ろに10m、上に3m）
	Vector3 offset = { 0.0f, 0.0f, -10.0f };

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

	// ==========================================
	// 1. 回転の処理 (矢印キーと Q/E)
	// ==========================================
	float pitch = 0.0f;
	float yaw = 0.0f;
	float roll = 0.0f;

	if (input->PushKey(DIK_UP))    pitch -= rotSpeed_; // 機首下げ
	if (input->PushKey(DIK_DOWN))  pitch += rotSpeed_; // 機首上げ
	if (input->PushKey(DIK_RIGHT)) yaw += rotSpeed_; // 右旋回
	if (input->PushKey(DIK_LEFT))  yaw -= rotSpeed_; // 左旋回
	if (input->PushKey(DIK_E))     roll -= rotSpeed_; // 右ロール
	if (input->PushKey(DIK_Q))     roll += rotSpeed_; // 左ロール


	// 各軸ごとの回転クォータニオンを作成して合成
	Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, pitch);
	Quaternion qYaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw);
	Quaternion qRoll = MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, roll);

	quaternion_ = MyMath::Multiply(quaternion_, qPitch);
	quaternion_ = MyMath::Multiply(quaternion_, qYaw);
	quaternion_ = MyMath::Multiply(quaternion_, qRoll);
	quaternion_ = MyMath::Normalize(quaternion_);

	// ==========================================
	// 2. 移動の処理 (Wキーで前進、Sキーでブレーキ/後退)
	// ==========================================
	Vector3 moveInput = { 0.0f, 0.0f, 0.0f };

	// ① どのキーが押されているかチェック
	if (input->PushKey(DIK_W)) moveInput.z += 1.0f;
	if (input->PushKey(DIK_S)) moveInput.z -= 1.0f;
	if (input->PushKey(DIK_D)) moveInput.x += 1.0f;
	if (input->PushKey(DIK_A)) moveInput.x -= 1.0f;
	if (input->PushKey(DIK_SPACE)) moveInput.y += 1.0f;    // 上昇 (Spaceに変更)
	if (input->PushKey(DIK_LSHIFT)) moveInput.y -= 1.0f;   // 下降 (LShiftに変更)

	// ② 斜め移動の加速を防ぐ（長さを1にする）
	if (moveInput.x != 0.0f || moveInput.y != 0.0f || moveInput.z != 0.0f) {
		moveInput = MyMath::Normalize(moveInput);
	}

	// ③ 入力方向を、自機の向いている方向に合わせて曲げる
	Vector3 velocity = MyMath::RotateVector(moveInput, quaternion_);

	// ④ スピードを掛けて足し込む
	position_.x += velocity.x * speed_;
	position_.y += velocity.y * speed_;
	position_.z += velocity.z * speed_;
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
