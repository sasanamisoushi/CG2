#include "Input.h"


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
}

void Input::Update() {

	//前回のキー入力を保持
	memcpy(keyPre, key, sizeof(key));

	//キーボード情報の取得開始
	keyboard->Acquire();
	//全キーの入力情報を取得する
	keyboard->GetDeviceState(sizeof(key), key);

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
