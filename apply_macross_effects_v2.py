import re
import sys

op_path = r"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Player\Player.cpp"
with open(op_path, "r", encoding="utf-8") as f:
    text = f.read()

# 1. Update Camera
# Search from 'lastCameraDirection_ = cameraDirection;' up to 'camera->SetQuaternion(cameraRotation);'
camera_pattern = re.compile(r"(lastCameraDirection_ = cameraDirection;).*?(camera->SetQuaternion\(cameraRotation\);)", re.DOTALL)

camera_replacement = r"""\1

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
	\2"""

if camera_pattern.search(text):
    text = camera_pattern.sub(camera_replacement, text)
    print("Camera logic replaced successfully!")
else:
    print("Failed to find camera logic block!")

# 2. Update Move
move_pattern = re.compile(r"(\t\tif \(currentMode_ == PlayerMode::Fighter\) \{\s*if \(moveForward\) \{\s*currentMaxSpeed \*=) 1\.8f;(\s*currentAccel \*=) 2\.0f;", re.DOTALL)

move_replacement = r"\1 3.0f;\2 5.0f;"

if move_pattern.search(text):
    text = move_pattern.sub(move_replacement, text)
    print("Move logic replaced successfully!")
else:
    print("Failed to find Move logic block! Trying fallback...")
    # fallback to find the older one if it was 1.8f not updated, wait, let's just search for it without hardcoding numbers if possible
    # Wait! Earlier I found that the move logic didn't even have currentMaxSpeed!
    # Ah! The previous replace failed, so the file still looks like:
    # velocity_ = Add(velocity_, Scale(worldMove, isSongActive_ ? p.moveAcceleration * 1.5f : p.moveAcceleration));
    # velocity_ = ClampLength(velocity_, isSongActive_ ? p.maxMoveSpeed * 1.5f : p.maxMoveSpeed);

move_pattern_fallback = re.compile(r"(	if \(LengthSq\(localMove\) > 0\.0001f\) \{\s*localMove = NormalizeOrZero\(localMove\);\s*Vector3 worldMove = MyMath::RotateVector\(localMove, quaternion_\);).*?(	\} else \{)", re.DOTALL)

move_replacement_fallback = r"""\1
		float currentMaxSpeed = p.maxMoveSpeed;
		float currentAccel = p.moveAcceleration;
		
		if (currentMode_ == PlayerMode::Fighter) {
			if (moveForward) {
				currentMaxSpeed *= 3.0f; // 大幅に加速！
				currentAccel *= 5.0f;    // 瞬時にトップスピードへ！
			} else if (moveBackward) {
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
\2"""

if move_pattern_fallback.search(text):
    text = move_pattern_fallback.sub(move_replacement_fallback, text)
    print("Move fallback logic replaced successfully!")
else:
    print("Failed to find fallback move logic block either!")

with open(op_path, "w", encoding="utf-8") as f:
    f.write(text)

print("Finished script!")
