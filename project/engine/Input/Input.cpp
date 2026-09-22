#include "Input.h"
#include <fstream>

namespace {
	constexpr SHORT kControllerStickDeadZone = 12000;
	constexpr BYTE kControllerTriggerThreshold = 50;

	size_t ToIndex(PlayerAction action) {
		return static_cast<size_t>(action);
	}
}


Input *Input::GetInstance() {
	static Input instance;
	return &instance;
}

void Input::Initialize(WinApp* winApp) {

	//WinAppのインスタンスを記録
	winApp_ = winApp;

	HRESULT result;



	//DirectInputの初期化
	result = DirectInput8Create(
		winApp->GetHinstance(), DIRECTINPUT_VERSION, IID_IDirectInput8,
		(void **)&directInput, nullptr);
	assert(SUCCEEDED(result));

	//キーボードデバイスの生成
	result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	assert(SUCCEEDED(result));

	//入力データの形式のセット
	result = keyboard->SetDataFormat(&c_dfDIKeyboard);//標準形式
	assert(SUCCEEDED(result));

	//排他制御レベルのセット
	result = keyboard->SetCooperativeLevel(winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(result));

	// ===========================
	// マウスデバイスの初期化
	// ===========================
	result = directInput->CreateDevice(GUID_SysMouse, &mouse_, NULL);
	if (SUCCEEDED(result)) {
		result = mouse_->SetDataFormat(&c_dfDIMouse);        // 標準マウス形式
		if (SUCCEEDED(result)) {
			// DISCL_NONEXCLUSIVE: 他のアプリとマウスを共有する（ImGuiと共存できる）
			result = mouse_->SetCooperativeLevel(winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
		}
	}

	ResetPlayerActionBindings();
	LoadPlayerActionBindings();
}

void Input::Update() {

	//前回のキー入力を保持
	memcpy(keyPre, key, sizeof(key));

	//キーボード情報の取得開始
	keyboard->Acquire();
	//全キーの入力情報を取得する
	keyboard->GetDeviceState(sizeof(key), key);

	controllerStatePre_ = controllerState_;
	ZeroMemory(&controllerState_, sizeof(controllerState_));
	isControllerConnected_ = XInputGetState(0, &controllerState_) == ERROR_SUCCESS;
	if (!isControllerConnected_) {
		ZeroMemory(&controllerStatePre_, sizeof(controllerStatePre_));
	}

	// ===========================
	// マウス入力の更新
	// ===========================
	if (mouse_) {
		// 前フレームの状態を保存
		mouseStatePre_ = mouseState_;

		// マウス情報の取得
		mouse_->Acquire();
		HRESULT hr = mouse_->GetDeviceState(sizeof(DIMOUSESTATE), &mouseState_);
		if (FAILED(hr)) {
			// 取得失敗（フォーカス喪失など）は差分をゼロにする
			mouseState_.lX = 0;
			mouseState_.lY = 0;
			mouseState_.lZ = 0;
		}
	}
	ApplyMouseCursorClip();

}

bool Input::PushKey(BYTE keyNumber) {

	//指定キーを押していればtrueを返す
	if (key[keyNumber]) {
		return true;
	}

	return false;
}

bool Input::TriggerKey(BYTE keyNumber) {

	//前回のキー入力を保持
	if (!keyPre[keyNumber] && key[keyNumber]) {
		return true;
	}


	return false;
}

bool Input::PushAction(PlayerAction action) const {
	const PlayerActionBinding &binding = GetActionBinding(action);
	return (binding.keyboardKey != 0 && key[binding.keyboardKey] != 0) ||
		IsControllerInputPressed(binding.controllerInput, false);
}

bool Input::TriggerAction(PlayerAction action) const {
	const PlayerActionBinding &binding = GetActionBinding(action);
	const bool keyboardTriggered = binding.keyboardKey != 0 &&
		keyPre[binding.keyboardKey] == 0 && key[binding.keyboardKey] != 0;
	return keyboardTriggered ||
		(IsControllerInputPressed(binding.controllerInput, false) &&
			!IsControllerInputPressed(binding.controllerInput, true));
}

const PlayerActionBinding& Input::GetActionBinding(PlayerAction action) const {
	return playerActionBindings_[ToIndex(action)];
}

void Input::SetKeyboardBinding(PlayerAction action, BYTE keyNumber) {
	playerActionBindings_[ToIndex(action)].keyboardKey = keyNumber;
}

void Input::SetControllerBinding(PlayerAction action, ControllerInput input) {
	playerActionBindings_[ToIndex(action)].controllerInput = input;
}

void Input::ResetPlayerActionBindings() {
	for (PlayerActionBinding &binding : playerActionBindings_) {
		binding = {};
	}
	auto bind = [this](PlayerAction action, BYTE keyNumber, ControllerInput controllerInput) {
		playerActionBindings_[ToIndex(action)] = { keyNumber, controllerInput };
	};
	bind(PlayerAction::MoveForward, DIK_W, ControllerInput::LeftStickUp);
	bind(PlayerAction::MoveBackward, DIK_S, ControllerInput::LeftStickDown);
	bind(PlayerAction::MoveUp, DIK_SPACE, ControllerInput::A);
	bind(PlayerAction::MoveDown, DIK_LSHIFT, ControllerInput::B);
	bind(PlayerAction::TurnLeft, DIK_LEFT, ControllerInput::LeftStickLeft);
	bind(PlayerAction::TurnRight, DIK_RIGHT, ControllerInput::LeftStickRight);
	bind(PlayerAction::PitchUp, DIK_UP, ControllerInput::RightStickUp);
	bind(PlayerAction::PitchDown, DIK_DOWN, ControllerInput::RightStickDown);
	bind(PlayerAction::RollLeft, DIK_Q, ControllerInput::LeftBumper);
	bind(PlayerAction::RollRight, DIK_E, ControllerInput::RightBumper);
	bind(PlayerAction::Guard, DIK_B, ControllerInput::X);
	bind(PlayerAction::TransformFighter, DIK_1, ControllerInput::DPadUp);
	bind(PlayerAction::TransformGerwalk, DIK_2, ControllerInput::DPadRight);
	bind(PlayerAction::TransformBattroid, DIK_3, ControllerInput::DPadDown);
	bind(PlayerAction::DodgeLeft, DIK_A, ControllerInput::DPadLeft);
	bind(PlayerAction::DodgeRight, DIK_D, ControllerInput::DPadRight);
	bind(PlayerAction::Melee, DIK_V, ControllerInput::Y);
	bind(PlayerAction::NormalFire, 0, ControllerInput::RightTrigger);
	bind(PlayerAction::HomingFire, 0, ControllerInput::LeftTrigger);
	bind(PlayerAction::LockToggle, DIK_TAB, ControllerInput::RightThumb);
	bind(PlayerAction::LockRelease, DIK_X, ControllerInput::LeftThumb);
	bind(PlayerAction::ReloadNormal, DIK_F, ControllerInput::None);
	bind(PlayerAction::ReloadHoming, DIK_G, ControllerInput::None);
	bind(PlayerAction::SpecialAttack, DIK_C, ControllerInput::Back);
	bind(PlayerAction::Song, DIK_V, ControllerInput::Start);
}

void Input::SavePlayerActionBindings() const {
	std::ofstream output("resources/input_bindings.cfg", std::ios::trunc);
	if (!output.is_open()) {
		return;
	}
	for (size_t index = 0; index < playerActionBindings_.size(); ++index) {
		const PlayerActionBinding &binding = playerActionBindings_[index];
		output << index << ' ' << static_cast<int>(binding.keyboardKey) << ' '
			<< static_cast<int>(binding.controllerInput) << '\n';
	}
}

void Input::LoadPlayerActionBindings() {
	std::ifstream input("resources/input_bindings.cfg");
	if (!input.is_open()) {
		return;
	}
	int actionIndex = 0;
	int keyNumber = 0;
	int controllerInput = 0;
	while (input >> actionIndex >> keyNumber >> controllerInput) {
		if (actionIndex < 0 || actionIndex >= static_cast<int>(PlayerAction::Count) ||
			keyNumber < 0 || keyNumber > 255 ||
			controllerInput < 0 || controllerInput >= static_cast<int>(ControllerInput::Count)) {
			continue;
		}
		playerActionBindings_[static_cast<size_t>(actionIndex)] = {
			static_cast<BYTE>(keyNumber), static_cast<ControllerInput>(controllerInput)
		};
	}
}

bool Input::IsControllerInputPressed(ControllerInput input, bool previousState) const {
	if (input == ControllerInput::None || (!isControllerConnected_ && !previousState)) {
		return false;
	}
	const XINPUT_GAMEPAD &gamepad = (previousState ? controllerStatePre_ : controllerState_).Gamepad;
	switch (input) {
	case ControllerInput::A: return (gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
	case ControllerInput::B: return (gamepad.wButtons & XINPUT_GAMEPAD_B) != 0;
	case ControllerInput::X: return (gamepad.wButtons & XINPUT_GAMEPAD_X) != 0;
	case ControllerInput::Y: return (gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0;
	case ControllerInput::LeftBumper: return (gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
	case ControllerInput::RightBumper: return (gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
	case ControllerInput::Back: return (gamepad.wButtons & XINPUT_GAMEPAD_BACK) != 0;
	case ControllerInput::Start: return (gamepad.wButtons & XINPUT_GAMEPAD_START) != 0;
	case ControllerInput::LeftThumb: return (gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
	case ControllerInput::RightThumb: return (gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
	case ControllerInput::DPadUp: return (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
	case ControllerInput::DPadDown: return (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
	case ControllerInput::DPadLeft: return (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
	case ControllerInput::DPadRight: return (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
	case ControllerInput::LeftTrigger: return gamepad.bLeftTrigger >= kControllerTriggerThreshold;
	case ControllerInput::RightTrigger: return gamepad.bRightTrigger >= kControllerTriggerThreshold;
	case ControllerInput::LeftStickUp: return gamepad.sThumbLY >= kControllerStickDeadZone;
	case ControllerInput::LeftStickDown: return gamepad.sThumbLY <= -kControllerStickDeadZone;
	case ControllerInput::LeftStickLeft: return gamepad.sThumbLX <= -kControllerStickDeadZone;
	case ControllerInput::LeftStickRight: return gamepad.sThumbLX >= kControllerStickDeadZone;
	case ControllerInput::RightStickUp: return gamepad.sThumbRY >= kControllerStickDeadZone;
	case ControllerInput::RightStickDown: return gamepad.sThumbRY <= -kControllerStickDeadZone;
	case ControllerInput::RightStickLeft: return gamepad.sThumbRX <= -kControllerStickDeadZone;
	case ControllerInput::RightStickRight: return gamepad.sThumbRX >= kControllerStickDeadZone;
	default: return false;
	}
}

const char* Input::GetPlayerActionName(PlayerAction action) {
	static constexpr const char *names[] = {
		"前進 / 加速", "後退 / 減速", "上昇", "下降", "左旋回", "右旋回",
		"上向き", "下向き", "左ロール", "右ロール", "ガード",
		"ファイター形態", "ガウォーク形態", "バトロイド形態", "左回避", "右回避",
		"近接攻撃", "通常射撃", "ホーミング射撃", "ロックオン切替", "ロックオン解除",
		"通常弾リロード", "誘導弾リロード", "SP攻撃", "歌"
	};
	const size_t index = ToIndex(action);
	return index < std::size(names) ? names[index] : "不明な操作";
}

const char* Input::GetControllerInputName(ControllerInput input) {
	static constexpr const char *names[] = {
		"なし", "A", "B", "X", "Y", "LB", "RB", "Back", "Start", "Lスティック押込", "Rスティック押込",
		"十字↑", "十字↓", "十字←", "十字→", "LT", "RT",
		"Lスティック↑", "Lスティック↓", "Lスティック←", "Lスティック→",
		"Rスティック↑", "Rスティック↓", "Rスティック←", "Rスティック→"
	};
	const size_t index = static_cast<size_t>(input);
	return index < std::size(names) ? names[index] : "なし";
}

std::string Input::GetKeyboardKeyName(BYTE keyNumber) {
	switch (keyNumber) {
	case 0: return "なし";
	case DIK_W: return "W"; case DIK_A: return "A"; case DIK_S: return "S"; case DIK_D: return "D";
	case DIK_Q: return "Q"; case DIK_E: return "E"; case DIK_B: return "B"; case DIK_C: return "C";
	case DIK_F: return "F"; case DIK_G: return "G"; case DIK_V: return "V"; case DIK_X: return "X";
	case DIK_1: return "1"; case DIK_2: return "2"; case DIK_3: return "3";
	case DIK_SPACE: return "Space"; case DIK_LSHIFT: return "Left Shift";
	case DIK_UP: return "↑"; case DIK_DOWN: return "↓"; case DIK_LEFT: return "←"; case DIK_RIGHT: return "→";
	case DIK_TAB: return "Tab";
	default: return "Key " + std::to_string(static_cast<int>(keyNumber));
	}
}

bool Input::PushMouseButton(int button) const {
	if (button < 0 || button > 3) return false;
	return (mouseState_.rgbButtons[button] & 0x80) != 0;
}

bool Input::TriggerMouseButton(int button) const {
	if (button < 0 || button > 3) return false;
	return ((mouseState_.rgbButtons[button] & 0x80) != 0) &&
	       ((mouseStatePre_.rgbButtons[button] & 0x80) == 0);
}

void Input::SetMouseCursorClipEnabled(bool enabled) {
	if (isMouseCursorClipEnabled_ == enabled) {
		return;
	}

	isMouseCursorClipEnabled_ = enabled;
	ApplyMouseCursorClip();
}

void Input::SetMouseCursorClipRect(float minX, float minY, float maxX, float maxY) {
	RECT rect{
		static_cast<LONG>(minX),
		static_cast<LONG>(minY),
		static_cast<LONG>(maxX),
		static_cast<LONG>(maxY)
	};

	hasMouseCursorClipRect_ = rect.right > rect.left && rect.bottom > rect.top;
	if (hasMouseCursorClipRect_) {
		mouseCursorClipRect_ = rect;
	}
	ApplyMouseCursorClip();
}

void Input::ClearMouseCursorClipRect() {
	hasMouseCursorClipRect_ = false;
	ApplyMouseCursorClip();
}

void Input::ApplyMouseCursorClip() {
	if (!winApp_) {
		return;
	}

	const HWND hwnd = winApp_->GetHwnd();
	const bool canClip =
		isMouseCursorClipEnabled_ &&
		hwnd != nullptr &&
		GetForegroundWindow() == hwnd &&
		!IsIconic(hwnd);

	if (!canClip) {
		if (isMouseCursorClipped_) {
			ClipCursor(nullptr);
			isMouseCursorClipped_ = false;
		}
		return;
	}

	RECT clipRect{};
	if (hasMouseCursorClipRect_) {
		clipRect = mouseCursorClipRect_;
	} else if (!GetClientMouseCursorClipRect(clipRect)) {
		if (isMouseCursorClipped_) {
			ClipCursor(nullptr);
			isMouseCursorClipped_ = false;
		}
		return;
	}

	if (clipRect.right <= clipRect.left || clipRect.bottom <= clipRect.top) {
		return;
	}

	if (ClipCursor(&clipRect)) {
		isMouseCursorClipped_ = true;
	}
}

bool Input::GetClientMouseCursorClipRect(RECT& rect) const {
	if (!winApp_ || !winApp_->GetHwnd()) {
		return false;
	}

	RECT clientRect{};
	if (!GetClientRect(winApp_->GetHwnd(), &clientRect)) {
		return false;
	}

	POINT leftTop{ clientRect.left, clientRect.top };
	POINT rightBottom{ clientRect.right, clientRect.bottom };
	if (!ClientToScreen(winApp_->GetHwnd(), &leftTop) ||
		!ClientToScreen(winApp_->GetHwnd(), &rightBottom)) {
		return false;
	}

	rect = { leftTop.x, leftTop.y, rightBottom.x, rightBottom.y };
	return rect.right > rect.left && rect.bottom > rect.top;
}
