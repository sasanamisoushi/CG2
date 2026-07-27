import sys
import os

op_path = r"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Player\Player.cpp"
with open(op_path, "r", encoding="utf-8") as f:
    text = f.read()

broken_block_camera = """	const float backDistance = 12.0f;
	const float heightOffset = 2.5f;
	
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

	

	camera->SetTranslate(camPos);
	camera->SetRotate({ 0,0,0 });
	camera->SetQuaternion(cameraRotation);"""

fixed_block_camera = """	float backDistance = 12.0f;
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
	camera->SetQuaternion(cameraRotation);"""

text = text.replace(broken_block_camera, fixed_block_camera)

broken_block_move = """		if (currentMode_ == PlayerMode::Fighter) {
			if (moveForward) {
				currentMaxSpeed *= 1.8f;
				currentAccel *= 2.0f;
			} else if (moveBackward) {
				currentMaxSpeed *= 0.4f;
			} else {
				currentMaxSpeed *= 0.8f;
			}
		}"""

fixed_block_move = """		if (currentMode_ == PlayerMode::Fighter) {
			if (moveForward) {
				currentMaxSpeed *= 3.0f; // 大幅に加速！
				currentAccel *= 5.0f;    // 瞬時にトップスピードへ！
			} else if (moveBackward) {
				currentMaxSpeed *= 0.4f;
			} else {
				currentMaxSpeed *= 0.8f;
			}
		}"""

text = text.replace(broken_block_move, fixed_block_move)

with open(op_path, "w", encoding="utf-8") as f:
    f.write(text)

print("Fixed!")
