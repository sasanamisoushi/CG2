#include "Player.h"
#include "3D/Object3dCommon.h"
#include "3D/ModelManager.h"
#include "Game/obstacle/Obstacle.h"
#include <algorithm>
#include <cmath>

namespace {
	constexpr const char *kDefaultPlayerBoxModelName = "PlayerBox";

	bool UsesNaturalPlayerModelScale(const std::string &modelName) {
		return modelName != kDefaultPlayerBoxModelName;
	}

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

	float Dot(const Vector3 &a, const Vector3 &b) {
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	float Abs(float value) {
		return value < 0.0f ? -value : value;
	}

	Vector3 AbsVector(const Vector3 &value) {
		return { Abs(value.x), Abs(value.y), Abs(value.z) };
	}

	float ProjectionRadius(const OBB &obb, const Vector3 &axis) {
		return obb.size.x * Abs(Dot(obb.orientations[0], axis)) +
			obb.size.y * Abs(Dot(obb.orientations[1], axis)) +
			obb.size.z * Abs(Dot(obb.orientations[2], axis));
	}

	float GetAxisSize(const Vector3 &value, int axisIndex) {
		if (axisIndex == 0) return value.x;
		if (axisIndex == 1) return value.y;
		return value.z;
	}

	Vector3 ComposeFromAxes(const OBB &basis, const Vector3 &amount) {
		return Add(Add(Scale(basis.orientations[0], amount.x), Scale(basis.orientations[1], amount.y)), Scale(basis.orientations[2], amount.z));
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

	Quaternion MakeNoRollLookQuaternion(const Vector3 &forward) {
		Vector3 normalizedForward = NormalizeOrZero(forward);
		if (LengthSq(normalizedForward) <= 0.0001f) {
			normalizedForward = { 0.0f, 0.0f, 1.0f };
		}

		const float clampedY = std::clamp(normalizedForward.y, -1.0f, 1.0f);
		const float pitch = -std::asin(clampedY);
		const float yaw = std::atan2(normalizedForward.x, normalizedForward.z);
		Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, pitch);
		Quaternion qYaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw);
		return MyMath::Normalize(MyMath::Multiply(qYaw, qPitch));
	}

	Vector3 MakeForwardFromFlatAndPitch(const Vector3 &flatForward, float pitch) {
		Vector3 normalizedFlat = NormalizeOrZero({ flatForward.x, 0.0f, flatForward.z });
		if (LengthSq(normalizedFlat) <= 0.0001f) {
			normalizedFlat = { 0.0f, 0.0f, 1.0f };
		}

		const float yaw = std::atan2(normalizedFlat.x, normalizedFlat.z);
		Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, pitch);
		Quaternion qYaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw);
		return MyMath::RotateVector({ 0.0f, 0.0f, 1.0f }, MyMath::Normalize(MyMath::Multiply(qYaw, qPitch)));
	}

	Quaternion MakeYawQuaternionFromForward(const Vector3 &forward) {
		Vector3 flatForward = NormalizeOrZero({ forward.x, 0.0f, forward.z });
		if (LengthSq(flatForward) <= 0.0001f) {
			flatForward = { 0.0f, 0.0f, 1.0f };
		}

		const float yaw = std::atan2(flatForward.x, flatForward.z);
		return MyMath::Normalize(MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw));
	}

	float CameraPitchFromForward(const Vector3 &forward) {
		Vector3 normalizedForward = NormalizeOrZero(forward);
		if (LengthSq(normalizedForward) <= 0.0001f) {
			return 0.0f;
		}
		return std::clamp(-std::asin(std::clamp(normalizedForward.y, -1.0f, 1.0f)), -1.2f, 1.2f);
	}
}

void Player::Initialize(const std::string &modelName) {
	modelName_ = modelName;
	
	if (modelName.find("vf-15c") != std::string::npos) {
		modelScale_ = { 0.08f, 0.08f, 0.08f }; // スケールを0.08に調整
	} else {
		modelScale_ = { 1.0f, 1.0f, 1.0f };
	}
	
	// モデルマネージャに読み込みを指示
	ModelManager::GetInstance()->LoadModel(modelName);
	
	const bool usesNaturalScale = UsesNaturalPlayerModelScale(modelName_);

	object_ = std::make_unique<Object3d>();
	object_->Initialize(Object3dCommon::GetInstance());
	object_->SetModel(modelName);

	if (object_->GetModel()) {
		object_->GetModel()->SetColor(usesNaturalScale ? Vector4{ 1.0f, 1.0f, 1.0f, 1.0f } : Vector4{ 0.2f, 0.5f, 1.0f, 1.0f });
	}
	currentDrawScale_ = modelScale_;
	targetDrawScale_ = modelScale_;
	object_->SetScale(currentDrawScale_);

	position_ = { 0.0f, 0.0f, 0.0f };
	velocity_ = { 0.0f, 0.0f, 0.0f };
	quaternion_ = { 0.0f, 0.0f, 0.0f, 1.0f };
	hp_ = 3;
	isDead_ = false;

	// アニメーションデータのロード
	if (modelName.find("vf-15c") != std::string::npos) {
		std::string dir = "resources/vf-15c";
		std::string file = "scene.gltf";
		animationData_ = LoadAnimationFile(dir, file);
		
		Node rootNode;
		if (object_ && object_->GetModel()) {
			rootNode = object_->GetModel()->GetModelData().rootNode;
		} else {
			rootNode = Model::LoadNodeHierarchy(dir, file);
		}
		skeleton_ = CreateSkeleton(rootNode);
		
		if (!skeleton_.joints.empty()) {
			enableSkinning_ = true;
			if (object_ && object_->GetModel()) {
				object_->skinCluster = object_->GetModel()->CreateSkinCluster(skeleton_);
			}
		}
	}

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

    boosterEffect_ = std::make_unique<BoosterEffect>();
    boosterEffect_->Initialize();
}

void Player::ChangeMode(PlayerMode newMode) {
	if (newMode != PlayerMode::Fighter) {
		const Vector3 currentForward = MyMath::RotateVector({ 0.0f, 0.0f, 1.0f }, quaternion_);
		cameraPitch_ = CameraPitchFromForward(currentForward);
		quaternion_ = MakeYawQuaternionFromForward(currentForward);
	}

    currentMode_ = newMode;
    const bool usesNaturalScale = UsesNaturalPlayerModelScale(modelName_);
    
    // 形態ごとの見た目の変更（スケールを変えて滑らかに変形させる）
    if (usesNaturalScale) {
        // vf-15cのようなちゃんとしたモデルの場合は、勝手にスケールを歪ませて形を崩さない
        targetDrawScale_ = modelScale_;
    } else {
        // 仮モデルの場合はスケールで形態を表現
	    if (currentMode_ == PlayerMode::Fighter) {
	        targetDrawScale_ = { modelScale_.x * 1.0f, modelScale_.y * 0.5f, modelScale_.z * 1.5f };
	    } else if (currentMode_ == PlayerMode::Gerwalk) {
	        targetDrawScale_ = { modelScale_.x * 1.0f, modelScale_.y * 1.0f, modelScale_.z * 1.0f };
	    } else if (currentMode_ == PlayerMode::Battroid) {
	        targetDrawScale_ = { modelScale_.x * 0.8f, modelScale_.y * 1.5f, modelScale_.z * 0.8f };
	    }
    }
}

void Player::Update(const std::list<std::unique_ptr<Obstacle>> &obstacles) {
	if (isDead_) return;

	Move();
	CheckCollision(obstacles);

	if (boosterEffect_) {
		auto input = Input::GetInstance();
		PlayerModeParams& p = modeParams_[static_cast<int>(currentMode_)];
		float speedVal = Length(velocity_);
		float speedRatio = speedVal / (std::max)(0.0001f, p.maxMoveSpeed);
		bool isAccelerating = input->PushKey(DIK_W);
		boosterEffect_->Update(position_, quaternion_, static_cast<int>(currentMode_), speedRatio, isAccelerating);
	}

    UpdateModel();
}

void Player::UpdateModel() {
    // アニメーションの更新と適用
    if (animationData_.duration > 0.0f && !skeleton_.joints.empty()) {
        if (!isAnimDebugActive_) {
            if (currentMode_ == PlayerMode::Fighter) {
                targetAnimationTime_ = 0.0f;
            } else if (currentMode_ == PlayerMode::Gerwalk) {
                targetAnimationTime_ = 1.71f;  // Frame 41 (Gerwalk pose matching 02:11 in Sketchfab)
            } else {
                targetAnimationTime_ = 5.0f;   // Frame 120 (Battroid)
            }

            // 補間でなめらかに変形させる
            float diff = targetAnimationTime_ - animationTime_;
            animationTime_ += diff * 0.1f; // 毎フレーム10%ずつ近づく
        } else {
            // デバッグ時は直接指定
            animationTime_ = targetAnimationTime_;
        }

        ApplyAnimation(skeleton_, animationData_, animationTime_);
        ::Update(skeleton_);

        if (enableSkinning_ && object_ && object_->GetModel()) {
            object_->GetModel()->UpdateSkinCluster(object_->skinCluster, skeleton_);
        }
    }

    // スケールの滑らかな補間（アニメーション変形のように見せる）
    currentDrawScale_.x += (targetDrawScale_.x - currentDrawScale_.x) * 0.2f;
    currentDrawScale_.y += (targetDrawScale_.y - currentDrawScale_.y) * 0.2f;
    currentDrawScale_.z += (targetDrawScale_.z - currentDrawScale_.z) * 0.2f;

    if (object_) {
	    object_->SetScale(currentDrawScale_);
	    object_->SetTranslate(position_);
	    object_->SetQuaternionRotate(quaternion_);
	    object_->Update();
    }
}

void Player::Draw() {
	if (object_) {
		object_->Draw();
	}
	if (boosterEffect_) {
		boosterEffect_->Draw();
	}
}

Vector3 Player::GetWorldHalfExtents() const {
	if (!object_ || !object_->GetModel()) {
		return { 0.2f, 0.2f, 0.2f };
	}

	if (modelName_.find("vf-15c") != std::string::npos) {
		const float scale = modelScale_.x; // 0.08f
		if (currentMode_ == PlayerMode::Fighter) {
			return { 2.8f * scale, 0.8f * scale, 3.0f * scale }; // { 0.224f, 0.064f, 0.24f }
		} else if (currentMode_ == PlayerMode::Gerwalk) {
			return { 2.8f * scale, 2.8f * scale, 2.8f * scale }; // { 0.224f, 0.224f, 0.224f }
		} else {
			return { 2.2f * scale, 4.5f * scale, 2.2f * scale }; // { 0.176f, 0.36f, 0.176f }
		}
	}

	const Vector3 localHalf = object_->GetModel()->GetHalfExtents();
	const Vector3 scale = AbsVector(object_->GetScale());
	return { localHalf.x * scale.x, localHalf.y * scale.y, localHalf.z * scale.z };
}

float Player::GetCollisionRadius() const {
	const Vector3 halfExtents = GetWorldHalfExtents();
	float radius = halfExtents.x;
	if (halfExtents.y > radius) radius = halfExtents.y;
	if (halfExtents.z > radius) radius = halfExtents.z;
	return radius;
}

OBB Player::GetOBB() const {
	OBB obb{};
	obb.center = position_;
	obb.size = GetWorldHalfExtents();

	Matrix4x4 rotationMatrix = MyMath::MakeRotateMatrix(quaternion_);
	obb.orientations[0] = MyMath::Normalize(Vector3{ rotationMatrix.m[0][0], rotationMatrix.m[0][1], rotationMatrix.m[0][2] });
	obb.orientations[1] = MyMath::Normalize(Vector3{ rotationMatrix.m[1][0], rotationMatrix.m[1][1], rotationMatrix.m[1][2] });
	obb.orientations[2] = MyMath::Normalize(Vector3{ rotationMatrix.m[2][0], rotationMatrix.m[2][1], rotationMatrix.m[2][2] });

	if (object_ && object_->GetModel()) {
		Vector3 localCenter = object_->GetModel()->GetBoundsCenter();
		
		// vf-15cモデルの場合はアニメーションによる変形を考慮して、形態ごとの適切なオフセットを設定する
		if (modelName_.find("vf-15c") != std::string::npos) {
			if (currentMode_ == PlayerMode::Fighter) {
				localCenter.y -= 0.0f;
			} else if (currentMode_ == PlayerMode::Gerwalk) {
				localCenter.y -= 0.8f; // 中心を下げて脚部をカバーする
			} else {
				localCenter.y -= 1.2f; // 中心を下げて人型の長身をカバーする
			}
		}

		const Vector3 scale = object_->GetScale();
		const Vector3 scaledCenter = { localCenter.x * scale.x, localCenter.y * scale.y, localCenter.z * scale.z };
		const Vector3 rotatedCenter = MyMath::Transform(scaledCenter, rotationMatrix);
		obb.center = Add(position_, rotatedCenter);
	}

	return obb;
}

void Player::UpdateCamera(Camera *camera, const Vector3 *targetPos) {
	Vector3 playerForward = NormalizeOrZero(GetForwardVector());
	if (LengthSq(playerForward) <= 0.0001f) {
		playerForward = { 0.0f, 0.0f, 1.0f };
	}
	Vector3 flatForward = NormalizeOrZero({ playerForward.x, 0.0f, playerForward.z });
	if (LengthSq(flatForward) <= 0.0001f) {
		flatForward = { 0.0f, 0.0f, 1.0f };
	}

	Vector3 cameraDirection;
	Vector3 lookTarget = GetOBB().center;

	if (targetPos) {
		// ロックオン中の場合
		Vector3 toTarget = { targetPos->x - lookTarget.x, targetPos->y - lookTarget.y, targetPos->z - lookTarget.z };
		Vector3 flatToTarget = { toTarget.x, 0.0f, toTarget.z };
		float distToTargetSq = LengthSq(flatToTarget);
		
		Vector3 targetDir;
		if (distToTargetSq > 0.0001f) {
			targetDir = NormalizeOrZero(flatToTarget);
		} else {
			targetDir = flatForward;
		}

		// 急激なカメラ回転を防ぐため、プレイヤーの前方方向とターゲットへの方向をブレンドする
		cameraDirection = {
			flatForward.x * 0.5f + targetDir.x * 0.5f,
			0.0f,
			flatForward.z * 0.5f + targetDir.z * 0.5f
		};
		cameraDirection = NormalizeOrZero(cameraDirection);
		if (LengthSq(cameraDirection) <= 0.0001f) {
			cameraDirection = flatForward;
		}

		// 高低差の追従
		float pitchBlend = std::clamp(toTarget.y / 50.0f, -0.5f, 0.5f);
		cameraDirection.y = pitchBlend;
		cameraDirection = NormalizeOrZero(cameraDirection);

		// 注視点をプレイヤーとターゲットの中間点にする
		float t = 0.35f;
		Vector3 blendTarget = {
			lookTarget.x + toTarget.x * t,
			lookTarget.y + toTarget.y * t,
			lookTarget.z + toTarget.z * t
		};
		
		// 最大注視距離制限
		float maxLookAhead = 15.0f;
		Vector3 toBlend = { blendTarget.x - lookTarget.x, blendTarget.y - lookTarget.y, blendTarget.z - lookTarget.z };
		if (LengthSq(toBlend) > maxLookAhead * maxLookAhead) {
			toBlend = NormalizeOrZero(toBlend);
			lookTarget = {
				lookTarget.x + toBlend.x * maxLookAhead,
				lookTarget.y + toBlend.y * maxLookAhead,
				lookTarget.z + toBlend.z * maxLookAhead
			};
		} else {
			lookTarget = blendTarget;
		}
	} else {
		// ロックオンしていない場合
		Vector3 rawDirection = {
			playerForward.x,
			std::clamp(playerForward.y, -0.75f, 0.75f),
			playerForward.z
		};
		if (LengthSq({ rawDirection.x, 0.0f, rawDirection.z }) <= 0.0001f) {
			rawDirection.x = flatForward.x * 0.35f;
			rawDirection.z = flatForward.z * 0.35f;
		}
		cameraDirection = NormalizeOrZero(rawDirection);
		if (LengthSq(cameraDirection) <= 0.0001f) {
			cameraDirection = flatForward;
		}
	}

	const float backDistance = 12.0f;
	const float heightOffset = 2.5f;
	
	// カメラの配置
	Vector3 cameraBackOffset = Scale(cameraDirection, -backDistance);
	Vector3 playerOBBCenter = GetOBB().center;
	Vector3 camPos = {
		playerOBBCenter.x + cameraBackOffset.x,
		playerOBBCenter.y + heightOffset,
		playerOBBCenter.z + cameraBackOffset.z
	};

	Vector3 lookForward = NormalizeOrZero({
		lookTarget.x - camPos.x,
		lookTarget.y - camPos.y,
		lookTarget.z - camPos.z
	});
	if (LengthSq(lookForward) <= 0.0001f) {
		lookForward = flatForward;
	}
	Quaternion cameraRotation = MakeNoRollLookQuaternion(lookForward);

	camera->SetTranslate(camPos);
	camera->SetRotate({ 0,0,0 });
	camera->SetQuaternion(cameraRotation);
}

Vector3 Player::GetForwardVector() const {
	Vector3 bodyForward = MyMath::RotateVector({ 0.0f, 0.0f, 1.0f }, quaternion_);
	if (currentMode_ == PlayerMode::Fighter) {
		return bodyForward;
	}

	return MakeForwardFromFlatAndPitch(bodyForward, cameraPitch_);
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
	if (currentMode_ != PlayerMode::Fighter) {
		const Vector3 currentForward = MyMath::RotateVector({ 0.0f, 0.0f, 1.0f }, quaternion_);
		cameraPitch_ = CameraPitchFromForward(currentForward);
		quaternion_ = MakeYawQuaternionFromForward(currentForward);
	}

	if (object_) {
		object_->SetQuaternionRotate(quaternion_);
		object_->Update();
	}
}

void Player::OnCollision() {
	isDead_ = true;
	if (object_) {
		object_.reset();
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
	float cameraPitchDelta = 0.0f;
	const bool usesDetachedCameraPitch = currentMode_ != PlayerMode::Fighter;

	// 姿勢制御（共通）
	if (input->PushKey(DIK_UP)) {
		if (usesDetachedCameraPitch) {
			cameraPitchDelta -= p.pitchSpeed;
		} else {
			pitch -= p.pitchSpeed;
		}
	}
	if (input->PushKey(DIK_DOWN)) {
		if (usesDetachedCameraPitch) {
			cameraPitchDelta += p.pitchSpeed;
		} else {
			pitch += p.pitchSpeed;
		}
	}
	if (input->PushKey(DIK_RIGHT)) yaw += p.yawSpeed;
	if (input->PushKey(DIK_LEFT)) yaw -= p.yawSpeed;
	if (!usesDetachedCameraPitch) {
		if (input->PushKey(DIK_E)) roll -= p.rollSpeed;
		if (input->PushKey(DIK_Q)) roll += p.rollSpeed;
	}

	// マウス入力による回転（視点・機体回転）
	long mouseDX = input->GetMouseDeltaX();
	long mouseDY = input->GetMouseDeltaY();
	float mouseSensitivity = 0.002f; // マウス感度
	if (mouseDX != 0) {
		yaw += mouseDX * mouseSensitivity;
	}
	if (mouseDY != 0) {
		if (usesDetachedCameraPitch) {
			cameraPitchDelta += mouseDY * mouseSensitivity;
		} else {
			pitch += mouseDY * mouseSensitivity;
		}
	}
	if (usesDetachedCameraPitch && std::abs(cameraPitchDelta) > 0.0001f) {
		cameraPitch_ = std::clamp(cameraPitch_ + cameraPitchDelta, -1.2f, 1.2f);
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

	if (usesDetachedCameraPitch) {
		quaternion_ = MyMath::Multiply(quaternion_, qYaw);
		quaternion_ = MakeYawQuaternionFromForward(MyMath::RotateVector({ 0.0f, 0.0f, 1.0f }, quaternion_));
	} else {
		quaternion_ = MyMath::Multiply(quaternion_, qPitch);
		quaternion_ = MyMath::Multiply(quaternion_, qYaw);
		quaternion_ = MyMath::Multiply(quaternion_, qRoll);
		quaternion_ = MyMath::Normalize(quaternion_);
	}

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
			
			if (object_) {
				object_->SetQuaternionRotate(quaternion_);
				object_->Update();
			}
		}
	}
}

void Player::CheckCollision(const std::list<std::unique_ptr<Obstacle>> &obstacles) {
	OBB playerOBB = GetOBB();

	for (const auto &obstacle : obstacles) {
		if (!obstacle || !obstacle->IsCollisionEnabled()) {
			continue;
		}

		if (obstacle->IsUseMeshCollider() && !obstacle->IsStageBounds()) {
			const std::vector<Triangle>& triangles = obstacle->GetWorldTriangles();
			
			std::vector<Sphere> playerSpheres;
			const float scale = modelScale_.x; // 0.08f
			
			if (modelName_.find("vf-15c") != std::string::npos) {
				if (currentMode_ == PlayerMode::Fighter) {
					Sphere s;
					s.center = playerOBB.center;
					s.radius = 1.2f * scale;
					playerSpheres.push_back(s);
				} else if (currentMode_ == PlayerMode::Gerwalk) {
					// ガウォーク形態: 2つの球を縦に並べる（上部と足元）
					float r = 2.0f * scale;
					Sphere sTop;
					sTop.center = playerOBB.center;
					sTop.center.y += 1.2f * scale;
					sTop.radius = r;
					playerSpheres.push_back(sTop);
					
					Sphere sBottom;
					sBottom.center = playerOBB.center;
					sBottom.center.y -= 0.8f * scale;
					sBottom.radius = r;
					playerSpheres.push_back(sBottom);
				} else {
					// バトロイド形態（人型）: 3つの球を縦に並べてカプセルを表現（頭/胸、腰/腿、足元）
					float r = 2.2f * scale;
					Sphere sTop;
					sTop.center = playerOBB.center;
					sTop.center.y += 1.5f * scale;
					sTop.radius = r;
					playerSpheres.push_back(sTop);
					
					Sphere sMid;
					sMid.center = playerOBB.center;
					sMid.center.y -= 0.5f * scale;
					sMid.radius = r;
					playerSpheres.push_back(sMid);
					
					Sphere sBottom;
					sBottom.center = playerOBB.center;
					sBottom.center.y -= 2.5f * scale;
					sBottom.radius = r;
					playerSpheres.push_back(sBottom);
				}
			} else {
				Sphere s;
				s.center = playerOBB.center;
				s.radius = GetCollisionRadius();
				playerSpheres.push_back(s);
			}

			for (auto& playerSphere : playerSpheres) {
				for (const auto& tri : triangles) {
					float minX = tri.p[0].x;
					if (tri.p[1].x < minX) minX = tri.p[1].x;
					if (tri.p[2].x < minX) minX = tri.p[2].x;

					float maxX = tri.p[0].x;
					if (tri.p[1].x > maxX) maxX = tri.p[1].x;
					if (tri.p[2].x > maxX) maxX = tri.p[2].x;

					float minY = tri.p[0].y;
					if (tri.p[1].y < minY) minY = tri.p[1].y;
					if (tri.p[2].y < minY) minY = tri.p[2].y;

					float maxY = tri.p[0].y;
					if (tri.p[1].y > maxY) maxY = tri.p[1].y;
					if (tri.p[2].y > maxY) maxY = tri.p[2].y;

					float minZ = tri.p[0].z;
					if (tri.p[1].z < minZ) minZ = tri.p[1].z;
					if (tri.p[2].z < minZ) minZ = tri.p[2].z;

					float maxZ = tri.p[0].z;
					if (tri.p[1].z > maxZ) maxZ = tri.p[1].z;
					if (tri.p[2].z > maxZ) maxZ = tri.p[2].z;

					if (playerSphere.center.x + playerSphere.radius < minX || playerSphere.center.x - playerSphere.radius > maxX ||
						playerSphere.center.y + playerSphere.radius < minY || playerSphere.center.y - playerSphere.radius > maxY ||
						playerSphere.center.z + playerSphere.radius < minZ || playerSphere.center.z - playerSphere.radius > maxZ) {
						continue;
					}

					Vector3 pushVector;
					if (MyMath::IsCollision(playerSphere, tri, pushVector)) {
						// 地面（上向きの面）との衝突で、裏側に回り込んで下に押し下げられるのを防止
						if (tri.normal.y > 0.3f && pushVector.y < 0.0f) {
							float pushLen = MyMath::Length(pushVector);
							pushVector = MyMath::Multiply(pushLen, tri.normal);
						}

						position_.x += pushVector.x;
						position_.y += pushVector.y;
						position_.z += pushVector.z;
						
						for (auto& otherSphere : playerSpheres) {
							otherSphere.center.x += pushVector.x;
							otherSphere.center.y += pushVector.y;
							otherSphere.center.z += pushVector.z;
						}
						
						playerOBB.center.x += pushVector.x;
						playerOBB.center.y += pushVector.y;
						playerOBB.center.z += pushVector.z;

						Vector3 pushNormal = MyMath::Normalize(pushVector);
						float dot = MyMath::Dot(velocity_, pushNormal);
						if (dot < 0.0f) {
							velocity_.x -= dot * pushNormal.x;
							velocity_.y -= dot * pushNormal.y;
							velocity_.z -= dot * pushNormal.z;
						}
					}
				}
			}
			continue;
		}

		const OBB obsOBB = obstacle->GetOBB();
		const Vector3 diff = {
			playerOBB.center.x - obsOBB.center.x,
			playerOBB.center.y - obsOBB.center.y,
			playerOBB.center.z - obsOBB.center.z,
		};
		const float localDistance[3] = {
			Dot(diff, obsOBB.orientations[0]),
			Dot(diff, obsOBB.orientations[1]),
			Dot(diff, obsOBB.orientations[2]),
		};
		const float playerProjection[3] = {
			ProjectionRadius(playerOBB, obsOBB.orientations[0]),
			ProjectionRadius(playerOBB, obsOBB.orientations[1]),
			ProjectionRadius(playerOBB, obsOBB.orientations[2]),
		};

		if (obstacle->IsStageBounds()) {
			Vector3 localPushOut = { 0.0f, 0.0f, 0.0f };

			for (int axis = 0; axis < 3; ++axis) {
				const float limit = (std::max)(0.0f, GetAxisSize(obsOBB.size, axis) - playerProjection[axis]);
				if (localDistance[axis] > limit) {
					if (axis == 0) localPushOut.x = limit - localDistance[axis];
					if (axis == 1) localPushOut.y = limit - localDistance[axis];
					if (axis == 2) localPushOut.z = limit - localDistance[axis];
				} else if (localDistance[axis] < -limit) {
					if (axis == 0) localPushOut.x = -limit - localDistance[axis];
					if (axis == 1) localPushOut.y = -limit - localDistance[axis];
					if (axis == 2) localPushOut.z = -limit - localDistance[axis];
				}
			}

			if (localPushOut.x != 0.0f || localPushOut.y != 0.0f || localPushOut.z != 0.0f) {
				Vector3 worldPushOut = ComposeFromAxes(obsOBB, localPushOut);
				position_.x += worldPushOut.x;
				position_.y += worldPushOut.y;
				position_.z += worldPushOut.z;
				playerOBB.center = Add(playerOBB.center, worldPushOut);
			}
		} 
		else {
			if (!MyMath::IsCollision(playerOBB, obsOBB)) {
				continue;
			}

			const float overlapX = obsOBB.size.x + playerProjection[0] - std::abs(localDistance[0]);
			const float overlapY = obsOBB.size.y + playerProjection[1] - std::abs(localDistance[1]);
			const float overlapZ = obsOBB.size.z + playerProjection[2] - std::abs(localDistance[2]);

			if (overlapX > 0.0f && overlapY > 0.0f && overlapZ > 0.0f) {
				Vector3 localPushOut = { 0.0f, 0.0f, 0.0f };

				if (overlapX < overlapY && overlapX < overlapZ) {
					localPushOut.x = (localDistance[0] > 0.0f) ? overlapX : -overlapX;
				} else if (overlapY < overlapX && overlapY < overlapZ) {
					localPushOut.y = (localDistance[1] > 0.0f) ? overlapY : -overlapY;
				} else {
					localPushOut.z = (localDistance[2] > 0.0f) ? overlapZ : -overlapZ;
				}

				Vector3 worldPushOut = ComposeFromAxes(obsOBB, localPushOut);
				position_.x += worldPushOut.x;
				position_.y += worldPushOut.y;
				position_.z += worldPushOut.z;
				playerOBB.center = Add(playerOBB.center, worldPushOut);
			}
		}
	}
}
