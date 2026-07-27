import os

op_path = r"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Player\Player.cpp"
with open(op_path, "r", encoding="utf-8") as f:
    text = f.read()

start_sig = "void Player::UpdateCamera(Camera *camera, const Vector3 *targetPos) {"
end_sig = "Vector3 Player::GetForwardVector() const {"

start_idx = text.find(start_sig)
end_idx = text.find(end_sig)

if start_idx != -1 and end_idx != -1:
    fixed_camera = """void Player::UpdateCamera(Camera *camera, const Vector3 *targetPos) {
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
	
	// ファイター形態でスピードが出ている時、カメラを後ろに引いてFOVを広げる
	if (currentMode_ == PlayerMode::Fighter) {
		backDistance = 12.0f + (speed * 1.5f); // 速度に応じてカメラが離れる
	}

	// FOVの動的変更
	float targetFov = 0.45f;
	if (currentMode_ == PlayerMode::Fighter) {
		targetFov = 0.45f + (speed * 0.15f);
		targetFov = std::clamp(targetFov, 0.45f, 1.10f);
	}
	float currentFov = camera->GetFovY();
	camera->SetFovY(currentFov + (targetFov - currentFov) * 0.1f);

	// カメラの配置
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

	// ファイター形態での旋回時のカメラロール
	if (currentMode_ == PlayerMode::Fighter) {
		auto input = Input::GetInstance();
		float rollAngle = 0.0f;
		if (input->PushKey(DIK_A) || input->PushKey(DIK_LEFT)) {
			rollAngle = 0.35f; // 左旋回時にカメラを左に傾ける
		} else if (input->PushKey(DIK_D) || input->PushKey(DIK_RIGHT)) {
			rollAngle = -0.35f; // 右旋回時にカメラを右に傾ける
		}
		
		static float currentCameraRoll = 0.0f;
		currentCameraRoll = currentCameraRoll + (rollAngle - currentCameraRoll) * 0.1f;
		
		Quaternion rollQ = MyMath::MakeAxisAngle({0.0f, 0.0f, 1.0f}, currentCameraRoll);
		cameraRotation = MyMath::Multiply(cameraRotation, rollQ);
	}

	camera->SetTranslate(camPos);
	camera->SetRotate({ 0,0,0 });
	camera->SetQuaternion(cameraRotation);
}
"""
    text = text[:start_idx] + fixed_camera + text[end_idx:]
    print("Fixed UpdateCamera!")
else:
    print("Failed to find boundaries for UpdateCamera!")


# Fix Move block
start_move = "	// =================================="
end_move = "	if (currentMode_ == PlayerMode::Battroid && !moveUp) {"

start_move_idx = text.find(start_move)
end_move_idx = text.find(end_move)

if start_move_idx != -1 and end_move_idx != -1:
    fixed_move = """	// ==================================

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
				currentMaxSpeed *= 3.0f; // 大幅に加速！
				currentAccel *= 5.0f;    // 瞬時にトップスピードへ！
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

"""
    text = text[:start_move_idx] + fixed_move + text[end_move_idx:]
    print("Fixed Move!")
else:
    print("Failed to find boundaries for Move!")


with open(op_path, "w", encoding="utf-8") as f:
    f.write(text)
