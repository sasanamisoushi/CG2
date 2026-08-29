#include "Player.h"
#include "3D/Object3dCommon.h"
#include "3D/ModelManager.h"
#include "Game/obstacle/Obstacle.h"
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace {
	constexpr const char *kDefaultPlayerBoxModelName = "PlayerBox";
	constexpr float kBattroidGravity = 0.006f;
	constexpr float kBattroidMaxFallSpeed = 0.16f;
	constexpr float kBattroidIdleAnimationTime = 5.0f;
	constexpr float kBattroidWalkFrequency = 4.5f;
	constexpr float kBattroidFootSwingAngle = 0.14f;
	constexpr float kBattroidFootStepDistance = 0.18f;
	constexpr float kBattroidFootLiftDistance = 0.08f;

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

	bool GetTriangleY(const Vector3& p0, const Vector3& p1, const Vector3& p2, float x, float z, float& outY) {
		float denom = (p1.z - p2.z) * (p0.x - p2.x) + (p2.x - p1.x) * (p0.z - p2.z);
		if (std::abs(denom) < 0.00001f) return false;

		float w0 = ((p1.z - p2.z) * (x - p2.x) + (p2.x - p1.x) * (z - p2.z)) / denom;
		float w1 = ((p2.z - p0.z) * (x - p2.x) + (p0.x - p2.x) * (z - p2.z)) / denom;
		float w2 = 1.0f - w0 - w1;

		if (w0 >= -0.01f && w1 >= -0.01f && w2 >= -0.01f) {
			outY = w0 * p0.y + w1 * p1.y + w2 * p2.y;
			return true;
		}
		return false;
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

	void CancelVerticalVelocityIntoPush(Vector3 &velocity, const Vector3 &pushOut) {
		if ((pushOut.y > 0.0f && velocity.y < 0.0f) || (pushOut.y < 0.0f && velocity.y > 0.0f)) {
			velocity.y = 0.0f;
		}
	}

	std::string StripNodeIndexSuffix(const std::string &name) {
		const size_t underscore = name.find_last_of('_');
		if (underscore == std::string::npos || underscore + 1 >= name.size()) {
			return name;
		}

		for (size_t index = underscore + 1; index < name.size(); ++index) {
			if (name[index] < '0' || name[index] > '9') {
				return name;
			}
		}
		return name.substr(0, underscore);
	}

	int32_t FindJointByBaseName(const Skeleton &skeleton, const std::string &baseName) {
		for (const Joint &joint : skeleton.joints) {
			if (StripNodeIndexSuffix(joint.name) == baseName) {
				return joint.index;
			}
		}
		return -1;
	}

	void AddJointLocalRotation(Skeleton &skeleton, const std::string &baseName, const Vector3 &axis, float angle) {
		const int32_t jointIndex = FindJointByBaseName(skeleton, baseName);
		if (jointIndex < 0 || jointIndex >= static_cast<int32_t>(skeleton.joints.size())) {
			return;
		}

		Quaternion extraRotation = MyMath::MakeAxisAngle(axis, angle);
		Joint &joint = skeleton.joints[jointIndex];
		joint.transform.rotate = MyMath::Normalize(MyMath::Multiply(joint.transform.rotate, extraRotation));
	}

	void AddJointLocalTranslation(Skeleton &skeleton, const std::string &baseName, const Vector3 &translation) {
		const int32_t jointIndex = FindJointByBaseName(skeleton, baseName);
		if (jointIndex < 0 || jointIndex >= static_cast<int32_t>(skeleton.joints.size())) {
			return;
		}

		Joint &joint = skeleton.joints[jointIndex];
		joint.transform.translate = Add(joint.transform.translate, translation);
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

	// プレイヤーの前方に展開する高視認性3Dエナジーシールドモデル
	ModelManager::GetInstance()->CreateSphereModel("PlayerGuardShieldDome", 32);
	if (Model *m = ModelManager::GetInstance()->FindModel("PlayerGuardShieldDome")) {
		m->SetTextureFilePath("resources/white1x1.png");
		m->SetEnableLighting(false);
		m->SetAlphaReference(0.0f);
		m->SetColor({ 0.2f, 0.85f, 1.0f, 0.85f });
	}
	guardBarrier_ = std::make_unique<Object3d>();
	guardBarrier_->Initialize(Object3dCommon::GetInstance());
	guardBarrier_->SetModel("PlayerGuardShieldDome");

	// リングは球ではなく、前方（ローカルZ方向）を向く平面リングとして生成する。
	ModelManager::GetInstance()->CreateRingModel("PlayerGuardShieldRing", 48, 1.0f, 0.82f);
	if (Model *m = ModelManager::GetInstance()->FindModel("PlayerGuardShieldRing")) {
		m->SetTextureFilePath("resources/white1x1.png");
		m->SetEnableLighting(false);
		m->SetAlphaReference(0.0f);
		m->SetColor({ 0.7f, 1.0f, 1.0f, 0.95f });
	}
	guardBarrierRing_ = std::make_unique<Object3d>();
	guardBarrierRing_->Initialize(Object3dCommon::GetInstance());
	guardBarrierRing_->SetModel("PlayerGuardShieldRing");

	FILE* fpInit = nullptr;
	fopen_s(&fpInit, "C:\\Users\\k024g\\.gemini\\antigravity\\brain\\7cca55d0-fd01-48ff-9ea7-7b27dd6bbc34\\debug_log.txt", "w"); // Create new log file
	if (fpInit) {
		fprintf(fpInit, "=== Initialize ===\n");
		fprintf(fpInit, "DomeModel: %p, RingModel: %p\n",
			ModelManager::GetInstance()->FindModel("PlayerGuardShieldDome"),
			ModelManager::GetInstance()->FindModel("PlayerGuardShieldRing"));
		fprintf(fpInit, "DomeObjModel: %p, RingObjModel: %p\n",
			guardBarrier_->GetModel(),
			guardBarrierRing_->GetModel());
		fclose(fpInit);
	}

	position_ = { 0.0f, 0.0f, 0.0f };
	velocity_ = { 0.0f, 0.0f, 0.0f };
	quaternion_ = { 0.0f, 0.0f, 0.0f, 1.0f };
	hp_ = kMaxHP;
	isDead_ = false;
	isSpecialAttackActive_ = false;
	dodgeTimer_ = 0;
	dodgeCooldownTimer_ = 0;
	dodgeDirection_ = 0.0f;
	isGuarding_ = false;
	guardBarrierPulse_ = 0.0f;
	guardScale_ = 0.0f;

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

	// アクションアニメーションの自動ロード
	actionAnimations_.clear();
    // モードパラメータの初期化
    // ファイター（ミサイル乱射特化）
    modeParams_[0].canGuard = false;
    modeParams_[0].canMelee = false;
    modeParams_[0].maxMultiLock = 8;
    modeParams_[0].lockOnAngleDot = 0.5f;
    modeParams_[0].maxLockOnDistance = 200.0f;

    // ガウォーク（広角ロック標準形態）
    modeParams_[1].canGuard = false;
    modeParams_[1].canMelee = false;
    modeParams_[1].maxMultiLock = 4;
    modeParams_[1].lockOnAngleDot = -0.2f;
    modeParams_[1].maxLockOnDistance = 100.0f;

    // バトロイド（防御＆格闘特化）
    modeParams_[2].moveDamping = 0.80f;
    modeParams_[2].pitchSpeed = 0.02f;
    modeParams_[2].yawSpeed = 0.02f;
    modeParams_[2].rollSpeed = 0.02f;
    modeParams_[2].canGuard = true;
    modeParams_[2].canMelee = true;
    modeParams_[2].maxMultiLock = 2;
    modeParams_[2].lockOnAngleDot = -1.0f;
    modeParams_[2].maxLockOnDistance = 60.0f;

    // 初期形態
    ChangeMode(PlayerMode::Fighter);

    boosterEffect_ = std::make_unique<BoosterEffect>();
    boosterEffect_->Initialize();
}

void Player::ChangeMode(PlayerMode newMode) {
	if (newMode != PlayerMode::Fighter) {
		// ガウォーク／バトロイドへ変形した時点でファイター専用回避を終了する。
		dodgeTimer_ = 0;
		dodgeDirection_ = 0.0f;
	} else {
		isGuarding_ = false;
	}

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

void Player::Update(const std::list<std::unique_ptr<Obstacle>> &obstacles, const Vector3 *lockOnTarget) {
	if (isDead_) return;

	isNearBoundary_ = false;
	boundaryWarningIntensity_ = 0.0f;
	isNearWallBoundary_ = false;
	wallBoundaryWarningIntensity_ = 0.0f;
	isNearCeilingBoundary_ = false;
	ceilingBoundaryWarningIntensity_ = 0.0f;

	if (Input::GetInstance()->PushKey(DIK_B)) {
		PlayActionAnimation("Guard");
	} else if (isPlayingAction_ && currentActionAnim_ && currentActionAnim_ == &actionAnimations_["Guard"]) {
		StopActionAnimation(); // ガードキーを離したら元に戻す
	}

	if (lockOnTarget) {
		UpdateLockOnRotation(*lockOnTarget);
	}
	if (isSpecialAttackActive_) {
		isGuarding_ = false;
		// 必殺技中はその場に固定し、慣性による移動も止める。
		velocity_ = { 0.0f, 0.0f, 0.0f };

		// 機体は固定したまま、視点だけはマウスと矢印キーで操作できる。
		Input *input = Input::GetInstance();
		constexpr float kSpecialCameraKeySpeed = 0.025f;
		constexpr float kSpecialCameraMouseSensitivity = 0.002f;
		if (input->PushKey(DIK_LEFT)) specialAttackCameraYaw_ -= kSpecialCameraKeySpeed;
		if (input->PushKey(DIK_RIGHT)) specialAttackCameraYaw_ += kSpecialCameraKeySpeed;
		if (input->PushKey(DIK_UP)) specialAttackCameraPitch_ -= kSpecialCameraKeySpeed;
		if (input->PushKey(DIK_DOWN)) specialAttackCameraPitch_ += kSpecialCameraKeySpeed;
		specialAttackCameraYaw_ += static_cast<float>(input->GetMouseDeltaX()) * kSpecialCameraMouseSensitivity;
		specialAttackCameraPitch_ += static_cast<float>(input->GetMouseDeltaY()) * kSpecialCameraMouseSensitivity;
		specialAttackCameraPitch_ = std::clamp(specialAttackCameraPitch_, -1.2f, 1.2f);
	} else {
		Move(lockOnTarget != nullptr);
		CheckCollision(obstacles);
	}

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
    auto input = Input::GetInstance();
    if (input->TriggerKey(DIK_1)) ChangeMode(PlayerMode::Fighter);
    if (input->TriggerKey(DIK_2)) ChangeMode(PlayerMode::Gerwalk);
    if (input->TriggerKey(DIK_3)) ChangeMode(PlayerMode::Battroid);

    // アニメーションの更新と適用
    if (animationData_.duration > 0.0f && !skeleton_.joints.empty()) {
        if (!isAnimDebugActive_) {
            if (isPlayingAction_ && currentActionAnim_) {
                actionAnimTime_ += 1.0f / 60.0f;
                if (actionAnimTime_ > currentActionAnim_->duration) {
                    isPlayingAction_ = false; // 再生完了
                    currentActionAnim_ = nullptr;
                }
            } else {
                float modeTargetTime = 0.0f;
                if (currentMode_ == PlayerMode::Fighter) {
                    modeTargetTime = 0.0f;
                } else if (currentMode_ == PlayerMode::Gerwalk) {
                    modeTargetTime = 1.71f;  // Frame 41
                } else {
                    modeTargetTime = 5.0f;   // kBattroidIdleAnimationTime
                }
                float diff = modeTargetTime - animationTime_;
                animationTime_ += diff * 0.1f;
            } 
        } else {
            if (overrideAnimation_ != nullptr) {
                // アニメーション編集ツール(カテゴリー5)等でカスタムアクションをデバッグ中の場合、
                // ベースアニメーション(変形)は現在のモード状態を維持する
                float modeTargetTime = 0.0f;
                if (currentMode_ == PlayerMode::Fighter) {
                    modeTargetTime = 0.0f;
                } else if (currentMode_ == PlayerMode::Gerwalk) {
                    modeTargetTime = 1.71f;
                } else {
                    modeTargetTime = 5.0f;
                }
                float diff = modeTargetTime - animationTime_;
                animationTime_ += diff * 0.1f;
            } else {
                // プレイヤー設定(カテゴリー0)等でベースアニメーション自体をデバッグ中の場合
                animationTime_ = this->targetAnimationTime_;
            }
        }

        // 1. まずベースのアニメーション（変形アニメーション等）を全体に適用し、リセットする
        ApplyAnimation(skeleton_, animationData_, animationTime_);

        // 2. その上から、アクションやカスタムアニメーション（一部のボーンのみ等）を上書きで適用する
        const Animation* overrideAnimToApply = nullptr;
        float timeToApply = animationTime_;
        
        if (overrideAnimation_) {
            overrideAnimToApply = overrideAnimation_;
            if (isAnimDebugActive_) {
                timeToApply = this->targetAnimationTime_; // カスタムアニメの時間はシミュレーター指定時間
            }
        } else if (isPlayingAction_ && currentActionAnim_ && !isAnimDebugActive_) {
            overrideAnimToApply = currentActionAnim_;
            timeToApply = actionAnimTime_;
        }
        
        if (overrideAnimToApply && overrideAnimToApply->duration > 0.0f) {
            ApplyAnimation(skeleton_, *overrideAnimToApply, timeToApply);
        }

        if (!isAnimDebugActive_ && currentMode_ == PlayerMode::Battroid && isBattroidWalking_) {
            ApplyBattroidProceduralWalk();
        }
        ::Update(skeleton_);

        if (enableSkinning_ && object_ && object_->GetModel()) {
            object_->GetModel()->UpdateSkinCluster(object_->skinCluster, skeleton_);
        }
    }
        // スケールの滑らかな補間（アニメーション変形のように見せる）
    currentDrawScale_.x += (targetDrawScale_.x - currentDrawScale_.x) * 0.2f;
    currentDrawScale_.y += (targetDrawScale_.y - currentDrawScale_.y) * 0.2f;
    currentDrawScale_.z += (targetDrawScale_.z - currentDrawScale_.z) * 0.2f;

	Quaternion visualQuaternion = quaternion_;
	if (dodgeTimer_ > 0) {
		const float progress = 1.0f - static_cast<float>(dodgeTimer_) / static_cast<float>(kDodgeDurationFrames);
		const float rollAngle = progress * 6.2831853f * dodgeDirection_;
		visualQuaternion = MyMath::Normalize(MyMath::Multiply(
			quaternion_, MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, rollAngle)));
	}
    if (object_) {
	    object_->SetScale(currentDrawScale_);
	    object_->SetTranslate(position_);
	    object_->SetQuaternionRotate(visualQuaternion);
	    object_->Update();
    }

	// Bキーを押している間だけガードを展開する。
	const bool guardInput = input->PushKey(DIK_B);
	isGuarding_ = guardInput && !isSpecialAttackActive_;
	const float targetGuardScale = isGuarding_ ? 1.0f : 0.0f;
	guardScale_ += (targetGuardScale - guardScale_) * 0.25f;

	// シールドが背景に溶けても状態を認識できるよう、ガード中は機体を青く発光させる。
	if (object_ && object_->GetModel()) {
		const Vector4 normalColor = UsesNaturalPlayerModelScale(modelName_)
			? Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }
			: Vector4{ 0.2f, 0.5f, 1.0f, 1.0f };
		object_->GetModel()->SetEnableLighting(!isGuarding_);
		object_->GetModel()->SetColor(isGuarding_
			? Vector4{ 0.15f, 0.75f, 1.0f, 1.0f }
			: normalColor);
	}

	if (guardScale_ > 0.01f) {
		guardBarrierPulse_ += 0.15f;
		const float pulse = 0.5f + 0.5f * std::sin(guardBarrierPulse_);

		Vector3 forward = GetForwardVector();
		Vector3 up = MyMath::RotateVector({ 0.0f, 1.0f, 0.0f }, quaternion_);

		// 形態ごとの胸部・視点高さ（目前の高さ）
		float eyeHeight = 0.1f;
		if (currentMode_ == PlayerMode::Battroid) {
			eyeHeight = 0.8f;
		} else if (currentMode_ == PlayerMode::Gerwalk) {
			eyeHeight = 0.35f;
		}

		Vector3 origin = {
			position_.x + up.x * eyeHeight,
			position_.y + up.y * eyeHeight,
			position_.z + up.z * eyeHeight
		};

		// 自機の表面のすぐ前に配置する。ハードコードした距離だと、
		// 小さいファイター形態では機体から離れすぎて見失いやすい。
		// カメラから見切れず、かつ機体と重ならない前方距離に固定する。
		const float forwardDistance = 0.80f;
		Vector3 shieldPos = {
			origin.x + forward.x * forwardDistance,
			origin.y + forward.y * forwardDistance,
			origin.z + forward.z * forwardDistance
		};

		static int logCounter = 0;
		if (logCounter++ % 60 == 0) {
			FILE* fp = nullptr;
			fopen_s(&fp, "C:\\Users\\k024g\\.gemini\\antigravity\\brain\\7cca55d0-fd01-48ff-9ea7-7b27dd6bbc34\\debug_log.txt", "a");
			if (fp) {
				fprintf(fp, "Pos: (%.3f, %.3f, %.3f) Fwd: (%.3f, %.3f, %.3f) ShieldPos: (%.3f, %.3f, %.3f)\n",
					position_.x, position_.y, position_.z,
					forward.x, forward.y, forward.z,
					shieldPos.x, shieldPos.y, shieldPos.z);
				fclose(fp);
			}
		}

		const float pulseScale = 1.0f + pulse * 0.06f;

		// メインの3D曲面エナジーシールドドーム
		if (guardBarrier_) {
			const float shieldRadius = 1.10f * guardScale_ * pulseScale;
			const float shieldDepth = 0.30f * guardScale_;
			guardBarrier_->SetTranslate(shieldPos);
			guardBarrier_->SetScale({ shieldRadius, shieldRadius, shieldDepth });
			guardBarrier_->SetQuaternionRotate(quaternion_); // プレイヤーの前方をカバー
			if (guardBarrier_->GetModel()) {
				guardBarrier_->GetModel()->SetEnableLighting(false);
				guardBarrier_->GetModel()->SetTextureFilePath("resources/white1x1.png");
				guardBarrier_->GetModel()->SetColor({ 0.20f, 0.85f, 1.0f, (0.80f + pulse * 0.15f) * guardScale_ });
			}
			guardBarrier_->Update();
		}

		// 外周の鮮烈な3Dエナジーフレームリング
		if (guardBarrierRing_) {
			const float ringRadius = 1.20f * guardScale_ * pulseScale;
			const float ringDepth = 1.0f;
			guardBarrierRing_->SetTranslate(shieldPos);
			guardBarrierRing_->SetScale({ ringRadius, ringRadius, ringDepth });
			
			// 外周リングを回転させてバリアのエネルギー展開感を強調
			Quaternion ringRot = MyMath::Multiply(quaternion_, MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, guardBarrierPulse_ * 0.6f));
			guardBarrierRing_->SetQuaternionRotate(ringRot);
			if (guardBarrierRing_->GetModel()) {
				guardBarrierRing_->GetModel()->SetEnableLighting(false);
				guardBarrierRing_->GetModel()->SetTextureFilePath("resources/white1x1.png");
				guardBarrierRing_->GetModel()->SetColor({ 0.80f, 1.0f, 1.0f, (0.95f + pulse * 0.05f) * guardScale_ });
			}
			guardBarrierRing_->Update();
		}
	}
}

void Player::ApplyBattroidProceduralWalk() {
	const PlayerModeParams &p = modeParams_[static_cast<int>(PlayerMode::Battroid)];
	const float horizontalSpeed = Length({ velocity_.x, 0.0f, velocity_.z });
	const float speedRatio = std::clamp(horizontalSpeed / (std::max)(0.0001f, p.maxMoveSpeed), 0.35f, 1.0f);

	battroidWalkTime_ += (1.0f / 60.0f) * (0.8f + speedRatio * 1.2f);

	const float phase = battroidWalkTime_ * kBattroidWalkFrequency;
	const float step = std::sin(phase) * kBattroidFootStepDistance * speedRatio;
	const float footSwing = std::cos(phase) * kBattroidFootSwingAngle * speedRatio;
	const float leftLift = (std::max)(0.0f, std::sin(phase)) * kBattroidFootLiftDistance * speedRatio;
	const float rightLift = (std::max)(0.0f, -std::sin(phase)) * kBattroidFootLiftDistance * speedRatio;

	AddJointLocalTranslation(skeleton_, "FootFront", { 0.0f, rightLift, step });
	AddJointLocalTranslation(skeleton_, "FootRear", { 0.0f, rightLift, step });
	AddJointLocalTranslation(skeleton_, "FootGuardFront", { 0.0f, rightLift, step });
	AddJointLocalTranslation(skeleton_, "FootGuardRear", { 0.0f, rightLift, step });
	AddJointLocalRotation(skeleton_, "FootFront", { 1.0f, 0.0f, 0.0f }, -footSwing);
	AddJointLocalRotation(skeleton_, "FootRear", { 1.0f, 0.0f, 0.0f }, -footSwing * 0.7f);

	AddJointLocalTranslation(skeleton_, "FootFront.001", { 0.0f, leftLift, -step });
	AddJointLocalTranslation(skeleton_, "FootRear.001", { 0.0f, leftLift, -step });
	AddJointLocalTranslation(skeleton_, "FootGuardFront.001", { 0.0f, leftLift, -step });
	AddJointLocalTranslation(skeleton_, "FootGuardRear.001", { 0.0f, leftLift, -step });
	AddJointLocalRotation(skeleton_, "FootFront.001", { 1.0f, 0.0f, 0.0f }, footSwing);
	AddJointLocalRotation(skeleton_, "FootRear.001", { 1.0f, 0.0f, 0.0f }, footSwing * 0.7f);
}

void Player::Draw(Camera* camera) {
	if (object_) {
		object_->Draw();
	}
	if (guardScale_ > 0.01f) {
		Object3dCommon::GetInstance()->SetAlphaBlendDrawSettings();
		if (guardBarrier_) {
			guardBarrier_->Draw();
		}
		Object3dCommon::GetInstance()->SetEffectDrawSettings();
		if (guardBarrierRing_) {
			guardBarrierRing_->Draw();
		}
		Object3dCommon::GetInstance()->SetCommonDrawSettings();
	}
	if (boosterEffect_) {
		boosterEffect_->Draw(camera);
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

	if (isSpecialAttackActive_) {
		cameraDirection = GetAttackDirection();
	} else if (targetPos) {
		Vector3 toTarget = { targetPos->x - lookTarget.x, targetPos->y - lookTarget.y, targetPos->z - lookTarget.z };
		Vector3 flatToTarget = { toTarget.x, 0.0f, toTarget.z };
		float distToTargetSq = LengthSq(flatToTarget);
		
		Vector3 targetDir;
		if (distToTargetSq > 0.0001f) {
			targetDir = NormalizeOrZero(flatToTarget);
		} else {
			targetDir = flatForward;
		}

		cameraDirection = targetDir;
		float pitchBlend = std::clamp(toTarget.y / 50.0f, -0.5f, 0.5f);
		cameraDirection.y = pitchBlend;
		cameraDirection = NormalizeOrZero(cameraDirection);

		float t = 0.35f;
		Vector3 blendTarget = {
			lookTarget.x + toTarget.x * t,
			lookTarget.y + toTarget.y * t,
			lookTarget.z + toTarget.z * t
		};
		
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

	lastCameraDirection_ = cameraDirection;

	float backDistance = 12.0f;
	const float heightOffset = 2.5f;
	float speed = Length(velocity_);
	
	if (currentMode_ == PlayerMode::Fighter) {
		backDistance = 12.0f + (speed * 1.5f);
	}

	float targetFov = 0.45f;
	if (currentMode_ == PlayerMode::Fighter) {
		targetFov = 0.45f + (speed * 0.15f);
		targetFov = std::clamp(targetFov, 0.45f, 1.10f);
	}
	float currentFov = camera->GetFovY();
	camera->SetFovY(currentFov + (targetFov - currentFov) * 0.1f);

	Vector3 cameraBackOffset = Scale(cameraDirection, -backDistance);
	Vector3 playerOBBCenter = GetOBB().center;
	Vector3 camPos = {
		playerOBBCenter.x + cameraBackOffset.x,
		playerOBBCenter.y + heightOffset + cameraBackOffset.y,
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

	if (currentMode_ == PlayerMode::Fighter) {
		auto input = Input::GetInstance();
		float rollAngle = 0.0f;
		if (input->PushKey(DIK_A) || input->PushKey(DIK_LEFT)) {
			rollAngle = 0.35f;
		} else if (input->PushKey(DIK_D) || input->PushKey(DIK_RIGHT)) {
			rollAngle = -0.35f;
		}
		
		static float currentCameraRoll = 0.0f;
		currentCameraRoll = currentCameraRoll + (rollAngle - currentCameraRoll) * 0.1f;
		
		Quaternion rollQ = MyMath::MakeAxisAngle({0.0f, 0.0f, 1.0f}, currentCameraRoll);
		cameraRotation = MyMath::Multiply(cameraRotation, rollQ);
	}

	camera->SetTranslate(camPos);
	camera->SetRotate({ 0,0,0 });
	camera->SetQuaternion(cameraRotation);
}Vector3 Player::GetForwardVector() const {
	Vector3 bodyForward = MyMath::RotateVector({ 0.0f, 0.0f, 1.0f }, quaternion_);
	if (currentMode_ == PlayerMode::Fighter) {
		return bodyForward;
	}

	return MakeForwardFromFlatAndPitch(bodyForward, cameraPitch_);
}

Vector3 Player::GetAttackDirection() const {
	if (!isSpecialAttackActive_) {
		return GetForwardVector();
	}

	const Vector3 flatAimForward = {
		std::sin(specialAttackCameraYaw_),
		0.0f,
		std::cos(specialAttackCameraYaw_)
	};
	return MakeForwardFromFlatAndPitch(flatAimForward, specialAttackCameraPitch_);
}


void Player::SyncRotationToLastCameraDirection() {
	if (LengthSq(lastCameraDirection_) > 0.0001f) {
		float pitch = -std::asin(std::clamp(lastCameraDirection_.y, -1.0f, 1.0f));
		float yaw = std::atan2(lastCameraDirection_.x, lastCameraDirection_.z);
		// Update pitch and yaw based on last camera direction (keep roll as 0 to snap flat)
		SetRotation({pitch, yaw, 0.0f});
	}
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
	if (boosterEffect_) {
		boosterEffect_.reset();
	}
}

void Player::TakeDamage(int damage) {
	if (isDead_ || isSpecialAttackActive_ || IsDodging() || IsGuarding()) return;
	hp_ -= damage;
	if (hp_ <= 0) {
		hp_ = 0;
		OnCollision();
	}
}

void Player::Move(bool rotationLocked) {
	auto input = Input::GetInstance();

    if (input->TriggerKey(DIK_1)) ChangeMode(PlayerMode::Fighter);
    if (input->TriggerKey(DIK_2)) ChangeMode(PlayerMode::Gerwalk);
    if (input->TriggerKey(DIK_3)) ChangeMode(PlayerMode::Battroid);

    PlayerModeParams& p = modeParams_[static_cast<int>(currentMode_)];

	const bool moveForward = input->PushKey(DIK_W);
	const bool moveBackward = input->PushKey(DIK_S);
	const bool moveUp = input->PushKey(DIK_SPACE);
	const bool moveDown = input->PushKey(DIK_LSHIFT);
	const bool guardInput = input->PushKey(DIK_B);
	isGuarding_ = guardInput;
	isBattroidWalking_ = currentMode_ == PlayerMode::Battroid && (moveForward || moveBackward);
	if (!isBattroidWalking_) {
		battroidWalkTime_ = 0.0f;
	}

	float pitch = 0.0f;
	float yaw = 0.0f;
	float roll = 0.0f;
	float cameraPitchDelta = 0.0f;
	const bool usesDetachedCameraPitch = currentMode_ != PlayerMode::Fighter;

	// 回避はファイター形態専用。TriggerKeyなので1入力につき1回だけ回転する。
	if (currentMode_ == PlayerMode::Fighter && dodgeCooldownTimer_ <= 0 && dodgeTimer_ <= 0) {
		if (input->TriggerKey(DIK_A)) {
			dodgeTimer_ = kDodgeDurationFrames;
			dodgeDirection_ = -1.0f;
		} else if (input->TriggerKey(DIK_D)) {
			dodgeTimer_ = kDodgeDurationFrames;
			dodgeDirection_ = 1.0f;
		}
	}

	// 姿勢制御（共通）
	if (!rotationLocked && input->PushKey(DIK_UP)) {
		if (usesDetachedCameraPitch) {
			cameraPitchDelta -= p.pitchSpeed;
		} else {
			pitch -= p.pitchSpeed;
		}
	}
	if (!rotationLocked && input->PushKey(DIK_DOWN)) {
		if (usesDetachedCameraPitch) {
			cameraPitchDelta += p.pitchSpeed;
		} else {
			pitch += p.pitchSpeed;
		}
	}
	if (!rotationLocked && input->PushKey(DIK_RIGHT)) yaw += p.yawSpeed;
	if (!rotationLocked && input->PushKey(DIK_LEFT)) yaw -= p.yawSpeed;
	if (!usesDetachedCameraPitch) {
		if (!rotationLocked && input->PushKey(DIK_E)) roll -= p.rollSpeed;
		if (!rotationLocked && input->PushKey(DIK_Q)) roll += p.rollSpeed;
	}

	// マウス入力による回転（視点・機体回転）
	long mouseDX = input->GetMouseDeltaX();
	long mouseDY = input->GetMouseDeltaY();
	float mouseSensitivity = 0.002f; // マウス感度
	if (!rotationLocked && mouseDX != 0) {
		yaw += mouseDX * mouseSensitivity;
	}
	if (!rotationLocked && mouseDY != 0) {
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
		
		if (!rotationLocked && moveUp) pitch -= p.pitchSpeed;
		if (!rotationLocked && moveDown) pitch += p.pitchSpeed;

	} else if (currentMode_ == PlayerMode::Gerwalk) {
		// ガウォーク形態：全方位へのカニ歩き、ホバリング
		if (moveForward) localMove.z += 1.0f;
		if (moveBackward) localMove.z -= 1.0f;
		if (moveUp) localMove.y += 1.0f;
		if (moveDown) localMove.y -= 1.0f;
		
	} else if (currentMode_ == PlayerMode::Battroid) {
		// バトロイド形態：ガウォークと同じ全方位移動
		if (moveForward) localMove.z += 1.0f;
		if (moveBackward) localMove.z -= 1.0f;
		if (moveUp) localMove.y += 1.0f;
		if (moveDown) localMove.y -= 1.0f;
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
	if (currentMode_ == PlayerMode::Fighter && !isInputRotation && !rotationLocked) {
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
		
		float currentMaxSpeed = p.maxMoveSpeed;
		float currentAccel = p.moveAcceleration;
		
		if (currentMode_ == PlayerMode::Fighter) {
			auto input = Input::GetInstance();
			bool mForward = input->PushKey(DIK_W);
			bool mBackward = input->PushKey(DIK_S);
			
			if (mForward) {
				currentMaxSpeed *= 3.0f;
				currentAccel *= 5.0f;
			} else if (mBackward) {
				currentMaxSpeed *= 0.4f;
			} else {
				currentMaxSpeed *= 0.8f;
			}
		}
		
		float finalAccel = isSongActive_ ? currentAccel * 1.5f : currentAccel;
		float finalMaxSpeed = isSongActive_ ? currentMaxSpeed * 1.5f : currentMaxSpeed;
		
		velocity_ = Add(velocity_, Scale(worldMove, finalAccel));
		
		if (LengthSq(velocity_) > finalMaxSpeed * finalMaxSpeed) {
			velocity_ = Scale(velocity_, 0.95f);
			if (LengthSq(velocity_) < finalMaxSpeed * finalMaxSpeed) {
				velocity_ = ClampLength(velocity_, finalMaxSpeed);
			}
		} else {
			velocity_ = ClampLength(velocity_, finalMaxSpeed);
		}
	} else {
		velocity_ = Scale(velocity_, p.moveDamping);
		if (LengthSq(velocity_) <= 0.00001f) {
			velocity_ = { 0.0f, 0.0f, 0.0f };
		}
	}
	if (currentMode_ == PlayerMode::Battroid && !moveUp) {
		velocity_.y = (std::max)(velocity_.y - kBattroidGravity, -kBattroidMaxFallSpeed);
	}

	position_ = Add(position_, velocity_);
	if (dodgeTimer_ > 0) {
		const Vector3 dodgeRight = MyMath::RotateVector({ 1.0f, 0.0f, 0.0f }, quaternion_);
		position_ = Add(position_, Scale(dodgeRight, dodgeDirection_ * 0.38f));
	}

	// 近接攻撃（Vキー）
	if (p.canMelee && !isMeleeAttacking_ && input->TriggerKey(DIK_V)) {
		isMeleeAttacking_ = true;
		meleeTimer_ = 30; // 30フレーム持続
		PlayActionAnimation("Melee"); // アニメーションがあれば再生
	}

	if (isMeleeAttacking_) {
		meleeTimer_--;
		if (meleeTimer_ <= 0) {
			isMeleeAttacking_ = false;
		} else {
			// 前方へダッシュする力を少し加える
			Vector3 forward = GetForwardVector();
			velocity_ = Add(velocity_, Scale(forward, 0.03f));
		}
	}

	if (isMeleeAttacking_) {
		// 近接攻撃中は他の移動入力を無視するか、弱める
		pitch = 0.0f;
		yaw = 0.0f;
		roll = 0.0f;
	} else if (dodgeTimer_ > 0) {
		dodgeTimer_--;
		if (dodgeTimer_ <= 0) {
			dodgeCooldownTimer_ = kDodgeCooldownFrames;
		}
	} else if (dodgeCooldownTimer_ > 0) {
		--dodgeCooldownTimer_;
	}
}

void Player::UpdateLockOnRotation(const Vector3& targetPos) {
	Vector3 toTarget = { targetPos.x - position_.x, targetPos.y - position_.y, targetPos.z - position_.z };
	if (LengthSq(toTarget) < 0.0001f) return;

	const Vector3 targetForward = NormalizeOrZero(toTarget);
	if (currentMode_ == PlayerMode::Fighter) {
		// カメラと同じ方向へ即時固定し、機体の正面側へカメラが回り込むのを防ぐ。
		quaternion_ = MakeNoRollLookQuaternion(targetForward);
	} else {
		// 分離ピッチを使う形態では、機体は水平旋回、上下方向はカメラピッチに保持する。
		quaternion_ = MakeYawQuaternionFromForward(targetForward);
		cameraPitch_ = CameraPitchFromForward(targetForward);
	}

	if (object_) {
		object_->SetQuaternionRotate(quaternion_);
		object_->Update();
	}
}

void Player::SetSpecialAttackActive(bool active) {
	if (active && !isSpecialAttackActive_) {
		const Vector3 currentCameraForward = LengthSq(lastCameraDirection_) > 0.0001f
			? NormalizeOrZero(lastCameraDirection_)
			: NormalizeOrZero(GetForwardVector());
		specialAttackCameraYaw_ = std::atan2(currentCameraForward.x, currentCameraForward.z);
		specialAttackCameraPitch_ = CameraPitchFromForward(currentCameraForward);
	}
	isSpecialAttackActive_ = active;
}

void Player::SetSongActive(bool active) {
	isSongActive_ = active;
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
			// 地底抜け防止レスキュー：足元の地形の最高標高を特定し、潜り込んでいたら即時地上に押し上げる
			float maxTerrainY = -99999.0f;
			bool foundTerrain = false;
			for (const auto& tri : triangles) {
				if (tri.normal.y > 0.2f) {
					float triY = 0.0f;
					if (GetTriangleY(tri.p[0], tri.p[1], tri.p[2], position_.x, position_.z, triY)) {
						if (triY > maxTerrainY) {
							maxTerrainY = triY;
							foundTerrain = true;
						}
					}
				}
			}

			float playerRadius = GetCollisionRadius();
			if (foundTerrain && position_.y < maxTerrainY + playerRadius * 0.5f) {
				position_.y = maxTerrainY + playerRadius * 0.5f;
				if (velocity_.y < 0.0f) {
					velocity_.y = 0.0f;
				}
				playerOBB = GetOBB();
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
						if (tri.normal.y > 0.3f) {
							float pushLen = MyMath::Length(pushVector);
							if (pushLen < 0.05f) pushLen = 0.05f;
							pushVector = MyMath::Multiply(pushLen, tri.normal);
							if (pushVector.y < 0.0f) {
								pushVector.y = -pushVector.y;
							}
						}

						position_.x += pushVector.x;
						position_.y += pushVector.y;
						position_.z += pushVector.z;
						CancelVerticalVelocityIntoPush(velocity_, pushVector);
						
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
			
			float warningThreshold = 50.0f; // 50 units away starts the warning
			float minDistanceToSideWall = 99999.0f;
			Vector3 closestSideWallNormal = {0.0f, 0.0f, 1.0f};
			float distanceToCeiling = 99999.0f;
			Vector3 ceilingNormal = {0.0f, 1.0f, 0.0f};

			for (int axis = 0; axis < 3; ++axis) {
				const float limit = (std::max)(0.0f, GetAxisSize(obsOBB.size, axis) - playerProjection[axis]);
				
				// 警告判定用の距離計算
				if (axis != 1) {
					float distPositive = limit - localDistance[axis];
					if (distPositive < minDistanceToSideWall) {
						minDistanceToSideWall = distPositive;
						closestSideWallNormal = obsOBB.orientations[axis];
					}

					float distNegative = limit + localDistance[axis];
					if (distNegative < minDistanceToSideWall) {
						minDistanceToSideWall = distNegative;
						closestSideWallNormal = {-obsOBB.orientations[axis].x, -obsOBB.orientations[axis].y, -obsOBB.orientations[axis].z};
					}
				} else {
					// Ceiling only. The negative Y side is the floor, so keep it out of the warning.
					float distCeiling = limit - localDistance[axis];
					if (distCeiling < distanceToCeiling) {
						distanceToCeiling = distCeiling;
						ceilingNormal = obsOBB.orientations[axis];
					}
				}

				if (localDistance[axis] > limit) {
					if (axis == 0) localPushOut.x = limit - localDistance[axis];
					if (axis == 1) localPushOut.y = limit - localDistance[axis];
					if (axis == 2) localPushOut.z = limit - localDistance[axis];
				} else if (localDistance[axis] < -limit) {
					if (axis == 0) localPushOut.x = -limit - localDistance[axis];
					// axis == 1 のマイナスY方向（底面）は山モデルがあるので、山への着地を妨げないよう押し戻しを行わない
					if (axis == 2) localPushOut.z = -limit - localDistance[axis];
				}
			}

			auto registerBoundaryAlert = [&](float intensity, const Vector3& position, const Vector3& normal) {
				if (intensity > boundaryWarningIntensity_) {
					isNearBoundary_ = true;
					boundaryWarningIntensity_ = intensity;
					boundaryAlertPosition_ = position;
					boundaryAlertNormal_ = normal;
				}
			};

			Vector3 ceilingPosition = {
				playerOBB.center.x + ceilingNormal.x * distanceToCeiling,
				playerOBB.center.y + ceilingNormal.y * distanceToCeiling,
				playerOBB.center.z + ceilingNormal.z * distanceToCeiling
			};

			// 警告フラグの更新
			if (minDistanceToSideWall < warningThreshold) {
				isNearWallBoundary_ = true;
				wallBoundaryWarningIntensity_ = 1.0f - (std::max)(0.0f, minDistanceToSideWall) / warningThreshold;
				wallBoundaryAlertNormal_ = closestSideWallNormal;

				wallBoundaryAlertPosition_ = {
					playerOBB.center.x + closestSideWallNormal.x * minDistanceToSideWall,
					playerOBB.center.y + closestSideWallNormal.y * minDistanceToSideWall,
					playerOBB.center.z + closestSideWallNormal.z * minDistanceToSideWall
				};

				if (distanceToCeiling < warningThreshold) {
					constexpr float kBoundaryAlertHalfHeight = 2.0f;
					wallBoundaryAlertPosition_.y = (std::min)(wallBoundaryAlertPosition_.y, ceilingPosition.y - kBoundaryAlertHalfHeight);
				}

				registerBoundaryAlert(wallBoundaryWarningIntensity_, wallBoundaryAlertPosition_, wallBoundaryAlertNormal_);
			}

			if (distanceToCeiling < warningThreshold) {
				isNearCeilingBoundary_ = true;
				ceilingBoundaryWarningIntensity_ = 1.0f - (std::max)(0.0f, distanceToCeiling) / warningThreshold;
				ceilingBoundaryAlertNormal_ = ceilingNormal;
				ceilingBoundaryAlertPosition_ = ceilingPosition;

				registerBoundaryAlert(ceilingBoundaryWarningIntensity_, ceilingBoundaryAlertPosition_, ceilingBoundaryAlertNormal_);
			}

			if (localPushOut.x != 0.0f || localPushOut.y != 0.0f || localPushOut.z != 0.0f) {
				Vector3 worldPushOut = ComposeFromAxes(obsOBB, localPushOut);
				position_.x += worldPushOut.x;
				position_.y += worldPushOut.y;
				position_.z += worldPushOut.z;
				CancelVerticalVelocityIntoPush(velocity_, worldPushOut);
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
				CancelVerticalVelocityIntoPush(velocity_, worldPushOut);
				playerOBB.center = Add(playerOBB.center, worldPushOut);
			}
		}
	}
}


void Player::PlayActionAnimation(const std::string& actionName) {
    if (actionAnimations_.count(actionName)) {
        currentActionAnim_ = &actionAnimations_[actionName];
        actionAnimTime_ = 0.0f;
        isPlayingAction_ = true;
    }
}

void Player::StopActionAnimation() {
    isPlayingAction_ = false;
    currentActionAnim_ = nullptr;
}

OBB Player::GetMeleeHitbox() const {
	OBB obb = GetOBB();
	// 前方に判定を出す
	Vector3 forward = GetForwardVector();
	// 攻撃範囲のサイズ設定
	obb.size.x += 4.0f; // 横に広く
	obb.size.y += 4.0f; // 縦に広く
	obb.size.z += 8.0f; // 前方に長く

	// 中心位置を前方にずらす
	obb.center.x += forward.x * 6.0f;
	obb.center.y += forward.y * 6.0f;
	obb.center.z += forward.z * 6.0f;
	return obb;
}


